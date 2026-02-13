#include <iostream>

#include "robot_remote/RemoteServer.h"

using robot_remote::KeywordResult;
using robot_remote::KeywordSpec;
using robot_remote::RemoteServer;
using robot_remote::XmlRpcArray;
using robot_remote::XmlRpcStruct;
using robot_remote::XmlRpcValue;

int main() {
    RemoteServer server(8270);

    KeywordSpec echo_keyword;
    echo_keyword.name = "Echo";
    echo_keyword.documentation = "Returns the first argument.";
    echo_keyword.argument_spec = {"text"};
    echo_keyword.handler = [](const std::vector<XmlRpcValue> &args, const std::map<std::string, XmlRpcValue> &) {
        KeywordResult result;
        if (!args.empty() && std::holds_alternative<std::string>(args[0].value)) {
            result.return_value = std::get<std::string>(args[0].value);
        }
        return result;
    };

    KeywordSpec add_keyword;
    add_keyword.name = "Add";
    add_keyword.documentation = "Adds numeric arguments.";
    add_keyword.argument_spec = {"*numbers"};
    add_keyword.handler = [](const std::vector<XmlRpcValue> &args, const std::map<std::string, XmlRpcValue> &) {
        double sum = 0.0;
        for (const auto &arg : args) {
            if (std::holds_alternative<int>(arg.value)) {
                sum += std::get<int>(arg.value);
            } else if (std::holds_alternative<double>(arg.value)) {
                sum += std::get<double>(arg.value);
            }
        }
        KeywordResult result;
        result.return_value = sum;
        return result;
    };

    server.set_keywords({echo_keyword, add_keyword});

    robot_remote::LibraryInfo info;
    info.name = "ExampleLibrary";
    info.version = "1.0";
    info.documentation = "Small demo library for Robot Framework Remote.";
    server.set_library_info(info);

    if (!server.start()) {
        std::cerr << "Failed to start XML-RPC server" << std::endl;
        return 1;
    }

    std::cout << "Robot Remote server listening on port 8270. Press Enter to stop." << std::endl;
    std::cin.get();

    server.stop();
    return 0;
}
