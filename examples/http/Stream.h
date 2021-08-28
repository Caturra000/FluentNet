#ifndef __FLUENT_HTTP_STREAM_H__
#define __FLUENT_HTTP_STREAM_H__
namespace fluent {
namespace http {

struct Stream {
    enum class StreamState {
        INIT,
        NEXT_HEADER,
        NEXT_BODY,
        READY,
    } state {StreamState::INIT};

    bool ready() { return state == StreamState::READY; }
};

} // http
} // fluent
#endif