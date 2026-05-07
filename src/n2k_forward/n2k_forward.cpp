/**
 * n2k_forward.cpp
 *
 * Reads Modbus register values from reg_map and transmits corresponding
 * NMEA 2000 PGNs over the CAN bus at the configured interval.
 *
 * Each PGN is sent unconditionally (using N2kDoubleNA for fields that have
 * no corresponding register) so that NMEA 2000 displays always receive a
 * fresh value.  If the source slave data is not yet valid, transmission is
 * skipped entirely.
 */

#include "n2k_forward.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include "../canbus/canbus.h"
#include <N2kMessages.h>
#include <Arduino.h>

static uint32_t s_last_send = 0;
static uint8_t  s_seq       = 0;   // SID incremented with each transmission

bool n2k_forward_init()
{
    Serial.println("[N2K-FWD] N2K forwarding module ready");
    return true;
}

void n2k_forward_update()
{
    GatewayConfig &cfg = config_get();

    if (!cfg.n2k_enabled)       return;
    if (!canbus_is_ready())     return;

    uint32_t now = millis();
    if (now - s_last_send < cfg.n2k_interval_ms) return;
    s_last_send = now;

    uint8_t sid = cfg.n2k_src_slave;
    if (!reg_map_is_valid(sid)) return;

    tN2kMsg msg;

    // ------------------------------------------------------------------
    // PGN 127250 – Vessel Heading
    // Register holds heading in units of 0.1 degree (unsigned).
    // ------------------------------------------------------------------
    {
        uint16_t raw = reg_map_get(sid, cfg.n2k_heading_reg);
        double   deg = raw * 0.1;
        SetN2kHeading(msg, s_seq, DegToRad(deg),
                      N2kDoubleNA, N2kDoubleNA, N2khr_true);
        canbus_send(msg);
    }

    // ------------------------------------------------------------------
    // PGN 128267 – Water Depth
    // Register holds depth in cm; convert to metres.
    // ------------------------------------------------------------------
    {
        uint16_t raw   = reg_map_get(sid, cfg.n2k_depth_reg);
        double   depth = raw * 0.01;
        SetN2kWaterDepth(msg, s_seq, depth, 0.0, N2kDoubleNA);
        canbus_send(msg);
    }

    // ------------------------------------------------------------------
    // PGN 128259 – Boat Speed
    // Register holds speed in units of 0.01 knot.
    // ------------------------------------------------------------------
    {
        uint16_t raw   = reg_map_get(sid, cfg.n2k_speed_reg);
        double   ms    = KnotsToms(raw * 0.01);
        SetN2kBoatSpeed(msg, s_seq, ms, N2kDoubleNA, N2kSWRT_Paddle_wheel);
        canbus_send(msg);
    }

    // ------------------------------------------------------------------
    // PGN 129025 – Position Rapid Update
    // lat = lat_int_reg (signed int16) + lat_frac_reg / 10000.0
    // lon = lon_int_reg (signed int16) + lon_frac_reg / 10000.0
    // The sign of the fractional part follows the integer part.
    // ------------------------------------------------------------------
    {
        int16_t  lat_i = (int16_t)reg_map_get(sid, cfg.n2k_lat_int_reg);
        uint16_t lat_f = reg_map_get(sid, cfg.n2k_lat_frac_reg);
        int16_t  lon_i = (int16_t)reg_map_get(sid, cfg.n2k_lon_int_reg);
        uint16_t lon_f = reg_map_get(sid, cfg.n2k_lon_frac_reg);

        double sign_lat = (lat_i < 0) ? -1.0 : 1.0;
        double sign_lon = (lon_i < 0) ? -1.0 : 1.0;
        double lat = lat_i + sign_lat * (lat_f / 10000.0);
        double lon = lon_i + sign_lon * (lon_f / 10000.0);

        SetN2kPositionRapid(msg, lat, lon);
        canbus_send(msg);
    }

    s_seq = (s_seq + 1) & 0xFF;
}
