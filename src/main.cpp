#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "web_content.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define DATA_PIN              5     // map LED strip
#define DATA_PIN_BANNER_TOP   6     // top banner LED strip
#define DATA_PIN_BANNER_BOT   7     // bottom banner LED strip
#define NUM_LEDS              100   // LEDs in the map strip
#define NUM_LEDS_BANNER_TOP   30    // LEDs in the top banner
#define NUM_LEDS_BANNER_BOT   30    // LEDs in the bottom banner
#define LED_TYPE          WS2812B
#define COLOR_ORDER       GRB
#define BRIGHTNESS        64        // 0–255
#define ONBOARD_LED_PIN   48        // RGB WS2812B on GPIO 48 (ESP32-S3 DevKitC)
#define BLINK_INTERVAL_MS 500
#define HEAT_MAX          10        // submission count that reaches peak "hot" red

// Access Point credentials
static const char* AP_SSID     = "MapExplorer";
static const char* AP_PASSWORD = "";            // leave empty for open network

// Maximum number of visitor submissions to store in memory
static const uint16_t MAX_SUBMISSIONS = 500;
// ─────────────────────────────────────────────────────────────────────────────

// ── LED arrays ────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];
CRGB bannerTopLeds[NUM_LEDS_BANNER_TOP];
CRGB bannerBotLeds[NUM_LEDS_BANNER_BOT];
CRGB onboardLed[1];

// ── State map ─────────────────────────────────────────────────────────────────
struct State {
    const char* name;
    uint8_t     ledFirst;        // first LED index in the map strip (inclusive)
    uint8_t     ledLast;         // last  LED index in the map strip (inclusive)
    CRGB        idleColor;       // shown when count == 0
    uint16_t    count;           // number of submissions from this state
};

static State states[] = {
    // { "StateName", ledFirst, ledLast, idleColor, count }
    { "Alabama",         0,  1, CRGB::DimGray, 0 },
    { "Alaska",          2,  3, CRGB::DimGray, 0 },
    { "Arizona",         4,  5, CRGB::DimGray, 0 },
    { "Arkansas",        6,  7, CRGB::DimGray, 0 },
    { "California",      8,  9, CRGB::DimGray, 0 },
    { "Colorado",       10, 11, CRGB::DimGray, 0 },
    { "Connecticut",    12, 13, CRGB::DimGray, 0 },
    { "Delaware",       14, 15, CRGB::DimGray, 0 },
    { "Florida",        16, 17, CRGB::DimGray, 0 },
    { "Georgia",        18, 19, CRGB::DimGray, 0 },
    { "Hawaii",         20, 21, CRGB::DimGray, 0 },
    { "Idaho",          22, 23, CRGB::DimGray, 0 },
    { "Illinois",       24, 25, CRGB::DimGray, 0 },
    { "Indiana",        26, 27, CRGB::DimGray, 0 },
    { "Iowa",           28, 29, CRGB::DimGray, 0 },
    { "Kansas",         30, 31, CRGB::DimGray, 0 },
    { "Kentucky",       32, 33, CRGB::DimGray, 0 },
    { "Louisiana",      34, 35, CRGB::DimGray, 0 },
    { "Maine",          36, 37, CRGB::DimGray, 0 },
    { "Maryland",       38, 39, CRGB::DimGray, 0 },
    { "Massachusetts",  40, 41, CRGB::DimGray, 0 },
    { "Michigan",       42, 43, CRGB::DimGray, 0 },
    { "Minnesota",      44, 45, CRGB::DimGray, 0 },
    { "Mississippi",    46, 47, CRGB::DimGray, 0 },
    { "Missouri",       48, 49, CRGB::DimGray, 0 },
    { "Montana",        50, 51, CRGB::DimGray, 0 },
    { "Nebraska",       52, 53, CRGB::DimGray, 0 },
    { "Nevada",         54, 55, CRGB::DimGray, 0 },
    { "New Hampshire",  56, 57, CRGB::DimGray, 0 },
    { "New Jersey",     58, 59, CRGB::DimGray, 0 },
    { "New Mexico",     60, 61, CRGB::DimGray, 0 },
    { "New York",       62, 63, CRGB::DimGray, 0 },
    { "North Carolina", 64, 65, CRGB::DimGray, 0 },
    { "North Dakota",   66, 67, CRGB::DimGray, 0 },
    { "Ohio",           68, 69, CRGB::DimGray, 0 },
    { "Oklahoma",       70, 71, CRGB::DimGray, 0 },
    { "Oregon",         72, 73, CRGB::DimGray, 0 },
    { "Pennsylvania",   74, 75, CRGB::DimGray, 0 },
    { "Rhode Island",   76, 77, CRGB::DimGray, 0 },
    { "South Carolina", 78, 79, CRGB::DimGray, 0 },
    { "South Dakota",   80, 81, CRGB::DimGray, 0 },
    { "Tennessee",      82, 83, CRGB::DimGray, 0 },
    { "Texas",          84, 85, CRGB::DimGray, 0 },
    { "Utah",           86, 87, CRGB::DimGray, 0 },
    { "Vermont",        88, 89, CRGB::DimGray, 0 },
    { "Virginia",       90, 91, CRGB::DimGray, 0 },
    { "Washington",     92, 93, CRGB::DimGray, 0 },
    { "West Virginia",  94, 95, CRGB::DimGray, 0 },
    { "Wisconsin",      96, 97, CRGB::DimGray, 0 },
    { "Wyoming",        98, 99, CRGB::DimGray, 0 },
};

static const uint8_t NUM_STATES = sizeof(states) / sizeof(states[0]);

// ── Submission storage ────────────────────────────────────────────────────────
struct Submission {
    char state[64];
    char country[64];
};

static Submission submissions[MAX_SUBMISSIONS];
static uint16_t   submissionCount = 0;

// One MAC address (6 bytes) stored per accepted submission.
static uint8_t seenMACs[MAX_SUBMISSIONS][6];
static uint16_t seenMACCount = 0;

// Set by the web handler; consumed in loop() to trigger animations.
static volatile bool    newSubmission    = false;
static volatile int16_t newSubmissionIdx = -1;   // states[] index, or -1 if no match

// Index of the state with the highest submission count (-1 if all zero).
static int16_t leadingStateIdx = -1;

// ── Web server & DNS ──────────────────────────────────────────────────────────
AsyncWebServer server(80);
DNSServer      dnsServer;

// ── LED helpers ───────────────────────────────────────────────────────────────

// Maps a submission count to a heatmap color.
// 0            → idleColor (caller's responsibility)
// 1            → blue (cool)
// HEAT_MAX / 2 → green / yellow (warm)
// HEAT_MAX+    → red (hot), clamped
//
// Gradient stops: blue → cyan → green → yellow → red
CRGB heatmapColor(uint16_t count) {
    if (count == 0) return CRGB::Black; // caller substitutes idleColor

    // Clamp to [1, HEAT_MAX] then map to 0–255
    uint16_t clamped = count > HEAT_MAX ? HEAT_MAX : count;
    uint8_t  t       = (uint8_t)((uint32_t)(clamped - 1) * 255 / (HEAT_MAX - 1));

    // Four equal segments across five stops
    static const CRGB stops[5] = {
        CRGB(  0,   0, 255),   // blue
        CRGB(  0, 255, 255),   // cyan
        CRGB(  0, 255,   0),   // green
        CRGB(255, 255,   0),   // yellow
        CRGB(255,   0,   0),   // red
    };
    uint8_t seg  = t / 64;             // 0–3
    uint8_t frac = (t % 64) * 4;      // 0–255 within segment
    if (seg >= 4) return stops[4];
    return blend(stops[seg], stops[seg + 1], frac);
}

// Redraw all map LEDs from current counts.
void refreshLEDs() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (uint8_t i = 0; i < NUM_STATES; i++) {
        CRGB color = (states[i].count == 0) ? states[i].idleColor
                                             : heatmapColor(states[i].count);
        for (uint8_t j = states[i].ledFirst; j <= states[i].ledLast; j++) {
            leds[j] = color;
        }
    }
    FastLED.show();
}

// Case-insensitive search; returns index into states[] or -1 if not found.
int findState(const char* name) {
    for (uint8_t i = 0; i < NUM_STATES; i++) {
        if (strcasecmp(states[i].name, name) == 0) return i;
    }
    return -1;
}

// Returns the index of the state with the highest count (>0), or -1 if all zero.
int16_t findLeadingState() {
    int16_t  best      = -1;
    uint16_t bestCount = 0;
    for (uint8_t i = 0; i < NUM_STATES; i++) {
        if (states[i].count > bestCount) {
            bestCount = states[i].count;
            best      = i;
        }
    }
    return best;
}

// Increment a state's submission count and update its LEDs immediately.
void incrementState(int idx) {
    if (idx < 0 || idx >= NUM_STATES) return;
    states[idx].count++;
    CRGB color = heatmapColor(states[idx].count);
    for (uint8_t j = states[idx].ledFirst; j <= states[idx].ledLast; j++) {
        leds[j] = color;
    }
    FastLED.show();
    leadingStateIdx = findLeadingState();
}

// ── MAC address helpers ───────────────────────────────────────────────────────

// Resolve the MAC address for a connected client by matching their IP against
// the AP's station list. Returns true and populates macOut on success.
bool getMACForIP(IPAddress clientIP, uint8_t macOut[6]) {
    wifi_sta_list_t      stationList;
    esp_netif_sta_list_t netifList;

    if (esp_wifi_ap_get_sta_list(&stationList)          != ESP_OK) return false;
    if (esp_netif_get_sta_list(&stationList, &netifList) != ESP_OK) return false;

    for (int i = 0; i < netifList.num; i++) {
        if (clientIP == IPAddress(netifList.sta[i].ip.addr)) {
            memcpy(macOut, netifList.sta[i].mac, 6);
            return true;
        }
    }
    return false;
}

bool hasSeenMAC(const uint8_t mac[6]) {
    for (uint16_t i = 0; i < seenMACCount; i++) {
        if (memcmp(seenMACs[i], mac, 6) == 0) return true;
    }
    return false;
}

void recordMAC(const uint8_t mac[6]) {
    if (seenMACCount < MAX_SUBMISSIONS) {
        memcpy(seenMACs[seenMACCount++], mac, 6);
    }
}

// ── Web server setup ──────────────────────────────────────────────────────────
void setupServer() {
    // Serve the form
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", INDEX_HTML);
    });

    // Handle form submission
    server.on("/submit", HTTP_POST, [](AsyncWebServerRequest* req) {
        // Check for duplicate submission from this device
        uint8_t mac[6];
        bool    macKnown = getMACForIP(req->client()->remoteIP(), mac);

        if (macKnown && hasSeenMAC(mac)) {
            req->send_P(200, "text/html", DUPLICATE_HTML);
            return;
        }

        if (!req->hasParam("state", true) || !req->hasParam("country", true)) {
            req->send(400, "text/plain", "Missing fields");
            return;
        }

        String state   = req->getParam("state",   true)->value();
        String country = req->getParam("country", true)->value();

        state.trim();
        country.trim();

        // Store the submission
        if (submissionCount < MAX_SUBMISSIONS) {
            strncpy(submissions[submissionCount].state,   state.c_str(),   63);
            strncpy(submissions[submissionCount].country, country.c_str(), 63);
            submissions[submissionCount].state[63]   = '\0';
            submissions[submissionCount].country[63] = '\0';
            submissionCount++;
        }

        // Record their MAC so they can't submit again
        if (macKnown) recordMAC(mac);

        // Update the LED if this state is on the map
        int idx = findState(state.c_str());
        if (idx >= 0) incrementState(idx);

        newSubmissionIdx = idx;
        newSubmission    = true;

        Serial.printf("Submission #%d — state: %s, country: %s\n",
                      submissionCount, state.c_str(), country.c_str());

        req->send_P(200, "text/html", SUCCESS_HTML);
    });

    // Admin panel page
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", ADMIN_HTML);
    });

    // Clear all submissions and reset LED map
    server.on("/admin/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
        submissionCount = 0;
        seenMACCount    = 0;
        for (uint8_t i = 0; i < NUM_STATES; i++) {
            states[i].count = 0;
        }
        leadingStateIdx = -1;
        refreshLEDs();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // JSON endpoint — visit http://192.168.4.1/data to see all submissions
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* req) {
        String json = "{\"count\":";
        json += submissionCount;
        json += ",\"submissions\":[";
        for (uint16_t i = 0; i < submissionCount; i++) {
            if (i > 0) json += ',';
            json += "{\"state\":\"";
            json += submissions[i].state;
            json += "\",\"country\":\"";
            json += submissions[i].country;
            json += "\"}";
        }
        json += "]}";
        req->send(200, "application/json", json);
    });

    // ── Captive portal detection endpoints ────────────────────────────────────
    // Each OS probes a different URL to decide whether to show the portal popup.

    // iOS / macOS
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });

    // Android
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    // Windows
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    // Catch-all: redirect anything else to the form
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    server.begin();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Onboard RGB LED
    FastLED.addLeds<WS2812B, ONBOARD_LED_PIN, GRB>(onboardLed, 1);
    // Map LED strip
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
    // Banner LED strips
    FastLED.addLeds<LED_TYPE, DATA_PIN_BANNER_TOP, COLOR_ORDER>(bannerTopLeds, NUM_LEDS_BANNER_TOP)
           .setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, DATA_PIN_BANNER_BOT, COLOR_ORDER>(bannerBotLeds, NUM_LEDS_BANNER_BOT)
           .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    onboardLed[0] = CRGB::Black;
    FastLED.show();
    delay(500);

    refreshLEDs();

    // Start Access Point
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("AP started  — SSID: %s\n", AP_SSID);
    Serial.printf("Web app at  — http://%s\n", WiFi.softAPIP().toString().c_str());

    // DNS server: send every domain lookup back to us so the OS shows the portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    setupServer();
    Serial.printf("Map ready   — %d state(s) defined\n", NUM_STATES);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    dnsServer.processNextRequest();

    // ── New submission: blink the state's LEDs + onboard LED green 3 times ──
    if (newSubmission) {
        newSubmission = false;
        int16_t idx = newSubmissionIdx;
        newSubmissionIdx = -1;

        for (uint8_t i = 0; i < 3; i++) {
            // Green on
            onboardLed[0] = CRGB::Green;
            if (idx >= 0) {
                for (uint8_t j = states[idx].ledFirst; j <= states[idx].ledLast; j++)
                    leds[j] = CRGB::Green;
            }
            FastLED.show();
            delay(150);

            // Green off
            onboardLed[0] = CRGB::Black;
            if (idx >= 0) {
                for (uint8_t j = states[idx].ledFirst; j <= states[idx].ledLast; j++)
                    leds[j] = CRGB::Black;
            }
            FastLED.show();
            delay(150);
        }

        // Restore heatmap colors after blink
        refreshLEDs();
    }

    // ── Pulse the leading state ───────────────────────────────────────────────
    // Uses sin8() to produce a smooth 0–255 sine wave, scaled to stay in the
    // upper half of brightness so the color is always recognisable.
    static uint32_t lastPulse = 0;
    if (millis() - lastPulse >= 20) {   // ~50 fps
        lastPulse = millis();
        int16_t li = leadingStateIdx;
        if (li >= 0 && states[li].count > 0) {
            // sin8 input wraps every 256 counts; >> 3 gives ~2 s period
            uint8_t level = 128 + scale8(sin8((uint8_t)(millis() >> 3)), 127);
            CRGB    color = heatmapColor(states[li].count);
            color.nscale8(level);
            for (uint8_t j = states[li].ledFirst; j <= states[li].ledLast; j++)
                leds[j] = color;
            FastLED.show();
        }
    }

    // ── Slow blue heartbeat on onboard LED ───────────────────────────────────
    static uint32_t lastBlink = 0;
    static bool     ledState  = false;

    if (millis() - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = millis();
        ledState  = !ledState;
        onboardLed[0] = ledState ? CRGB::Blue : CRGB::Black;
        FastLED.show();
    }
}
