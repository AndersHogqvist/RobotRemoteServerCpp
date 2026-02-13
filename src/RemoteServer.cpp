#include "robot_remote/RemoteServer.h"

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
    });
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
