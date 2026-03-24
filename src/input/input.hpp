#pragma once

#include "../protocol/input_event.hpp"

namespace input {
#ifdef __linux__
    void start_capture(void* client_ptr);
    void stop_capture();
#elif defined(__APPLE__)
    void init_injection();
    void handle_event(const protocol::InputEvent& event);
#endif
}
