#ifndef __FLUENT_HTTP_REQUEST_H__
#define __FLUENT_HTTP_REQUEST_H__
#include <bits/stdc++.h>

namespace fluent {
namespace http {

class Request {
public:
    enum class Method {
        UNKNOWN,
        GET,
        POST,
        HEAD,
        PUT,
        DELETE
    };

    enum class Version {
        UNKNOWN,
        V10,
        V11,
    };

    // TODO local-stack linkded list for small size headers
    using HeaderList = std::map<std::string, std::string>;

public:
    Request(): method(Method::UNKNOWN), version(Version::UNKNOWN) {}

    template <typename Iter>
    bool setMethod(Iter first, Iter last);

// shared
// no setter/getter, do it yourself
public:
    Method method;
    Version version;
    std::string path;
    std::string query;
    HeaderList headers;
};

// ugly but faster than accessing static array table
template <typename Iter>
inline bool Request::setMethod(Iter first, Iter last) {
    auto difference = std::distance(first, last);
    if(difference >= 3) {
        last = std::prev(last);
        if(std::equal(first, last, "GET")) {
            method = Method::GET;
            return true;
        }
        if(std::equal(first, last, "PUT")) {
            method = Method::PUT;
            return true;
        }
        if(std::equal(first, last, "POST")) {
            method = Method::POST;
            return true;
        }
        if(std::equal(first, last, "HEAD")) {
            method = Method::HEAD;
            return true;
        }
        if(std::equal(first, last, "DELETE")) {
            method = Method::DELETE;
            return true;
        }
    }
    return false;
}

} // http
} // fluent
#endif