# Wookie Tracker

An ESP32-S3 captive portal that turns a physical LED map into a live visitor board. When someone connects to the Wi-Fi hotspot, a web form asks where they're from. Their answer lights up the corresponding state on the map — and stays lit for the rest of the event.

---

## How It Works

### Captive Portal
The ESP32 broadcasts an open Wi-Fi access point (`MapExplorer`). When a device connects, the DNS server redirects all traffic to the onboard web server, triggering the OS captive portal pop-up automatically on iOS, Android, and Windows.

### Visitor Form
The landing page asks visitors to select their **region** and, if they're from the United States, their **state**. On submission:
- The entry is stored in memory (up to 500 submissions)
- The device's MAC address is recorded to prevent duplicate submissions
- The corresponding LEDs on the map strip light up in the "visited" color
- The onboard RGB LED flashes green three times to confirm

### LED Map
Three independent WS2812B strips are controlled via FastLED:

| Strip | Pin | Purpose |
|---|---|---|
| Map | GPIO 5 | US state map — each state has a configurable LED range |
| Top Banner | GPIO 6 | Decorative top banner |
| Bottom Banner | GPIO 7 | Decorative bottom banner |

Each state entry in `main.cpp` defines a first and last LED index (inclusive), so states with larger physical footprints can span multiple LEDs.

### Admin Panel
A gear icon (⚙) in the corner of the main form opens a password-protected admin panel at `/admin`. From there you can:
- View all current submissions (region + state)
- Clear all entries and reset the LED map back to its default colors

The data is also available as raw JSON at `http://192.168.4.1/data`.

---

## Project Structure

```
esp32-wookie/
├── platformio.ini       # Build configuration
└── src/
    ├── main.cpp         # Firmware — LED control, web server, submission logic
    └── web_content.h    # HTML pages stored in flash (PROGMEM)
```

---

## Development Environment Setup

### Prerequisites

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE extension for VS Code](https://platformio.org/install/ide?install=vscode)

PlatformIO handles the compiler toolchain, board support packages, and library dependencies automatically — no manual Arduino IDE setup needed.

### Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/acpingault/wookie-tracker.git
   cd wookie-tracker
   ```

2. **Open in VS Code**
   ```bash
   code .
   ```
   PlatformIO will detect `platformio.ini` and prompt you to open the project. Accept, then wait for it to install the ESP32 toolchain and the following libraries on first run:
   - `fastled/FastLED ^3.7.0`
   - `mathieucarbou/AsyncTCP ^3.3.1`
   - `mathieucarbou/ESPAsyncWebServer ^3.3.12`

3. **Connect the ESP32-S3**
   Plug the board in via USB. The `build_flags` in `platformio.ini` enable USB CDC on boot, so the board should appear as a serial port automatically.

4. **Build and upload**
   - Click the **Upload** button (→) in the PlatformIO toolbar, or run:
     ```bash
     pio run --target upload
     ```

5. **Monitor serial output**
   Click the **Serial Monitor** plug icon or run:
   ```bash
   pio device monitor
   ```
   Baud rate is set to `115200`. On boot you'll see the AP address and the number of configured states.

### Customizing the LED Map

Open `src/main.cpp` and edit the `states[]` array. Each entry follows this format:

```cpp
// { "StateName", ledFirst, ledLast, defaultColor, visitedColor, false }
{ "California",  0,  2, CRGB::Gold, CRGB::Green, false },
{ "Oregon",      3,  4, CRGB::Gold, CRGB::Green, false },
```

- `ledFirst` / `ledLast` — the inclusive range of LEDs on the map strip that represent this state
- `defaultColor` — color shown on startup before anyone from this state visits
- `visitedColor` — color shown after the first submission from this state

Update `NUM_LEDS`, `NUM_LEDS_BANNER_TOP`, and `NUM_LEDS_BANNER_BOT` at the top of the file to match your physical strip lengths.
