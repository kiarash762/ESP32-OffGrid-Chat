# ESP32 Off-Grid Mesh Chat 📡💬

> **Decentralized, zero-internet offline messaging server running entirely on an ESP32 microcontroller.**

An emergency and disaster-resilient local communication node. The ESP32 hosts its own Wi-Fi Access Point (SoftAP), captive portal, HTTP server, and real-time WebSockets hub. Any connected device (smartphone, tablet, laptop) can instantly chat through their web browser without installing any applications or having internet access.

---

## ✨ Features

- **🌐 100% Offline & Autonomous:** No internet, router, SIM card, or external server required.
- **⚡ Real-Time WebSockets:** Zero-latency, instant bidirectional message distribution across all connected peers.
- **🚪 Auto Captive Portal:** Built-in DNS server automatically redirects connecting devices to the chat interface.
- **↩️ Rich Chat Features:**
  - **Reply:** Quote and respond to specific messages with quick navigation.
  - **Edit:** In-place message updating with real-time sync and `(edited)` indicator.
  - **Delete:** Soft deletion with broadcast across all active clients.
  - **Usernames:** Customizable client nicknames stored in browser `localStorage`.
- **🌙 Modern Mobile-First Dark UI:** Responsive single-page application (SPA) embedded directly in the ESP32's flash memory (`PROGMEM`), featuring full RTL (Persian/Arabic) typography support.
- **👥 Active Peer Counter:** Live display of currently connected users on the local node.

---

## 🛠️ Hardware Requirements

- **Microcontroller:** Any ESP32 development board (ESP32-WROOM-32, ESP32-D0WD-V3, NodeMCU-32S, etc.)
- **Power:** USB cable connected to a laptop, phone charger, or power bank (can run for days on a portable battery pack).

---

## 🚀 Getting Started

### Prerequisites

1. Install [Arduino IDE](https://www.arduino.cc/en/software) or `arduino-cli`.
2. Add ESP32 board support via Arduino Board Manager:
   - URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install required library:
   - **WebSockets** by *Markus Sattler* (search `WebSockets` in Library Manager).

### Configuration & Flashing

1. Clone or download this repository:
   ```bash
   git clone https://github.com/kiarash762/ESP32-OffGrid-Chat.git
   ```
2. Open `OffGridChat.ino` in Arduino IDE.
3. (Optional) Customize the Wi-Fi credentials at the top of the file:
   ```cpp
   const char* AP_SSID = "OffGrid-Chat";
   const char* AP_PASS = "kiaGOD762"; // Minimum 8 characters, or "" for open network
   ```
4. Select your board (e.g., `ESP32 Dev Module`) and the correct COM port.
5. Click **Upload**.

---

## 📱 How to Use

1. Turn on the ESP32 (powered via USB / power bank).
2. On your phone or laptop, search for Wi-Fi networks and connect to **`OffGrid-Chat`**.
3. Enter the Wi-Fi password (default: `kiaGOD762`).
4. A notification ("Sign in to network") will pop up automatically. Tap it, or open any web browser and navigate to:
   ```
   http://192.168.4.1
   ```
5. Choose your nickname and start chatting in real-time with everyone connected to the node!

---

## 🏗️ Architecture

```
                      +-----------------------------+
                      |       ESP32 Microcontroller |
                      |                             |
                      |  [Wi-Fi SoftAP 2.4 GHz]     |
                      |  [DNS Server - Port 53]     |
                      |  [HTTP Server - Port 80]    |
                      |  [WebSocket Server - P: 81] |
                      +--------------+--------------+
                                     |
               +---------------------+---------------------+
               |                     |                     |
         [Smartphone A]        [Smartphone B]          [Laptop C]
         (Captive Portal)      (Captive Portal)       (Browser)
```

- **Wi-Fi SoftAP:** Allocates local IP addresses (`192.168.4.x`) via built-in DHCP.
- **DNS Server:** Captures all DNS requests (`*`) and resolves them to `192.168.4.1`, triggering native OS Captive Portal detection on iOS, Android, and Windows.
- **Web Server:** Serves the compressed HTML5/CSS3/Vanilla JS single-page web app.
- **WebSocket Server:** Maintains persistent TCP sockets for real-time JSON event dispatching (`msg`, `edit`, `delete`, `status`).

---

## 📜 License

This project is licensed under the [MIT License](LICENSE).
