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
    CRGB        defaultColor;    // shown on startup
    CRGB        visitedColor;    // shown after at least one submission
    bool        visited;
};

static State states[] = {
    // { "StateName", ledFirst, ledLast, defaultColor, visitedColor, false }
    { "California",  0,  2, CRGB::Gold, CRGB::Green, false },
    { "Oregon",      3,  4, CRGB::Gold, CRGB::Green, false },
    { "Washington",  5,  6, CRGB::Gold, CRGB::Green, false },
    { "Nevada",      7,  8, CRGB::Gold, CRGB::Green, false },
    { "Arizona",     9, 10, CRGB::Gold, CRGB::Green, false },
    // ... add remaining states here
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

// Set to true by the web handler; consumed in loop() to trigger the flash.
static volatile bool newSubmission = false;

// ── Web server & DNS ──────────────────────────────────────────────────────────
AsyncWebServer server(80);
DNSServer      dnsServer;

// ── LED helpers ───────────────────────────────────────────────────────────────

// Redraw all map LEDs from current state (visited or not).
void refreshLEDs() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (uint8_t i = 0; i < NUM_STATES; i++) {
        CRGB color = states[i].visited ? states[i].visitedColor : states[i].defaultColor;
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

// Mark a state visited and update the strip immediately.
void markStateVisited(int idx) {
    if (idx < 0 || idx >= NUM_STATES) return;
    states[idx].visited = true;
    for (uint8_t j = states[idx].ledFirst; j <= states[idx].ledLast; j++) {
        leds[j] = states[idx].visitedColor;
    }
    FastLED.show();
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
        if (idx >= 0) markStateVisited(idx);

        newSubmission = true;

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
            states[i].visited = false;
        }
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

    // On a new submission: flash green 3 times, then resume heartbeat.
    if (newSubmission) {
        newSubmission = false;
        for (uint8_t i = 0; i < 3; i++) {
            onboardLed[0] = CRGB::Green;
            FastLED.show();
            delay(150);
            onboardLed[0] = CRGB::Black;
            FastLED.show();
            delay(150);
        }
    }

    // Slow blue heartbeat
    static uint32_t lastBlink = 0;
    static bool     ledState  = false;

    if (millis() - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = millis();
        ledState  = !ledState;
        onboardLed[0] = ledState ? CRGB::Blue : CRGB::Black;
        FastLED.show();
    }
}
