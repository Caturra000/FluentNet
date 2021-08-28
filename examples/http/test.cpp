#include <bits/stdc++.h>
#include "Server.h"

static const char html[] = R"(
<html>

  <head>
    <title>Welcome!</title>
  </head>

  <body>
    <h1>Hello</h1>
  </body>

</html>
)";

int main() {
    fluent::InetAddress address {INADDR_ANY, 2433};
    fluent::http::Server server(address);
    auto routine = [](const fluent::http::Request *request, fluent::http::Response *response) {
        if(request->path == "/") {
            response->code = fluent::http::Response::Code::_200OK;
            response->message = "OK";
            response->body = html;
        } else {
            response->code = fluent::http::Response::Code::_404NotFound;
            response->message = "Not Found";
            response->closeFlag = true;
        }
    };
    server.onRequest(routine);
    server.ready();
    server.run();
}