#include "robot_remote/XmlRpcServer.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace robot_remote {

namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

struct XmlNode {
    std::string name;
    std::string text;
    std::vector<XmlNode> children;
};

class XmlParser {
public:
    explicit XmlParser(std::string_view input) : input_(input) {}

    XmlNode parse() {
        skip_ws();
        if (starts_with("<?")) {
            skip_until("?>");
        }
        skip_ws();
        if (starts_with("<!--")) {
            skip_until("-->");
        }
        skip_ws();
        return parse_node();
    }

private:
    XmlNode parse_node() {
        skip_ws();
        if (!consume('<')) {
            throw std::runtime_error("Expected '<'");
        }
        if (starts_with("?")) {
            skip_until("?>");
            return parse_node();
        }
        if (starts_with("!--")) {
            skip_until("-->");
            return parse_node();
        }

        std::string name = parse_name();
        skip_attributes();
        consume('>');

        XmlNode node;
        node.name = std::move(name);

        while (true) {
            skip_ws();
            if (starts_with("</")) {
                consume('<');
                consume('/');
                std::string end_name = parse_name();
                skip_attributes();
                consume('>');
                if (end_name != node.name) {
                    throw std::runtime_error("Mismatched closing tag");
                }
                break;
            }
            if (peek() == '<') {
                node.children.push_back(parse_node());
                continue;
            }
            node.text += parse_text();
        }

        return node;
    }

    std::string parse_text() {
        std::string out;
        while (pos_ < input_.size() && input_[pos_] != '<') {
            out.push_back(input_[pos_]);
            ++pos_;
        }
        return out;
    }

    std::string parse_name() {
        std::string out;
        while (pos_ < input_.size()) {
            char ch = input_[pos_];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == ':' || ch == '-') {
                out.push_back(ch);
                ++pos_;
                continue;
            }
            break;
        }
        return out;
    }

    void skip_attributes() {
        bool in_quote = false;
        char quote_char = '\0';
        while (pos_ < input_.size()) {
            char ch = input_[pos_];
            if (!in_quote && ch == '>') {
                return;
            }
            if (ch == '"' || ch == '\'') {
                if (!in_quote) {
                    in_quote = true;
                    quote_char = ch;
                } else if (quote_char == ch) {
                    in_quote = false;
                }
            }
            ++pos_;
        }
    }

    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    void skip_until(std::string_view token) {
        auto idx = input_.find(token, pos_);
        if (idx == std::string_view::npos) {
            pos_ = input_.size();
            return;
        }
        pos_ = idx + token.size();
    }

    bool starts_with(std::string_view token) const {
        return input_.substr(pos_, token.size()) == token;
    }

    char peek() const {
        if (pos_ >= input_.size()) {
            return '\0';
        }
        return input_[pos_];
    }

    bool consume(char ch) {
        if (pos_ >= input_.size() || input_[pos_] != ch) {
            return false;
        }
        ++pos_;
        return true;
    }

    std::string_view input_;
    size_t pos_{0};
};

const XmlNode *find_child(const XmlNode &node, const std::string &name) {
    for (const auto &child : node.children) {
        if (child.name == name) {
            return &child;
        }
    }
    return nullptr;
}

std::vector<const XmlNode *> find_children(const XmlNode &node, const std::string &name) {
    std::vector<const XmlNode *> out;
    for (const auto &child : node.children) {
        if (child.name == name) {
            out.push_back(&child);
        }
    }
    return out;
}

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

XmlRpcValue parse_value(const XmlNode &value_node) {
    if (value_node.children.empty()) {
        return XmlRpcValue(trim(value_node.text));
    }

    const XmlNode &type_node = value_node.children.front();
    if (type_node.name == "string") {
        return XmlRpcValue(trim(type_node.text));
    }
    if (type_node.name == "int" || type_node.name == "i4") {
        return XmlRpcValue(std::stoi(trim(type_node.text)));
    }
    if (type_node.name == "double") {
        return XmlRpcValue(std::stod(trim(type_node.text)));
    }
    if (type_node.name == "boolean") {
        std::string val = trim(type_node.text);
        return XmlRpcValue(val == "1" || val == "true" || val == "True");
    }
    if (type_node.name == "array") {
        XmlRpcArray arr;
        const XmlNode *data_node = find_child(type_node, "data");
        if (data_node) {
            for (const auto *child : find_children(*data_node, "value")) {
                arr.push_back(parse_value(*child));
            }
        }
        return XmlRpcValue(std::move(arr));
    }
    if (type_node.name == "struct") {
        XmlRpcStruct st;
        for (const auto *member : find_children(type_node, "member")) {
            const XmlNode *name_node = find_child(*member, "name");
            const XmlNode *value_node_child = find_child(*member, "value");
            if (!name_node || !value_node_child) {
                continue;
            }
            st[trim(name_node->text)] = parse_value(*value_node_child);
        }
        return XmlRpcValue(std::move(st));
    }

    return XmlRpcValue(trim(type_node.text));
}

std::string xml_escape(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string serialize_value(const XmlRpcValue &value) {
    std::ostringstream out;
    out << "<value>";
    if (std::holds_alternative<std::monostate>(value.value)) {
        out << "<string></string>";
    } else if (std::holds_alternative<int>(value.value)) {
        out << "<int>" << std::get<int>(value.value) << "</int>";
    } else if (std::holds_alternative<double>(value.value)) {
        out.setf(std::ios::fixed);
        out.precision(12);
        out << "<double>" << std::get<double>(value.value) << "</double>";
    } else if (std::holds_alternative<bool>(value.value)) {
        out << "<boolean>" << (std::get<bool>(value.value) ? "1" : "0") << "</boolean>";
    } else if (std::holds_alternative<std::string>(value.value)) {
        out << "<string>" << xml_escape(std::get<std::string>(value.value)) << "</string>";
    } else if (std::holds_alternative<XmlRpcArray>(value.value)) {
        out << "<array><data>";
        for (const auto &item : std::get<XmlRpcArray>(value.value)) {
            out << serialize_value(item);
        }
        out << "</data></array>";
    } else if (std::holds_alternative<XmlRpcStruct>(value.value)) {
        out << "<struct>";
        for (const auto &pair : std::get<XmlRpcStruct>(value.value)) {
            out << "<member><name>" << xml_escape(pair.first) << "</name>";
            out << serialize_value(pair.second) << "</member>";
        }
        out << "</struct>";
    }
    out << "</value>";
    return out.str();
}

std::string make_method_response(const XmlRpcValue &value) {
    std::ostringstream out;
    out << "<?xml version=\"1.0\"?>";
    out << "<methodResponse><params><param>";
    out << serialize_value(value);
    out << "</param></params></methodResponse>";
    return out.str();
}

std::string make_fault_response(int code, const std::string &message) {
    XmlRpcStruct st;
    st["faultCode"] = XmlRpcValue(code);
    st["faultString"] = XmlRpcValue(message);
    std::ostringstream out;
    out << "<?xml version=\"1.0\"?>";
    out << "<methodResponse><fault>" << serialize_value(XmlRpcValue(st)) << "</fault></methodResponse>";
    return out.str();
}

std::optional<int> parse_content_length(const std::string &headers) {
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        if (key == "content-length") {
            return std::stoi(trim(value));
        }
    }
    return std::nullopt;
}

std::string read_exact(int fd, size_t bytes) {
    std::string out;
    out.resize(bytes);
    size_t offset = 0;
    while (offset < bytes) {
        ssize_t read_now = ::recv(fd, out.data() + offset, bytes - offset, 0);
        if (read_now <= 0) {
            throw std::runtime_error("Connection closed");
        }
        offset += static_cast<size_t>(read_now);
    }
    return out;
}

std::optional<HttpRequest> read_request(int fd) {
    std::string buffer;
    char chunk[4096];
    while (buffer.find("\r\n\r\n") == std::string::npos) {
        ssize_t read_now = ::recv(fd, chunk, sizeof(chunk), 0);
        if (read_now <= 0) {
            return std::nullopt;
        }
        buffer.append(chunk, static_cast<size_t>(read_now));
        if (buffer.size() > 1024 * 1024) {
            throw std::runtime_error("Header too large");
        }
    }

    size_t header_end = buffer.find("\r\n\r\n");
    std::string headers = buffer.substr(0, header_end + 4);
    std::istringstream header_stream(headers);
    std::string request_line;
    if (!std::getline(header_stream, request_line)) {
        throw std::runtime_error("Missing request line");
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream request_line_stream(request_line);
    HttpRequest request;
    std::string http_version;
    if (!(request_line_stream >> request.method >> request.path >> http_version)) {
        throw std::runtime_error("Invalid request line");
    }

    std::optional<int> content_length = parse_content_length(headers);
    request.body = buffer.substr(header_end + 4);

    if (content_length) {
        if (request.body.size() < static_cast<size_t>(*content_length)) {
            request.body += read_exact(fd, static_cast<size_t>(*content_length) - request.body.size());
        } else if (request.body.size() > static_cast<size_t>(*content_length)) {
            request.body.resize(static_cast<size_t>(*content_length));
        }
    }

    return request;
}

void send_response(int fd, const std::string &status_line, const std::string &content_type, const std::string &body) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status_line << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    std::string payload = out.str();
    size_t offset = 0;
    while (offset < payload.size()) {
        ssize_t wrote = ::send(fd, payload.data() + offset, payload.size() - offset, 0);
        if (wrote <= 0) {
            break;
        }
        offset += static_cast<size_t>(wrote);
    }
}

void send_xml_response(int fd, const std::string &body) {
    send_response(fd, "200 OK", "text/xml", body);
}

void send_text_response(int fd, const std::string &status_line, const std::string &body) {
    send_response(fd, status_line, "text/plain; charset=utf-8", body);
}

void send_html_response(int fd, const std::string &body) {
    send_response(fd, "200 OK", "text/html; charset=utf-8", body);
}

}  // namespace

XmlRpcServer::XmlRpcServer(int port, MethodHandler handler, HttpPageHandler http_page_handler)
    : port_(port), handler_(std::move(handler)), http_page_handler_(std::move(http_page_handler)) {}

XmlRpcServer::~XmlRpcServer() {
    stop();
}

bool XmlRpcServer::start() {
    if (running_) {
        return false;
    }
    running_ = true;
    thread_ = std::thread(&XmlRpcServer::run, this);
    return true;
}

void XmlRpcServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool XmlRpcServer::is_running() const {
    return running_;
}

void XmlRpcServer::run() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        running_ = false;
        return;
    }

    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        running_ = false;
        return;
    }

    if (::listen(server_fd_, 8) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        running_ = false;
        return;
    }

    while (running_) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(server_fd_, &set);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        int ready = ::select(server_fd_ + 1, &set, nullptr, nullptr, &timeout);
        if (ready <= 0 || !FD_ISSET(server_fd_, &set)) {
            continue;
        }

        int client_fd = ::accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            continue;
        }

        try {
            std::optional<HttpRequest> request = read_request(client_fd);
            if (!request) {
                ::close(client_fd);
                continue;
            }

#ifdef ROBOT_REMOTE_ENABLE_HTTP_SERVER
            if (request->method == "GET") {
                if ((request->path == "/" || request->path == "/index.html") && http_page_handler_) {
                    send_html_response(client_fd, http_page_handler_());
                } else {
                    send_text_response(client_fd, "404 Not Found", "Not Found");
                }
                ::close(client_fd);
                continue;
            }
#endif

            if (request->method != "POST") {
                send_text_response(client_fd, "405 Method Not Allowed", "Only POST is supported for XML-RPC");
                ::close(client_fd);
                continue;
            }

            if (request->body.empty()) {
                send_response(client_fd, "400 Bad Request", "text/xml", make_fault_response(400, "Missing request body"));
                ::close(client_fd);
                continue;
            }

            XmlParser parser(request->body);
            XmlNode root = parser.parse();
            const XmlNode *method_name_node = find_child(root, "methodName");
            if (!method_name_node) {
                send_xml_response(client_fd, make_fault_response(400, "Missing methodName"));
                ::close(client_fd);
                continue;
            }

            std::string method_name = trim(method_name_node->text);
            std::vector<XmlRpcValue> params;
            const XmlNode *params_node = find_child(root, "params");
            if (params_node) {
                for (const auto *param : find_children(*params_node, "param")) {
                    const XmlNode *value_node = find_child(*param, "value");
                    if (value_node) {
                        params.push_back(parse_value(*value_node));
                    }
                }
            }

            XmlRpcValue result = handler_(method_name, params);
            send_xml_response(client_fd, make_method_response(result));
        } catch (const std::exception &ex) {
            send_xml_response(client_fd, make_fault_response(500, ex.what()));
        }

        ::close(client_fd);
    }
}

}  // namespace robot_remote
