/**
 * wifi_mgr.cpp
 *
 * Connects to the configured WiFi network in STA mode.  Falls back to
 * soft-AP mode (SSID "ESP32-Gateway", password "config1234") when
 * credentials are absent or the connection times out.  The soft-AP lets
 * users reach the web configuration UI at 192.168.4.1.
 */

#include "wifi_mgr.h"
#include "../config/config.h"
#include <WiFi.h>

// How long to wait for a STA connection before giving up (ms)
#define WIFI_CONNECT_TIMEOUT_MS 10000

// Fallback soft-AP credentials
#define AP_SSID "ESP32-Gateway"
#define AP_PASS "config1234"

static bool s_ap_mode = false;

bool wifi_init()
{
    GatewayConfig &cfg = config_get();

    if (strlen(cfg.wifi_ssid) == 0) {
        Serial.println("[WIFI] No SSID configured – starting soft-AP");
        goto start_ap;
    }

    Serial.printf("[WIFI] Connecting to '%s' ...\n", cfg.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    {
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_ap_mode = false;
        Serial.printf("[WIFI] Connected  IP: %s\n",
                      WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WIFI] STA connection failed – falling back to soft-AP");

start_ap:
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    s_ap_mode = true;
    Serial.printf("[WIFI] Soft-AP '%s' started  IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
    return true;
}

bool wifi_is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_is_ap_mode()
{
    return s_ap_mode;
}

String wifi_get_ip()
{
    if (s_ap_mode) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}
