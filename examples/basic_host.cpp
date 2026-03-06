#include <iostream>

#include "robot_remote/RemoteServer.h"
#include <filesystem>

using robot_remote::KeywordResult;
using robot_remote::KeywordSpec;
using robot_remote::RemoteServer;
using robot_remote::XmlRpcArray;
using robot_remote::XmlRpcStruct;
using robot_remote::XmlRpcValue;

static constexpr int SERVER_PORT = 8270;

int main() {
  RemoteServer server(SERVER_PORT);

  KeywordSpec count_items_in_directory;
  count_items_in_directory.name = "Count Items In Directory";
  count_items_in_directory.documentation =
      "Returns the number of items in the directory specified by `path`.";
  count_items_in_directory.argument_spec = {"path"};
  count_items_in_directory.handler =
      [](const std::vector<XmlRpcValue> &args,
         const std::map<std::string, XmlRpcValue> &) {
        KeywordResult result;
        if (!args.empty() &&
            std::holds_alternative<std::string>(args[0].value)) {
          std::string path = std::get<std::string>(args[0].value);
          try {
            result.return_value = static_cast<int>(
                std::distance(std::filesystem::directory_iterator(path),
                              std::filesystem::directory_iterator{}));
          } catch (const std::filesystem::filesystem_error &e) {
            result.return_value =
                0; // Return 0 if the directory cannot be accessed
          }
        }
        return result;
      };

  KeywordSpec strings_should_be_equal;
  strings_should_be_equal.name = "Strings Should Be Equal";
  strings_should_be_equal.documentation =
      "Asserts that two strings are equal. Returns an error message if they "
      "are not.";
  strings_should_be_equal.argument_spec = {"string1", "string2"};
  strings_should_be_equal.handler =
      [](const std::vector<XmlRpcValue> &args,
         const std::map<std::string, XmlRpcValue> &) {
        KeywordResult result;
        if (args.size() >= 2 &&
            std::holds_alternative<std::string>(args[0].value) &&
            std::holds_alternative<std::string>(args[1].value)) {
          std::string string1 = std::get<std::string>(args[0].value);
          std::string string2 = std::get<std::string>(args[1].value);
          if (string1 != string2) {
            result.success = false;
            result.error =
                "Strings are not equal: '" + string1 + "' != '" + string2 + "'";
          }
        } else {
          result.success = false;
          result.error = "Invalid arguments. Expected two strings.";
        }
        return result;
      };

  KeywordSpec echo_keyword;
  echo_keyword.name = "Echo";
  echo_keyword.documentation = "Returns the first argument.";
  echo_keyword.argument_spec = {"text"};
  echo_keyword.handler = [](const std::vector<XmlRpcValue> &args,
                            const std::map<std::string, XmlRpcValue> &) {
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
  add_keyword.handler = [](const std::vector<XmlRpcValue> &args,
                           const std::map<std::string, XmlRpcValue> &) {
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

  server.set_keywords({count_items_in_directory, strings_should_be_equal,
                       echo_keyword, add_keyword});

  robot_remote::LibraryInfo info;
  info.name = "ExampleLibrary";
  info.version = "1.0";
  info.documentation = "Small demo library for Robot Framework Remote.";
  server.set_library_info(info);

  if (!server.start()) {
    std::cerr << "Failed to start XML-RPC server" << std::endl;
    return 1;
  }

  std::cout << "Robot Remote server listening on port " << SERVER_PORT
            << ". Press Enter to stop." << std::endl;
  std::cin.get();

  server.stop();
  return 0;
}
