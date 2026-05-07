/**
 * config.cpp
 *
 * Uses the ESP32 Preferences library (NVS) to persist a single binary blob
 * containing the entire GatewayConfig struct.  If the stored blob size does
 * not match sizeof(GatewayConfig) (e.g. first boot or firmware upgrade that
 * changed the struct layout), factory defaults are used instead.
 */

#include "config.h"
#include <Preferences.h>
#include <string.h>

static GatewayConfig s_cfg;
static Preferences   s_prefs;

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

void config_reset_defaults()
{
    memset(&s_cfg, 0, sizeof(s_cfg));

    // WiFi – blank SSID triggers AP-mode fallback in wifi_mgr
    s_cfg.wifi_ssid[0] = '\0';
    s_cfg.wifi_pass[0] = '\0';

    // Modbus RTU
    s_cfg.rtu_baud          = 9600;
    s_cfg.rtu_tx_pin        = 17;
    s_cfg.rtu_rx_pin        = 18;
    s_cfg.rtu_de_pin        = 16;
    s_cfg.rtu_slave_count   = 1;
    s_cfg.rtu_slave_ids[0]  = 1;
    s_cfg.rtu_reg_start     = 0;
    s_cfg.rtu_reg_count     = 16;
    s_cfg.rtu_poll_ms       = 1000;

    // Modbus TCP
    s_cfg.tcp_port          = 502;

    // UDP broadcast
    s_cfg.udp_port          = 1502;
    s_cfg.udp_interval_ms   = 2000;

    // N2K forwarding
    s_cfg.n2k_enabled       = true;
    s_cfg.n2k_src_slave     = 1;
    s_cfg.n2k_heading_reg   = 0;
    s_cfg.n2k_depth_reg     = 1;
    s_cfg.n2k_speed_reg     = 2;
    s_cfg.n2k_lat_int_reg   = 3;
    s_cfg.n2k_lat_frac_reg  = 4;
    s_cfg.n2k_lon_int_reg   = 5;
    s_cfg.n2k_lon_frac_reg  = 6;
    s_cfg.n2k_interval_ms   = 1000;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void config_init()
{
    config_reset_defaults();
}

void config_load()
{
    s_prefs.begin("gateway", /*readOnly=*/true);
    size_t stored = s_prefs.getBytesLength("cfg");
    if (stored == sizeof(GatewayConfig)) {
        s_prefs.getBytes("cfg", &s_cfg, sizeof(GatewayConfig));
        Serial.println("[CFG] Loaded config from NVS");
    } else {
        Serial.printf("[CFG] No valid config in NVS (stored=%u expected=%u); using defaults\n",
                      stored, sizeof(GatewayConfig));
    }
    s_prefs.end();
}

void config_save()
{
    s_prefs.begin("gateway", /*readOnly=*/false);
    s_prefs.putBytes("cfg", &s_cfg, sizeof(GatewayConfig));
    s_prefs.end();
    Serial.println("[CFG] Config saved to NVS");
}

GatewayConfig &config_get()
{
    return s_cfg;
}
