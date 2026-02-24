#include "robot_remote/RemoteServer.h"

#include <sstream>
#include <stdexcept>

namespace robot_remote {

namespace {

XmlRpcStruct make_failure(const std::string &message, const std::string &traceback) {
    XmlRpcStruct st;
    st["status"] = XmlRpcValue("FAIL");
    st["return"] = XmlRpcValue();
    st["error"] = XmlRpcValue(message);
    st["traceback"] = XmlRpcValue(traceback);
    return st;
}

XmlRpcStruct make_success(const XmlRpcValue &value, const std::string &output) {
    XmlRpcStruct st;
    st["status"] = XmlRpcValue("PASS");
    st["return"] = value;
    st["error"] = XmlRpcValue("");
    st["traceback"] = XmlRpcValue("");
    if (!output.empty()) {
        st["output"] = XmlRpcValue(output);
    }
    return st;
}

const std::string &as_string(const XmlRpcValue &value) {
    if (!std::holds_alternative<std::string>(value.value)) {
        throw std::runtime_error("Expected string parameter");
    }
    return std::get<std::string>(value.value);
}

std::string html_escape(const std::string &value) {
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
            out += "&#39;";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

}  // namespace

RemoteServer::RemoteServer(int port) : port_(port) {}

void RemoteServer::set_keywords(const std::vector<KeywordSpec> &keywords) {
    keywords_.clear();
    for (const auto &keyword : keywords) {
        keywords_[keyword.name] = keyword;
    }
}

void RemoteServer::set_library_info(const LibraryInfo &info) {
    library_info_ = info;
}

bool RemoteServer::start() {
    if (server_) {
        return false;
    }
    server_ = std::make_unique<XmlRpcServer>(port_, [this](const std::string &method, const std::vector<XmlRpcValue> &params) {
        return handle_call(method, params);
    }, [this]() { return build_keywords_page(); });
    return server_->start();
}

void RemoteServer::stop() {
    if (!server_) {
        return;
    }
    server_->stop();
    server_.reset();
}

bool RemoteServer::is_running() const {
    return server_ && server_->is_running();
}

std::string RemoteServer::build_keywords_page() const {
    std::ostringstream out;
    out << "<!doctype html><html><head><meta charset=\"utf-8\">";
    out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    out << "<title>" << html_escape(library_info_.name.empty() ? "Robot Remote Server" : library_info_.name) << "</title>";
    out << "<style>body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;max-width:900px;margin:2rem auto;padding:0 1rem;line-height:1.5;}";
    out << "h1,h2{margin-bottom:.4rem;} .kw{border:1px solid #d7dbe0;border-radius:8px;padding:1rem;margin:1rem 0;}";
    out << "ul{margin:.4rem 0 0 1.2rem;}code{background:#f4f6f8;padding:.1rem .35rem;border-radius:4px;}</style></head><body>";

    out << "<h1>" << html_escape(library_info_.name.empty() ? "Robot Remote Library" : library_info_.name) << "</h1>";
    if (!library_info_.version.empty()) {
        out << "<p><strong>Version:</strong> " << html_escape(library_info_.version) << "</p>";
    }
    if (!library_info_.documentation.empty()) {
        out << "<p>" << html_escape(library_info_.documentation) << "</p>";
    }

    out << "<h2>Available keywords</h2>";
    if (keywords_.empty()) {
        out << "<p>No keywords registered.</p>";
    }

    for (const auto &pair : keywords_) {
        const KeywordSpec &keyword = pair.second;
        out << "<section class=\"kw\">";
        out << "<h3>" << html_escape(keyword.name) << "</h3>";
        out << "<p>" << html_escape(keyword.documentation.empty() ? "(no description)" : keyword.documentation) << "</p>";
        out << "<p><strong>Arguments</strong></p>";
        if (keyword.argument_spec.empty()) {
            out << "<p><em>None</em></p>";
        } else {
            out << "<ul>";
            for (const auto &argument : keyword.argument_spec) {
                out << "<li><code>" << html_escape(argument) << "</code></li>";
            }
            out << "</ul>";
        }
        out << "</section>";
    }

    out << "</body></html>";
    return out.str();
}

XmlRpcValue RemoteServer::handle_call(const std::string &method, const std::vector<XmlRpcValue> &params) {
    if (method == "get_keyword_names") {
        XmlRpcArray names;
        for (const auto &pair : keywords_) {
            names.emplace_back(pair.first);
        }
        return XmlRpcValue(std::move(names));
    }

    if (method == "get_keyword_documentation") {
        if (params.empty()) {
            return XmlRpcValue("");
        }
        auto it = keywords_.find(as_string(params[0]));
        if (it == keywords_.end()) {
            return XmlRpcValue("");
        }
        return XmlRpcValue(it->second.documentation);
    }

    if (method == "get_keyword_arguments") {
        if (params.empty()) {
            return XmlRpcValue(XmlRpcArray{});
        }
        auto it = keywords_.find(as_string(params[0]));
        if (it == keywords_.end()) {
            return XmlRpcValue(XmlRpcArray{});
        }
        XmlRpcArray args;
        for (const auto &arg : it->second.argument_spec) {
            args.emplace_back(arg);
        }
        return XmlRpcValue(std::move(args));
    }

    if (method == "get_library_information") {
        XmlRpcStruct info;
        info["name"] = XmlRpcValue(library_info_.name);
        info["version"] = XmlRpcValue(library_info_.version);
        info["doc"] = XmlRpcValue(library_info_.documentation);
        info["scope"] = XmlRpcValue(library_info_.scope);
        info["named_args"] = XmlRpcValue(library_info_.named_args);

        XmlRpcArray keywords;
        for (const auto &pair : keywords_) {
            const KeywordSpec &spec = pair.second;
            XmlRpcStruct kw;
            kw["name"] = XmlRpcValue(spec.name);
            kw["doc"] = XmlRpcValue(spec.documentation);
            XmlRpcArray args;
            for (const auto &arg : spec.argument_spec) {
                args.emplace_back(arg);
            }
            kw["args"] = XmlRpcValue(std::move(args));
            keywords.emplace_back(XmlRpcValue(std::move(kw)));
        }

        info["keywords"] = XmlRpcValue(std::move(keywords));
        return XmlRpcValue(std::move(info));
    }

    if (method == "run_keyword") {
        if (params.size() < 2) {
            return XmlRpcValue(make_failure("run_keyword expects at least 2 params", ""));
        }
        std::string name = as_string(params[0]);
        auto it = keywords_.find(name);
        if (it == keywords_.end()) {
            return XmlRpcValue(make_failure("Unknown keyword: " + name, ""));
        }

        std::vector<XmlRpcValue> args;
        std::map<std::string, XmlRpcValue> kwargs;

        if (std::holds_alternative<XmlRpcArray>(params[1].value)) {
            args = std::get<XmlRpcArray>(params[1].value);
        }
        if (params.size() >= 3 && std::holds_alternative<XmlRpcStruct>(params[2].value)) {
            kwargs = std::get<XmlRpcStruct>(params[2].value);
        }

        try {
            KeywordResult result = it->second.handler(args, kwargs);
            if (!result.success) {
                return XmlRpcValue(make_failure(result.error, result.traceback));
            }
            return XmlRpcValue(make_success(result.return_value, result.output));
        } catch (const std::exception &ex) {
            return XmlRpcValue(make_failure(ex.what(), ""));
        }
    }

    return XmlRpcValue(make_failure("Unknown method: " + method, ""));
}

}  // namespace robot_remote
