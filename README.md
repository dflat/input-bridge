# Input Bridge

Input Bridge is a lightweight software KVM (Keyboard, Video, Mouse - minus the Video) written in modern C++20. It allows you to seamlessly share your Linux machine's keyboard and mouse/trackpad with a macOS machine over your local network.

It is designed to be low-latency, using TCP with Nagle's algorithm disabled. On the Linux side, it uses `libevdev` and `EVIOCGRAB` to exclusively capture inputs (preventing your local Sway/Wayland from receiving them) while forwarding them to the Mac. On the macOS side, it injects these events into the system using the `CoreGraphics` framework.

## Features

- **Exclusive Input Capture:** Grabs input devices on Linux so your keystrokes don't affect your local machine while controlling the remote Mac.
- **Cross-Platform Binary:** A single codebase compiled for both platforms. Run `--client` on Linux and `--server` on macOS.
- **Dependency Free (mostly):** Uses `FetchContent` to download Standalone ASIO automatically. No `brew` required on macOS!
- **Systemd Integration:** Easily toggle control by starting/stopping a systemd service on Linux.

## Requirements

### Linux (Client / Sender)
- `libevdev` (e.g., `sudo pacman -S libevdev` or `sudo apt install libevdev-dev`)
- `libudev` (e.g., `sudo pacman -S systemd` or `sudo apt install libudev-dev`)
- `cmake`, `gcc` or `clang`

### macOS (Server / Receiver)
- `cmake`, `clang` (via Xcode Command Line Tools)
- *Note: No third-party package managers like Homebrew are required.*

## Building and Installation

### Linux
```bash
git clone https://github.com/yourusername/input-bridge.git
cd input-bridge
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### macOS
```bash
git clone https://github.com/yourusername/input-bridge.git
cd input-bridge
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Usage

### 1. Start the Server (macOS)
On your Mac, start the receiver. It will listen for incoming connections.
```bash
./build/input-bridge --server 8080
```
*Note: macOS will likely prompt you to grant the Terminal (or the executable) **Accessibility** permissions. This is required to inject synthetic keyboard and mouse events into the OS. Go to System Settings -> Privacy & Security -> Accessibility and toggle it on.*

### 2. Connect the Client (Linux)
On your Linux machine, you can run the client manually (requires root to grab `/dev/input` nodes):
```bash
sudo input-bridge --client <MACOS_IP_ADDRESS> 8080
```
Once connected, your mouse and keyboard on the Linux machine will immediately start controlling the Mac. 
**To stop manually:** Press **`Left Alt + Backslash`** simultaneously on your keyboard. This is a hardcoded escape sequence that will terminate the client and return input to your Linux machine.

### 3. Systemd Toggle (Recommended)
The best way to use this is via a systemd service, so you can map a keybind in your window manager (like Sway) to start and stop the service.

1. Edit the provided `input-bridge.service` file and replace `<macos-ip>` with your Mac's actual IP address.
2. Copy the service file:
   ```bash
   sudo cp input-bridge.service /etc/systemd/system/
   sudo systemctl daemon-reload
   ```
3. Start forwarding:
   ```bash
   sudo systemctl start input-bridge
   ```
4. Stop forwarding (returns control to Linux):
   ```bash
   sudo systemctl stop input-bridge
   ```

You can bind these commands to a shortcut in your Sway config for a seamless KVM switch experience!

## Testing Mappings Locally
If you want to test how `libevdev` sees your keys on Linux without forwarding them to a Mac, you can use the `--dry-run` option. This will grab your devices as usual but just print the intercepted keycodes to the terminal:
```bash
sudo input-bridge --dry-run
```
Press `Left Alt + Backslash` to exit.

## How it Works
1. **Discovery:** The Linux client uses `libudev` to scan for all connected input devices (keyboards, mice, trackpads) that expose `EV_KEY` or `EV_REL` events.
2. **Grab:** It calls `libevdev_grab` on these devices. This creates an exclusive lock at the kernel level.
3. **Forwarding:** Raw Linux `input_event` structs are translated into a custom cross-platform binary protocol and sent over TCP.
4. **Injection:** The macOS server receives these packets, maps the generic Linux keycodes to macOS `CGKeyCode` virtual keycodes, and posts them to the window server via `CGEventPost`.

## Limitations
- Custom mouse buttons (Forward/Back) are not currently fully mapped.
- Keycode mapping from Linux to macOS might miss some obscure keys. The core standard keys are mapped.
