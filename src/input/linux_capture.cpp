#include "input.hpp"
#include "../network/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

#ifdef __linux__

#include <libudev.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>

namespace input {

struct Device {
    int fd;
    struct libevdev *evdev;
};

static std::vector<Device> devices;
static std::atomic<bool> capture_running{false};
static std::thread capture_thread;

// Keep track of keys for escape sequence (Right Ctrl + Right Alt)
static bool rctrl_pressed = false;
static bool ralt_pressed = false;

void translate_and_send(struct input_event& ev, network::Client* client) {
    protocol::InputEvent net_ev{};
    
    if (ev.type == EV_KEY) {
        if (ev.code == KEY_RIGHTCTRL) {
            rctrl_pressed = (ev.value == 1 || ev.value == 2);
        }
        if (ev.code == KEY_RIGHTALT) {
            ralt_pressed = (ev.value == 1 || ev.value == 2);
        }

        if (rctrl_pressed && ralt_pressed) {
            std::cout << "\n[Escape sequence detected] Exiting input capture..." << std::endl;
            // A bit hacky: kill the entire process since asio run loop is main thread
            // Ideally we cleanly shut down ASIO. For a CLI utility, exit(0) is acceptable 
            // to cleanly return control if they aren't using systemd.
            exit(0); 
        }

        if (ev.value == 0) {
            net_ev.type = protocol::EventType::KeyRelease;
        } else if (ev.value == 1 || ev.value == 2) {
            // value 2 is repeat, we treat it as key press for now, or maybe macOS handles repeats?
            // Usually we forward repeats or let the OS do it. Let's forward repeats as presses.
            net_ev.type = protocol::EventType::KeyPress;
        } else {
            return; // Unknown value
        }
        
        // Mouse clicks also come as EV_KEY (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE)
        if (ev.code >= BTN_MISC && ev.code <= BTN_GEAR_UP) {
            net_ev.type = (ev.value == 0) ? protocol::EventType::MouseClickRelease : protocol::EventType::MouseClickPress;
            if (ev.code == BTN_LEFT) net_ev.data.mouse_click.button = 1;
            else if (ev.code == BTN_RIGHT) net_ev.data.mouse_click.button = 2;
            else if (ev.code == BTN_MIDDLE) net_ev.data.mouse_click.button = 3;
            else return; // Ignore other buttons for now
        } else {
            net_ev.data.key.keycode = ev.code;
        }
        client->send_event(net_ev);
    } else if (ev.type == EV_REL) {
        if (ev.code == REL_X) {
            net_ev.type = protocol::EventType::MouseMove;
            net_ev.data.mouse_move.dx = ev.value;
            net_ev.data.mouse_move.dy = 0;
            client->send_event(net_ev);
        } else if (ev.code == REL_Y) {
            net_ev.type = protocol::EventType::MouseMove;
            net_ev.data.mouse_move.dx = 0;
            net_ev.data.mouse_move.dy = ev.value;
            client->send_event(net_ev);
        } else if (ev.code == REL_WHEEL) {
            net_ev.type = protocol::EventType::MouseScroll;
            net_ev.data.mouse_scroll.dx = 0;
            net_ev.data.mouse_scroll.dy = ev.value;
            client->send_event(net_ev);
        } else if (ev.code == REL_HWHEEL) {
            net_ev.type = protocol::EventType::MouseScroll;
            net_ev.data.mouse_scroll.dx = ev.value;
            net_ev.data.mouse_scroll.dy = 0;
            client->send_event(net_ev);
        }
    }
}

void capture_loop(network::Client* client) {
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        std::cerr << "Failed to create epoll instance" << std::endl;
        return;
    }

    for (const auto& dev : devices) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = dev.fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, dev.fd, &ev);
    }

    struct epoll_event events[16];
    
    while (capture_running) {
        int n = epoll_wait(epfd, events, 16, 100); // 100ms timeout to check capture_running
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed" << std::endl;
            break;
        }

        for (int i = 0; i < n; i++) {
            for (auto& dev : devices) {
                if (dev.fd == events[i].data.fd) {
                    struct input_event ev;
                    while (libevdev_next_event(dev.evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
                        translate_and_send(ev, client);
                    }
                    break;
                }
            }
        }
    }

    close(epfd);
}

void start_capture(void* client_ptr) {
    if (capture_running) return;

    network::Client* client = reinterpret_cast<network::Client*>(client_ptr);

    struct udev *udev = udev_new();
    if (!udev) {
        std::cerr << "Can't create udev" << std::endl;
        return;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices_list = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dev_list_entry;

    udev_list_entry_foreach(dev_list_entry, devices_list) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);
        
        const char *devnode = udev_device_get_devnode(dev);
        if (devnode) {
            // Check if it's an event node
            if (strstr(devnode, "/event") != nullptr) {
                int fd = open(devnode, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    struct libevdev *evdev = nullptr;
                    int rc = libevdev_new_from_fd(fd, &evdev);
                    if (rc < 0) {
                        close(fd);
                    } else {
                        // Check if it has keys or relative axes
                        if (libevdev_has_event_type(evdev, EV_KEY) || libevdev_has_event_type(evdev, EV_REL)) {
                            // Grab the device
                            rc = libevdev_grab(evdev, LIBEVDEV_GRAB);
                            if (rc == 0) {
                                std::cout << "Grabbed device: " << libevdev_get_name(evdev) << " (" << devnode << ")" << std::endl;
                                devices.push_back({fd, evdev});
                            } else {
                                std::cerr << "Failed to grab device: " << devnode << std::endl;
                                libevdev_free(evdev);
                                close(fd);
                            }
                        } else {
                            libevdev_free(evdev);
                            close(fd);
                        }
                    }
                }
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    if (devices.empty()) {
        std::cerr << "No input devices successfully grabbed." << std::endl;
        return;
    }

    capture_running = true;
    capture_thread = std::thread(capture_loop, client);
}

void stop_capture() {
    if (!capture_running) return;
    capture_running = false;
    
    if (capture_thread.joinable()) {
        capture_thread.join();
    }

    for (auto& dev : devices) {
        libevdev_grab(dev.evdev, LIBEVDEV_UNGRAB);
        libevdev_free(dev.evdev);
        close(dev.fd);
    }
    devices.clear();
    std::cout << "Linux input capture stopped." << std::endl;
}

} // namespace input
#endif
