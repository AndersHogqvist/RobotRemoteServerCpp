#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>

#include "robot_remote/XmlRpcValue.h"

namespace robot_remote {

class XmlRpcServer {
public:
  /// Callback for handling XML-RPC method calls.
  using MethodHandler = std::function<XmlRpcValue(
      const std::string &, const std::vector<XmlRpcValue> &)>;
  /// Callback for producing an HTTP HTML page.
  using HttpPageHandler = std::function<std::string()>;

  /// Create a server that listens on the given port.
  XmlRpcServer(int port, MethodHandler handler,
               HttpPageHandler http_page_handler = {});
  ~XmlRpcServer();

  /// Start the server loop on a background thread.
  bool start();
  /// Stop the server and join the background thread.
  void stop();
  /// Query whether the server thread is active.
  bool is_running() const;

private:
  void run();
  void serve_client(asio::ip::tcp::socket socket);

  int port_;
  MethodHandler handler_;
  HttpPageHandler http_page_handler_;
  std::atomic<bool> running_{false};
  std::thread thread_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace robot_remote
