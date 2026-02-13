#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "robot_remote/XmlRpcServer.h"
#include "robot_remote/XmlRpcValue.h"

namespace robot_remote {

struct KeywordResult {
    /// Return value for a keyword call.
    XmlRpcValue return_value;
    /// Optional log output captured by the keyword.
    std::string output;
    /// Success flag for the keyword execution.
    bool success{true};
    /// Error message when success is false.
    std::string error;
    /// Optional traceback when success is false.
    std::string traceback;
};

using KeywordHandler = std::function<KeywordResult(const std::vector<XmlRpcValue> &, const std::map<std::string, XmlRpcValue> &)>;

struct KeywordSpec {
    /// Keyword name as used in Robot Framework.
    std::string name;
    /// Keyword documentation string.
    std::string documentation;
    /// Keyword argument spec list.
    std::vector<std::string> argument_spec;
    /// Callback invoked when the keyword is called.
    KeywordHandler handler;
};

/// Metadata returned by get_library_information.
struct LibraryInfo {
    /// Library name.
    std::string name;
    /// Library version string.
    std::string version;
    /// Library documentation.
    std::string documentation;
    /// Library scope, e.g. GLOBAL or TEST.
    std::string scope{"GLOBAL"};
    /// Whether the library supports named arguments.
    bool named_args{true};
};

class RemoteServer {
public:
    /// Create a RemoteServer bound to the given TCP port.
    explicit RemoteServer(int port);

    /// Register keywords for the remote library.
    void set_keywords(const std::vector<KeywordSpec> &keywords);
    /// Set library metadata for get_library_information.
    void set_library_info(const LibraryInfo &info);

    /// Start the XML-RPC server.
    bool start();
    /// Stop the XML-RPC server.
    void stop();
    /// Query whether the server is running.
    bool is_running() const;

private:
    XmlRpcValue handle_call(const std::string &method, const std::vector<XmlRpcValue> &params);

    int port_;
    LibraryInfo library_info_{};
    std::map<std::string, KeywordSpec> keywords_;
    std::unique_ptr<XmlRpcServer> server_;
};

}  // namespace robot_remote
