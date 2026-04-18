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
#define DATA_PIN              16     // map LED strip (states)
#define DATA_PIN_REGIONS      5    // region LED strip (6 individual LEDs)
#define DATA_PIN_BANNER_TOP   18    // top banner LED strip
#define DATA_PIN_BANNER_BOT   17    // bottom banner LED strip
#define NUM_LEDS              153   // LEDs in the map strip
#define NUM_LEDS_REGIONS      6     // one LED per non-US region
#define NUM_LEDS_BANNER_TOP   100    // LEDs in the top banner
#define NUM_LEDS_BANNER_BOT   165    // LEDs in the bottom banner
#define LED_TYPE          WS2812B
#define COLOR_ORDER       GRB
#define BRIGHTNESS        200        // 0–255 (~25%)
#define STATUS_LED_PIN    2         // Built-in blue LED on ESP32-WROOM-32E
#define BLINK_INTERVAL_MS 500
#define HEAT_FLOOR        20        // minimum t value (0–255) for any state with ≥1 submission
                                    // keeps first-submission states off pure blue
#define BANNER_COLOR      CRGB::Red    // solid color for top and bottom banners

// Access Point credentials
static const char* AP_SSID     = "MapExplorer";
static const char* AP_PASSWORD = "";            // leave empty for open network

// Maximum number of visitor submissions to store in memory
static const uint16_t MAX_SUBMISSIONS = 10;
// ─────────────────────────────────────────────────────────────────────────────

// ── LED arrays ────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];
CRGB regionLeds[NUM_LEDS_REGIONS];
CRGB bannerTopLeds[NUM_LEDS_BANNER_TOP];
CRGB bannerBotLeds[NUM_LEDS_BANNER_BOT];

// ── State map ─────────────────────────────────────────────────────────────────
struct State {
    const char* name;
    uint8_t     ledFirst;        // first LED index in the strip (inclusive)
    uint8_t     ledLast;         // last  LED index in the strip (inclusive)
    CRGB*       strip;           // which LED array this entry writes to
    CRGB        idleColor;       // shown when count == 0
    uint16_t    count;           // number of submissions from this state/region
};

static State states[] = {
    // { "StateName", ledFirst, ledLast, strip, idleColor, count }
    { "Alabama",          32,  34, leds, CRGB::Black, 0 },
    { "Alaska",          146, 149, leds, CRGB::Black, 0 },
    { "Arizona",         114, 117, leds, CRGB::Black, 0 },
    { "Arkansas",         67,  69, leds, CRGB::Black, 0 },
    { "California",      137, 145, leds, CRGB::Black, 0 },
    { "Colorado",        106, 109, leds, CRGB::Black, 0 },
    { "Connecticut",       8,   8, leds, CRGB::Black, 0 },
    { "Delaware",          0,   0, leds, CRGB::Black, 0 },
    { "Florida",          28,  31, leds, CRGB::Black, 0 },
    { "Georgia",          25,  27, leds, CRGB::Black, 0 },
    { "Hawaii",          150, 152, leds, CRGB::Black, 0 },
    { "Idaho",           125, 129, leds, CRGB::Black, 0 },
    { "Illinois",         52,  54, leds, CRGB::Black, 0 },
    { "Indiana",          50,  51, leds, CRGB::Black, 0 },
    { "Iowa",             62,  63, leds, CRGB::Black, 0 },
    { "Kansas",           82,  84, leds, CRGB::Black, 0 },
    { "Kentucky",         41,  43, leds, CRGB::Black, 0 },
    { "Louisiana",        70,  71, leds, CRGB::Black, 0 },
    { "Maine",             3,   4, leds, CRGB::Black, 0 },
    { "Maryland",         13,  13, leds, CRGB::Black, 0 },
    { "Massachusetts",     7,   7, leds, CRGB::Black, 0 },
    { "Michigan",         46,  49, leds, CRGB::Black, 0 },
    { "Minnesota",        58,  61, leds, CRGB::Black, 0 },
    { "Mississippi",      35,  37, leds, CRGB::Black, 0 },
    { "Missouri",         64,  66, leds, CRGB::Black, 0 },
    { "Montana",          96, 101, leds, CRGB::Black, 0 },
    { "Nebraska",         85,  88, leds, CRGB::Black, 0 },
    { "Nevada",          121, 124, leds, CRGB::Black, 0 },
    { "New Hampshire",     5,   5, leds, CRGB::Black, 0 },
    { "New Jersey",       12,  12, leds, CRGB::Black, 0 },
    { "New Mexico",      110, 113, leds, CRGB::Black, 0 },
    { "New York",          9,  11, leds, CRGB::Black, 0 },
    { "North Carolina",   20,  22, leds, CRGB::Black, 0 },
    { "North Dakota",     93,  95, leds, CRGB::Black, 0 },
    { "Ohio",             44,  45, leds, CRGB::Black, 0 },
    { "Oklahoma",         79,  81, leds, CRGB::Black, 0 },
    { "Oregon",          133, 136, leds, CRGB::Black, 0 },
    { "Pennsylvania",     14,  16, leds, CRGB::Black, 0 },
    { "Rhode Island",      1,   2, leds, CRGB::Black, 0 },
    { "South Carolina",   23,  24, leds, CRGB::Black, 0 },
    { "South Dakota",     89,  92, leds, CRGB::Black, 0 },
    { "Tennessee",        38,  40, leds, CRGB::Black, 0 },
    { "Texas",            72,  78, leds, CRGB::Black, 0 },
    { "Utah",            118, 120, leds, CRGB::Black, 0 },
    { "Vermont",           6,   6, leds, CRGB::Black, 0 },
    { "Virginia",         18,  19, leds, CRGB::Black, 0 },
    { "Washington",      130, 132, leds, CRGB::Black, 0 },
    { "West Virginia",    17,  17, leds, CRGB::Black, 0 },
    { "Wisconsin",        55,  57, leds, CRGB::Black, 0 },
    { "Wyoming",         102, 105, leds, CRGB::Black, 0 },
};

static const uint8_t NUM_STATES = sizeof(states) / sizeof(states[0]);

// ── Region LEDs (one LED each on their own strip, GPIO DATA_PIN_REGIONS) ──────
// These share the same State struct and heatmap scheme as US states.
// "country" form field value must match name exactly (case-insensitive).
static State regions[] = {
    // { "RegionName", ledIndex, ledIndex, strip, idleColor, count }
    { "Canada",    0, 0, regionLeds, CRGB::Black, 0 },
    { "Mexico",    1, 1, regionLeds, CRGB::Black, 0 },
    { "Europe",    2, 2, regionLeds, CRGB::Black, 0 },
    { "Asia",      3, 3, regionLeds, CRGB::Black, 0 },
    { "Africa",    4, 4, regionLeds, CRGB::Black, 0 },
    { "Australia", 5, 5, regionLeds, CRGB::Black, 0 },
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

    // Scale relative to the current leader so the top state is always full red,
    // and all others sit proportionally below it on the gradient.
    uint16_t maxCount = (leadingEntry && leadingEntry->count > 0) ? leadingEntry->count : count;

    // Map to HEAT_FLOOR–255 so even a single submission shows noticeable color
    uint8_t t = (uint8_t)(HEAT_FLOOR + (uint32_t)(count) * (255 - HEAT_FLOOR) / maxCount);

    // Four equal segments across five stops
    static const CRGB stops[5] = {
        CRGB(  0,   0, 255),   // blue
        CRGB(  0, 255, 255),   // cyan
        CRGB(  0, 255,   0),   // green
        CRGB(255, 255,   0),   // yellow
        CRGB(255,   0,   0),   // red
    };
    // t=255 → seg=3, frac=252 via integer division — close but not quite stops[4].
    // Treat t>=252 (top ~1%) as pure red so the leader always lands on solid red.
    if (t >= 252) return stops[4];
    uint8_t seg  = t / 64;             // 0–3
    uint8_t frac = (uint8_t)((t % 64) * 4);  // 0–252 within segment
    return blend(stops[seg], stops[seg + 1], frac);
}

// Apply heatmap color to a single entry's LED range on its own strip.
static void applyEntryColor(State& e) {
    CRGB color = (e.count == 0) ? e.idleColor : heatmapColor(e.count);
    for (uint8_t j = e.ledFirst; j <= e.ledLast; j++)
        e.strip[j] = color;
}

// Redraw all map and region LEDs from current counts.
void refreshLEDs() {
    fill_solid(leds,       NUM_LEDS,         CRGB::Black);
    fill_solid(regionLeds, NUM_LEDS_REGIONS, CRGB::Black);
    for (uint8_t i = 0; i < NUM_STATES;  i++) applyEntryColor(states[i]);
    for (uint8_t i = 0; i < NUM_REGIONS; i++) applyEntryColor(regions[i]);
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

// Increment a state's count, persist it, recompute the leader, then refresh its LEDs.
// leadingEntry must be updated first so heatmapColor() scales against the correct max.
void incrementState(int idx) {
    if (idx < 0 || idx >= NUM_STATES) return;
    states[idx].count++;
    saveCount('s', idx, states[idx].count);
    leadingEntry = findLeadingEntry();   // update leader BEFORE computing color
    applyEntryColor(states[idx]);
    FastLED.show();
}

// Increment a region's count, persist it, recompute the leader, then refresh its LED.
void incrementRegion(int idx) {
    if (idx < 0 || idx >= NUM_REGIONS) return;
    regions[idx].count++;
    saveCount('r', idx, regions[idx].count);
    leadingEntry = findLeadingEntry();   // update leader BEFORE computing color
    applyEntryColor(regions[idx]);
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

    // Top-5 endpoint — returns the 5 states/regions with the highest counts
    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest* req) {
        // Track which indices have already been selected
        bool usedState[NUM_STATES]  = {};
        bool usedRegion[NUM_REGIONS] = {};

        String json = "{\"top\":[";
        bool first = true;

        for (uint8_t rank = 0; rank < 5; rank++) {
            uint16_t    best          = 0;
            int         bestIdx       = -1;
            bool        bestIsRegion  = false;

            for (uint8_t i = 0; i < NUM_STATES; i++) {
                if (!usedState[i] && states[i].count > best) {
                    best = states[i].count; bestIdx = i; bestIsRegion = false;
                }
            }
            for (uint8_t i = 0; i < NUM_REGIONS; i++) {
                if (!usedRegion[i] && regions[i].count > best) {
                    best = regions[i].count; bestIdx = i; bestIsRegion = true;
                }
            }

            if (bestIdx < 0 || best == 0) break;

            if (!first) json += ',';
            first = false;
            json += "{\"name\":\"";
            json += bestIsRegion ? regions[bestIdx].name : states[bestIdx].name;
            json += "\",\"count\":";
            json += best;
            json += "}";

            if (bestIsRegion) usedRegion[bestIdx] = true;
            else              usedState[bestIdx]  = true;
        }

        json += "]}";
        req->send(200, "application/json", json);
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

    // Map LED strip (states)
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
    // Region LED strip
    FastLED.addLeds<LED_TYPE, DATA_PIN_REGIONS, COLOR_ORDER>(regionLeds, NUM_LEDS_REGIONS)
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

    // ── New submission: blink the state LEDs green 3×, then chase the banners ──
    if (newSubmission) {
        newSubmission = false;
        State* entry = (State*)newSubmissionEntry;
        newSubmissionEntry = nullptr;

        // Step 1 — blink the submitted state/region LEDs green 3×
        for (uint8_t i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            if (entry) {
                for (uint8_t j = entry->ledFirst; j <= entry->ledLast; j++)
                    entry->strip[j] = CRGB::Green;
            }
            FastLED.show();
            delay(150);

            digitalWrite(STATUS_LED_PIN, LOW);
            if (entry) {
                for (uint8_t j = entry->ledFirst; j <= entry->ledLast; j++)
                    entry->strip[j] = CRGB::Black;
            }
            FastLED.show();
            delay(150);
        }

        // Step 2 — chasing-comet animation on both banner strips
        // A bright white head with a fading tail sweeps from end to end twice.
        static const uint8_t CHASE_TRAIL  = 12;   // tail length in LEDs
        static const uint8_t CHASE_PASSES = 2;    // how many full sweeps
        static const uint8_t CHASE_DELAY  = 5;    // ms per frame (~200 fps cap)
        const int chaseLen = max((int)NUM_LEDS_BANNER_TOP, (int)NUM_LEDS_BANNER_BOT)
                             + CHASE_TRAIL;        // travel far enough to clear the strip

        for (uint8_t pass = 0; pass < CHASE_PASSES; pass++) {
            for (int pos = 0; pos < chaseLen; pos++) {
                fill_solid(bannerTopLeds, NUM_LEDS_BANNER_TOP, CRGB::Black);
                fill_solid(bannerBotLeds, NUM_LEDS_BANNER_BOT, CRGB::Black);

                for (uint8_t t = 0; t < CHASE_TRAIL; t++) {
                    int idx = pos - (int)t;
                    // Brightness fades linearly from 255 at the head to 0 at the tail
                    uint8_t brightness = (uint8_t)((uint16_t)(CHASE_TRAIL - t) * 255 / CHASE_TRAIL);
                    CRGB dot(brightness, brightness, brightness);
                    if (idx >= 0 && idx < NUM_LEDS_BANNER_TOP) bannerTopLeds[idx] = dot;
                    if (idx >= 0 && idx < NUM_LEDS_BANNER_BOT) bannerBotLeds[idx] = dot;
                }

                FastLED.show();
                delay(CHASE_DELAY);
            }
        }

        // Step 3 — restore banners and repaint the map with updated heatmap colors
        fill_solid(bannerTopLeds, NUM_LEDS_BANNER_TOP, BANNER_COLOR);
        fill_solid(bannerBotLeds, NUM_LEDS_BANNER_BOT, BANNER_COLOR);
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
                le->strip[j] = color;
            dirty = true;
        }
        if (dirty) FastLED.show();

        // Banners disabled — uncomment to re-enable
         fill_solid(bannerTopLeds, NUM_LEDS_BANNER_TOP, BANNER_COLOR);
         fill_solid(bannerBotLeds, NUM_LEDS_BANNER_BOT, BANNER_COLOR);
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
