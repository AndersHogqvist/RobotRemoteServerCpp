#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace robot_remote {

/// Container for XML-RPC values used by the remote server interface.
struct XmlRpcValue;

using XmlRpcArray = std::vector<XmlRpcValue>;
using XmlRpcStruct = std::map<std::string, XmlRpcValue>;

/// Variant value for XML-RPC serialization and deserialization.
struct XmlRpcValue {
  using Variant = std::variant<std::monostate, int, double, bool, std::string,
                               XmlRpcArray, XmlRpcStruct>;

  Variant value;

  XmlRpcValue() = default;
  XmlRpcValue(int v) : value(v) {}
  XmlRpcValue(double v) : value(v) {}
  XmlRpcValue(bool v) : value(v) {}
  XmlRpcValue(const char *v) : value(std::string(v)) {}
  XmlRpcValue(std::string v) : value(std::move(v)) {}
  XmlRpcValue(XmlRpcArray v) : value(std::move(v)) {}
  XmlRpcValue(XmlRpcStruct v) : value(std::move(v)) {}
};

} // namespace robot_remote
