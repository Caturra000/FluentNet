#ifndef __FLUENT_HTTP_PARSER_H__
#define __FLUENT_HTTP_PARSER_H__
#include <bits/stdc++.h>
#include "Stream.h"
#include "Request.h"
namespace fluent {
namespace http {

class Parser final {
public:
    template <typename Iter>
    ssize_t parse(Request &request, Stream &stream, Iter first, Iter last);

private:
    template <typename Iter>
    bool parseNextLine(Request &request, Iter first, Iter last);

    template <typename Iter>
    bool parseInit(ssize_t &consumed, bool &hasNext, Request &request, Stream &stream, Iter first, Iter last);

    template <typename Iter>
    bool parseNextHeader(ssize_t &consumed, bool &hasNext, Request &request, Stream &stream, Iter first, Iter last);
};

template <typename Iter>
inline ssize_t Parser::parse(Request &request, Stream &stream, Iter first, Iter last) {
    using StreamState = Stream::StreamState;
    ssize_t consumed = 0;
    bool hasNext = true;
    // http message may be incomplete,
    // while(!state.ready()) is inapplicable
    while(hasNext) switch(stream.state) {
        case StreamState::INIT:
            if(!parseInit(consumed, hasNext, request, stream, first, last)) {
                return -400; // bad request
            }
            break;
        case StreamState::NEXT_HEADER:
            if(!parseNextHeader(consumed, hasNext, request, stream, first, last)) {
                return -400;
            }
            break;
        case StreamState::NEXT_BODY:
        case StreamState::READY:
        default:
            hasNext = false;
            break;
    }
    return consumed;
}

// TODO add UNLIKELY macro
template <typename Iter>
inline bool Parser::parseNextLine(Request &request, Iter first, Iter last) {
    Iter space = std::find(first, last, ' ');
    // method
    bool methodDone = (space != last && request.setMethod(first, space));
    if(!methodDone) return false;

    first = std::next(space);
    space = std::find(first, last, ' ');

    // path and query
    // TODO query
    if(std::find(first, space, '?') != space) {
        throw FluentException("currently does not support query");
    }
    request.path.assign(first, space);

    first = std::next(space);

    // version
    last = std::prev(last, 2);
    bool versionValidate = std::distance(first, last) == 8 && std::equal(first, std::prev(last), "HTTP/1.");
    if(!versionValidate) return false;
    versionValidate = *std::prev(last) == '0' || *std::prev(last) == '1';
    if(!versionValidate) return false;
    request.version = *std::prev(last) == '0' ? Request::Version::V10 : Request::Version::V11;
    return true;
}

template <typename Iter>
inline bool Parser::parseInit(ssize_t &consumed, bool &hasNext, Request &request, Stream &stream, Iter first, Iter last) {
    Iter rpos = std::find(first, last, '\r');
    Iter npos = std::next(rpos);
    if(rpos != last && rpos != first && npos != last && *npos == '\n') {
        Iter nextBegin = std::next(rpos, 2);
        if(!parseNextLine(request, first, nextBegin)) {
            return false; // error code: 400
        }
        stream.state = Stream::StreamState::NEXT_HEADER;
        auto difference = std::distance(first, nextBegin);
        consumed += difference;
        first += difference;
        hasNext = bool(first < last);
    } else {
        hasNext = false;
    }
    return true;
}

template <typename Iter>
inline bool Parser::parseNextHeader(ssize_t &consumed, bool &hasNext, Request &request, Stream &stream, Iter first, Iter last) {
    Iter rpos = std::find(first, last, '\r');
    Iter npos = std::next(rpos);
    Iter nextBegin = std::next(npos);
    if(rpos != last && npos != last && *npos == '\n') {
        if(rpos == first) {
            Iter pos = std::find(first, rpos, ':');
            if(pos == rpos) {
                return false;
            }
            std::string header(first, std::distance(first, pos));
            // {: }
            std::string content(std::next(pos, 2), std::distance(std::next(pos, 2), rpos));
            request.headers[std::move(header)] = std::move(content);

            auto difference = std::distance(first, nextBegin);
            consumed += difference;
            first += difference;
            hasNext = bool(first < last);
        } else {
            // TODO NEXT_BODY
            stream.state = Stream::StreamState::READY;
        }
    } else {
        hasNext = false;
    }
    return true;
}


} // http
} // fluent
#endif