#include "input.hpp"
#include <iostream>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <map>

namespace input {

// Basic mapping from Linux evdev keycodes to macOS Virtual Keycodes.
// This is not exhaustive.
static const std::map<uint16_t, CGKeyCode> evdev_to_mac_key = {
    {1, 53}, // KEY_ESC -> kVK_Escape
    {2, 18}, // KEY_1 -> kVK_ANSI_1
    {3, 19}, // KEY_2 -> kVK_ANSI_2
    {4, 20}, // KEY_3 -> kVK_ANSI_3
    {5, 21}, // KEY_4 -> kVK_ANSI_4
    {6, 23}, // KEY_5 -> kVK_ANSI_5
    {7, 22}, // KEY_6 -> kVK_ANSI_6
    {8, 26}, // KEY_7 -> kVK_ANSI_7
    {9, 28}, // KEY_8 -> kVK_ANSI_8
    {10, 25}, // KEY_9 -> kVK_ANSI_9
    {11, 29}, // KEY_0 -> kVK_ANSI_0
    {12, 27}, // KEY_MINUS -> kVK_ANSI_Minus
    {13, 24}, // KEY_EQUAL -> kVK_ANSI_Equal
    {14, 51}, // KEY_BACKSPACE -> kVK_Delete
    {15, 48}, // KEY_TAB -> kVK_Tab
    {16, 12}, // KEY_Q -> kVK_ANSI_Q
    {17, 13}, // KEY_W -> kVK_ANSI_W
    {18, 14}, // KEY_E -> kVK_ANSI_E
    {19, 15}, // KEY_R -> kVK_ANSI_R
    {20, 17}, // KEY_T -> kVK_ANSI_T
    {21, 16}, // KEY_Y -> kVK_ANSI_Y
    {22, 32}, // KEY_U -> kVK_ANSI_U
    {23, 34}, // KEY_I -> kVK_ANSI_I
    {24, 31}, // KEY_O -> kVK_ANSI_O
    {25, 35}, // KEY_P -> kVK_ANSI_P
    {26, 33}, // KEY_LEFTBRACE -> kVK_ANSI_LeftBracket
    {27, 30}, // KEY_RIGHTBRACE -> kVK_ANSI_RightBracket
    {28, 36}, // KEY_ENTER -> kVK_Return
    {29, 59}, // KEY_LEFTCTRL -> kVK_Control
    {30, 0},  // KEY_A -> kVK_ANSI_A
    {31, 1},  // KEY_S -> kVK_ANSI_S
    {32, 2},  // KEY_D -> kVK_ANSI_D
    {33, 3},  // KEY_F -> kVK_ANSI_F
    {34, 5},  // KEY_G -> kVK_ANSI_G
    {35, 4},  // KEY_H -> kVK_ANSI_H
    {36, 38}, // KEY_J -> kVK_ANSI_J
    {37, 40}, // KEY_K -> kVK_ANSI_K
    {38, 37}, // KEY_L -> kVK_ANSI_L
    {39, 41}, // KEY_SEMICOLON -> kVK_ANSI_Semicolon
    {40, 39}, // KEY_APOSTROPHE -> kVK_ANSI_Quote
    {41, 50}, // KEY_GRAVE -> kVK_ANSI_Grave
    {42, 56}, // KEY_LEFTSHIFT -> kVK_Shift
    {43, 42}, // KEY_BACKSLASH -> kVK_ANSI_Backslash
    {44, 6},  // KEY_Z -> kVK_ANSI_Z
    {45, 7},  // KEY_X -> kVK_ANSI_X
    {46, 8},  // KEY_C -> kVK_ANSI_C
    {47, 9},  // KEY_V -> kVK_ANSI_V
    {48, 11}, // KEY_B -> kVK_ANSI_B
    {49, 45}, // KEY_N -> kVK_ANSI_N
    {50, 46}, // KEY_M -> kVK_ANSI_M
    {51, 43}, // KEY_COMMA -> kVK_ANSI_Comma
    {52, 47}, // KEY_DOT -> kVK_ANSI_Period
    {53, 44}, // KEY_SLASH -> kVK_ANSI_Slash
    {54, 60}, // KEY_RIGHTSHIFT -> kVK_RightShift
    {55, 67}, // KEY_KPASTERISK -> kVK_ANSI_KeypadMultiply
    {56, 58}, // KEY_LEFTALT -> kVK_Option
    {57, 49}, // KEY_SPACE -> kVK_Space
    {58, 57}, // KEY_CAPSLOCK -> kVK_CapsLock
    {105, 123}, // KEY_LEFT -> kVK_LeftArrow
    {106, 124}, // KEY_RIGHT -> kVK_RightArrow
    {103, 126}, // KEY_UP -> kVK_UpArrow
    {108, 125}, // KEY_DOWN -> kVK_DownArrow
    {125, 55}   // KEY_LEFTMETA -> kVK_Command
};

static CGEventSourceRef eventSource;

void init_injection() {
    eventSource = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    std::cout << "macOS input injection initialized." << std::endl;
}

CGPoint get_current_mouse_location() {
    CGEventRef event = CGEventCreate(nullptr);
    CGPoint cursor = CGEventGetLocation(event);
    CFRelease(event);
    return cursor;
}

void handle_event(const protocol::InputEvent& event) {
    if (event.type == protocol::EventType::KeyPress || event.type == protocol::EventType::KeyRelease) {
        auto it = evdev_to_mac_key.find(event.data.key.keycode);
        if (it != evdev_to_mac_key.end()) {
            bool isDown = (event.type == protocol::EventType::KeyPress);
            CGEventRef keyEvent = CGEventCreateKeyboardEvent(eventSource, it->second, isDown);
            CGEventPost(kCGHIDEventTap, keyEvent);
            CFRelease(keyEvent);
        } else {
            std::cerr << "Unmapped keycode: " << event.data.key.keycode << std::endl;
        }
    } 
    else if (event.type == protocol::EventType::MouseMove) {
        CGPoint cursor = get_current_mouse_location();
        cursor.x += event.data.mouse_move.dx;
        cursor.y += event.data.mouse_move.dy;

        CGEventRef moveEvent = CGEventCreateMouseEvent(
            eventSource, kCGEventMouseMoved, cursor, kCGMouseButtonLeft // Button param ignored for purely moving
        );
        CGEventPost(kCGHIDEventTap, moveEvent);
        CFRelease(moveEvent);
    }
    else if (event.type == protocol::EventType::MouseClickPress || event.type == protocol::EventType::MouseClickRelease) {
        CGPoint cursor = get_current_mouse_location();
        bool isDown = (event.type == protocol::EventType::MouseClickPress);
        
        CGEventType cgType;
        CGMouseButton cgButton;

        if (event.data.mouse_click.button == 1) { // Left
            cgType = isDown ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            cgButton = kCGMouseButtonLeft;
        } else if (event.data.mouse_click.button == 2) { // Right
            cgType = isDown ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            cgButton = kCGMouseButtonRight;
        } else if (event.data.mouse_click.button == 3) { // Middle
            cgType = isDown ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            cgButton = kCGMouseButtonCenter;
        } else {
            return;
        }

        CGEventRef clickEvent = CGEventCreateMouseEvent(eventSource, cgType, cursor, cgButton);
        CGEventPost(kCGHIDEventTap, clickEvent);
        CFRelease(clickEvent);
    }
    else if (event.type == protocol::EventType::MouseScroll) {
        // macOS ScrollWheel event expects units (lines or pixels).
        // 1 represents a small scroll step.
        CGEventRef scrollEvent = CGEventCreateScrollWheelEvent(
            eventSource, kCGScrollEventUnitLine, 2, event.data.mouse_scroll.dy, event.data.mouse_scroll.dx
        );
        CGEventPost(kCGHIDEventTap, scrollEvent);
        CFRelease(scrollEvent);
    }
}

} // namespace input
#endif
