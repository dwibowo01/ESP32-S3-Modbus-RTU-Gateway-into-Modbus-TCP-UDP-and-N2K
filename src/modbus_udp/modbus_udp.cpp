/**
 * modbus_udp.cpp
 *
 * Broadcasts register data as JSON over UDP at the configured interval.
 * Only slaves that have at least one successful poll are included.
 */

#include "modbus_udp.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Arduino.h>

static WiFiUDP  s_udp;
static uint32_t s_last_broadcast = 0;

bool modbus_udp_init()
{
    GatewayConfig &cfg = config_get();
    s_udp.begin(cfg.udp_port);
    Serial.printf("[UDP] Broadcast on port %u every %u ms\n",
                  cfg.udp_port, cfg.udp_interval_ms);
    return true;
}

void modbus_udp_update()
{
    GatewayConfig &cfg = config_get();

    uint32_t now = millis();
    if (now - s_last_broadcast < cfg.udp_interval_ms) return;
    s_last_broadcast = now;

    // Build JSON document on the heap
    JsonDocument doc;
    doc["ts"] = now;
    JsonArray slaves = doc["slaves"].to<JsonArray>();

    for (uint8_t s = 0; s < cfg.rtu_slave_count; s++) {
        uint8_t sid = cfg.rtu_slave_ids[s];

        JsonObject obj  = slaves.add<JsonObject>();
        obj["id"]       = sid;
        obj["valid"]    = reg_map_is_valid(sid);

        JsonArray regs = obj["regs"].to<JsonArray>();
        for (uint16_t r = 0; r < cfg.rtu_reg_count; r++) {
            regs.add(reg_map_get(sid, r));
        }
    }

    // Serialise to a String and broadcast
    String payload;
    serializeJson(doc, payload);

    s_udp.beginPacket(IPAddress(255, 255, 255, 255), cfg.udp_port);
    s_udp.print(payload);
    s_udp.endPacket();

    Serial.printf("[UDP] Broadcast %u bytes\n", payload.length());
}
