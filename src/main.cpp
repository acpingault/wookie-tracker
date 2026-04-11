#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "web_content.h"

static Preferences prefs;
#define PREFS_NS  "wookie"       // NVS namespace for counts
#define SUBS_FILE "/subs.csv"    // LittleFS path for submission log

// ── Configuration ─────────────────────────────────────────────────────────────
#define DATA_PIN              5     // map LED strip
#define DATA_PIN_BANNER_TOP   16    // top banner LED strip
#define DATA_PIN_BANNER_BOT   17    // bottom banner LED strip
#define NUM_LEDS              106   // 100 state LEDs (0–99) + 6 region LEDs (100–105)
#define NUM_LEDS_BANNER_TOP   30    // LEDs in the top banner
#define NUM_LEDS_BANNER_BOT   30    // LEDs in the bottom banner
#define LED_TYPE          WS2812B
#define COLOR_ORDER       GRB
#define BRIGHTNESS        64        // 0–255
#define STATUS_LED_PIN    2         // Built-in blue LED on ESP32-WROOM-32E
#define BLINK_INTERVAL_MS 500
#define HEAT_TOP_PCT      15        // % of total submissions that pegs peak "hot" red
#define SHIMMER_COLOR     CRGB::White  // base color for banner shimmer
#define SHIMMER_SPEED     3            // higher = faster shimmer
#define SHIMMER_MIN_BRI   40           // minimum brightness (0–255); keeps LEDs from going dark

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

// ── Region LEDs (individual LEDs, indices 100–105 on the map strip) ───────────
// These share the same State struct and heatmap scheme as US states.
// "country" form field value must match name exactly (case-insensitive).
static State regions[] = {
    // { "RegionName", ledIndex, ledIndex, idleColor, count }
    { "Canada",    100, 100, CRGB::DimGray, 0 },
    { "Mexico",    101, 101, CRGB::DimGray, 0 },
    { "Europe",    102, 102, CRGB::DimGray, 0 },
    { "Asia",      103, 103, CRGB::DimGray, 0 },
    { "Africa",    104, 104, CRGB::DimGray, 0 },
    { "Australia", 105, 105, CRGB::DimGray, 0 },
};

static const uint8_t NUM_REGIONS = sizeof(regions) / sizeof(regions[0]);

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
static volatile State*  newSubmissionEntry = nullptr; // points into states[] or regions[]

// Pointer to the state/region with the highest submission count (nullptr if all zero).
static State* leadingEntry = nullptr;

// ── Web server & DNS ──────────────────────────────────────────────────────────
AsyncWebServer server(80);
DNSServer      dnsServer;

// Forward declaration — defined in the Persistence section below.
void saveCount(char prefix, uint8_t idx, uint16_t count);

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

    // Threshold = 15% of total submissions, minimum 1 to avoid divide-by-zero
    uint16_t threshold = (uint16_t)(submissionCount * HEAT_TOP_PCT / 100);
    if (threshold < 1) threshold = 1;

    // Clamp to [1, threshold] then map to 0–255
    uint16_t clamped = count > threshold ? threshold : count;
    uint8_t  t       = (uint8_t)((uint32_t)(clamped - 1) * 255 / (threshold - 1 ? threshold - 1 : 1));

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

// Apply heatmap color to a single entry's LED range and push to strip.
static void applyEntryColor(State& e) {
    CRGB color = (e.count == 0) ? e.idleColor : heatmapColor(e.count);
    for (uint8_t j = e.ledFirst; j <= e.ledLast; j++)
        leds[j] = color;
}

// Redraw all map LEDs (states + regions) from current counts.
void refreshLEDs() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (uint8_t i = 0; i < NUM_STATES;   i++) applyEntryColor(states[i]);
    for (uint8_t i = 0; i < NUM_REGIONS;  i++) applyEntryColor(regions[i]);
    FastLED.show();
}

// Returns pointer to the entry (state or region) with the highest count, or
// nullptr if all counts are zero.
State* findLeadingEntry() {
    State*   best      = nullptr;
    uint16_t bestCount = 0;
    for (uint8_t i = 0; i < NUM_STATES;  i++) {
        if (states[i].count  > bestCount) { bestCount = states[i].count;  best = &states[i]; }
    }
    for (uint8_t i = 0; i < NUM_REGIONS; i++) {
        if (regions[i].count > bestCount) { bestCount = regions[i].count; best = &regions[i]; }
    }
    return best;
}

// Case-insensitive search helpers.
int findState(const char* name) {
    for (uint8_t i = 0; i < NUM_STATES;  i++)
        if (strcasecmp(states[i].name,  name) == 0) return i;
    return -1;
}
int findRegion(const char* name) {
    for (uint8_t i = 0; i < NUM_REGIONS; i++)
        if (strcasecmp(regions[i].name, name) == 0) return i;
    return -1;
}

// Increment a state's count, persist it, refresh its LEDs, recompute the leader.
void incrementState(int idx) {
    if (idx < 0 || idx >= NUM_STATES) return;
    states[idx].count++;
    saveCount('s', idx, states[idx].count);
    applyEntryColor(states[idx]);
    FastLED.show();
    leadingEntry = findLeadingEntry();
}

// Increment a region's count, persist it, refresh its LED, recompute the leader.
void incrementRegion(int idx) {
    if (idx < 0 || idx >= NUM_REGIONS) return;
    regions[idx].count++;
    saveCount('r', idx, regions[idx].count);
    applyEntryColor(regions[idx]);
    FastLED.show();
    leadingEntry = findLeadingEntry();
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

// ── Persistence ───────────────────────────────────────────────────────────────

// Write a single count to NVS. Key format: "s0"–"s49" for states,
// "r0"–"r5" for regions.
void saveCount(char prefix, uint8_t idx, uint16_t count) {
    char key[5];
    snprintf(key, sizeof(key), "%c%u", prefix, idx);
    prefs.begin(PREFS_NS, false);
    prefs.putUShort(key, count);
    prefs.end();
}

// Read all counts from NVS into states[] and regions[] on boot.
void loadCounts() {
    prefs.begin(PREFS_NS, true);
    char key[5];
    for (uint8_t i = 0; i < NUM_STATES;  i++) {
        snprintf(key, sizeof(key), "s%u", i);
        states[i].count  = prefs.getUShort(key, 0);
    }
    for (uint8_t i = 0; i < NUM_REGIONS; i++) {
        snprintf(key, sizeof(key), "r%u", i);
        regions[i].count = prefs.getUShort(key, 0);
    }
    prefs.end();
    leadingEntry = findLeadingEntry();
}

// Append one submission line to the log file.
void appendSubmission(const char* state, const char* country) {
    File f = LittleFS.open(SUBS_FILE, "a");
    if (!f) { Serial.println("LittleFS: failed to open subs file for append"); return; }
    f.printf("%s,%s\n", state, country);
    f.close();
}

// Load persisted submissions from the log file into the submissions[] array.
void loadSubmissions() {
    if (!LittleFS.exists(SUBS_FILE)) return;
    File f = LittleFS.open(SUBS_FILE, "r");
    if (!f) { Serial.println("LittleFS: failed to open subs file for read"); return; }
    while (f.available() && submissionCount < MAX_SUBMISSIONS) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;
        int comma = line.indexOf(',');
        if (comma < 0) continue;
        String s = line.substring(0, comma);
        String c = line.substring(comma + 1);
        strncpy(submissions[submissionCount].state,   s.c_str(), 63);
        strncpy(submissions[submissionCount].country, c.c_str(), 63);
        submissions[submissionCount].state[63]   = '\0';
        submissions[submissionCount].country[63] = '\0';
        submissionCount++;
    }
    f.close();
    Serial.printf("Loaded %d submission(s) from flash\n", submissionCount);
}

// Erase all persisted data (called by admin clear).
void clearPersistence() {
    prefs.begin(PREFS_NS, false);
    prefs.clear();
    prefs.end();
    if (LittleFS.exists(SUBS_FILE)) LittleFS.remove(SUBS_FILE);
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

        // Update LEDs — US state on the map strip, or a region LED
        int stateIdx  = findState(state.c_str());
        int regionIdx = findRegion(country.c_str());

        if (stateIdx  >= 0) incrementState(stateIdx);
        if (regionIdx >= 0) incrementRegion(regionIdx);

        // Persist the submission to flash
        appendSubmission(state.c_str(), country.c_str());

        // Tell loop() which entry to blink (state takes priority if both matched)
        if      (stateIdx  >= 0) newSubmissionEntry = &states[stateIdx];
        else if (regionIdx >= 0) newSubmissionEntry = &regions[regionIdx];
        else                     newSubmissionEntry = nullptr;
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
        for (uint8_t i = 0; i < NUM_STATES;  i++) states[i].count  = 0;
        for (uint8_t i = 0; i < NUM_REGIONS; i++) regions[i].count = 0;
        leadingEntry = nullptr;
        clearPersistence();
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

    // Mount LittleFS; format partition on first use if needed
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed — filesystem will not persist");
    }

    // Restore counts and submission log from flash
    loadCounts();
    loadSubmissions();

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

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

    // ── New submission: blink the entry's LEDs + onboard LED green 3 times ──
    if (newSubmission) {
        newSubmission = false;
        State* entry = (State*)newSubmissionEntry;
        newSubmissionEntry = nullptr;

        for (uint8_t i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            if (entry) {
                for (uint8_t j = entry->ledFirst; j <= entry->ledLast; j++)
                    leds[j] = CRGB::Green;
            }
            FastLED.show();
            delay(150);

            digitalWrite(STATUS_LED_PIN, LOW);
            if (entry) {
                for (uint8_t j = entry->ledFirst; j <= entry->ledLast; j++)
                    leds[j] = CRGB::Black;
            }
            FastLED.show();
            delay(150);
        }

        refreshLEDs();
    }

    // ── Pulse the leading state/region ───────────────────────────────────────
    // sin8 input wraps every 256 counts; >> 3 ≈ 2 s period. Level stays in
    // the upper half (128–255) so the heatmap color stays recognisable.
    static uint32_t lastPulse = 0;
    if (millis() - lastPulse >= 20) {   // ~50 fps
        lastPulse = millis();
        bool dirty = false;

        // Pulse the leading state/region
        State* le = leadingEntry;
        if (le && le->count > 0) {
            uint8_t level = 128 + scale8(sin8((uint8_t)(millis() >> 3)), 127);
            CRGB    color = heatmapColor(le->count);
            color.nscale8(level);
            for (uint8_t j = le->ledFirst; j <= le->ledLast; j++)
                leds[j] = color;
            dirty = true;
        }
        if (dirty) FastLED.show();

        // Shimmer the banners using Perlin noise — each LED gets a smooth
        // organic brightness driven by its position and the current time.
        uint16_t t = (uint16_t)(millis() / SHIMMER_SPEED);
        for (uint8_t i = 0; i < NUM_LEDS_BANNER_TOP; i++) {
            uint8_t bri = scale8(inoise8(i * 40, t), 255 - SHIMMER_MIN_BRI) + SHIMMER_MIN_BRI;
            CRGB c = SHIMMER_COLOR;
            c.nscale8(bri);
            bannerTopLeds[i] = c;
        }
        for (uint8_t i = 0; i < NUM_LEDS_BANNER_BOT; i++) {
            // Offset the noise by a large prime so top and bottom shimmer independently
            uint8_t bri = scale8(inoise8(i * 40 + 10000, t), 255 - SHIMMER_MIN_BRI) + SHIMMER_MIN_BRI;
            CRGB c = SHIMMER_COLOR;
            c.nscale8(bri);
            bannerBotLeds[i] = c;
        }
        FastLED.show();
    }

    // ── Slow blue heartbeat on onboard LED ───────────────────────────────────
    static uint32_t lastBlink = 0;
    static bool     ledState  = false;

    if (millis() - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = millis();
        ledState  = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
    }
}
