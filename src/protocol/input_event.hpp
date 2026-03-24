#pragma once

#include <cstdint>

namespace protocol {

enum class EventType : uint8_t {
    KeyPress,
    KeyRelease,
    MouseMove, // Relative movement
    MouseClickPress,
    MouseClickRelease,
    MouseScroll
};

// Ensure standard alignment for cross-platform network transmission
#pragma pack(push, 1)
struct InputEvent {
    EventType type;
    union {
        struct {
            uint16_t keycode; // Linux evdev keycode
        } key;
        struct {
            int32_t dx;
            int32_t dy;
        } mouse_move;
        struct {
            uint8_t button; // Left: 1, Right: 2, Middle: 3
        } mouse_click;
        struct {
            int32_t dx; // Horizontal scroll
            int32_t dy; // Vertical scroll
        } mouse_scroll;
    } data;
};
#pragma pack(pop)

} // namespace protocol