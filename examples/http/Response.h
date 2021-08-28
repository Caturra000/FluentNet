#ifndef __FLUENT_HTTP_RESPONSE_H__
#define __FLUENT_HTTP_RESPONSE_H__
#include <bits/stdc++.h>
namespace fluent {
namespace http {

class Response {
public:
    enum class Code: uint {
        _Unknown,
        _200OK = 200,
        _301MovedPermanently = 301,
        _400BadRequest = 400,
        _404NotFound = 404,
    };

    using HeaderList = std::map<std::string, std::string>;


    std::string stream() const;

// shared
public:
    Code code;
    std::string message;
    HeaderList headers;
    std::string body;
    bool closeFlag {false};
};

inline std::string Response::stream() const {
    std::string result = "HTTP/1.1 ";
    result.append(std::to_string(static_cast<uint>(code)))
            .append(" ")
            .append(message);
    if(closeFlag) {
        result.append("\r\nConnection: close\r\n");
    } else {
        result.append("\r\nContent-Length: ")
                .append(std::to_string(body.size()))
                .append("\r\nConnection: Keep-Alive\r\n");
    }

    for(auto &header : headers) {
        result.append(header.first)
                .append(": ")
                .append(header.second)
                .append("\r\n");
    }
    result.append("\r\n").append(body);
    return result;
}

} // http
} // fluent
#endif