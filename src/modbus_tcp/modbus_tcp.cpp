/**
 * modbus_tcp.cpp
 *
 * Modbus TCP server – read-only gateway exposing the shared register map.
 *
 * Frame layout (RFC 2543 / Modbus spec)
 * --------------------------------------
 * MBAP header (6 bytes):
 *   [0-1]  Transaction Identifier  (echoed back)
 *   [2-3]  Protocol Identifier     (0x0000)
 *   [4-5]  Length                  (bytes that follow, i.e. Unit + PDU)
 * PDU:
 *   [6]    Unit Identifier         (slave address / routing)
 *   [7]    Function Code
 *   [8-N]  Function-specific data
 */

#include "modbus_tcp.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include <WiFi.h>
#include <WiFiServer.h>
#include <Arduino.h>

#define MAX_TCP_CLIENTS  4
#define MBAP_SIZE        6    // bytes in the MBAP header

// Modbus function codes handled
#define FC_READ_HOLDING  0x03
#define FC_READ_INPUT    0x04

// Modbus exception codes
#define EX_ILLEGAL_FUNC  0x01
#define EX_ILLEGAL_ADDR  0x02
#define EX_ILLEGAL_VAL   0x03

static WiFiServer *s_server  = nullptr;
static WiFiClient  s_clients[MAX_TCP_CLIENTS];

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Send a Modbus exception response. */
static void send_exception(WiFiClient &client,
                           uint16_t trans_id,
                           uint8_t  unit_id,
                           uint8_t  fc,
                           uint8_t  exc_code)
{
    uint8_t resp[9];
    resp[0] = (trans_id >> 8) & 0xFF;
    resp[1] =  trans_id       & 0xFF;
    resp[2] = 0x00;
    resp[3] = 0x00;               // Protocol ID
    resp[4] = 0x00;
    resp[5] = 0x03;               // Remaining length: unit(1)+fc(1)+exc(1)
    resp[6] = unit_id;
    resp[7] = fc | 0x80;          // Error flag
    resp[8] = exc_code;
    client.write(resp, sizeof(resp));
}

/** Handle one pending request from a connected client. */
static void service_client(WiFiClient &client)
{
    if (!client.available()) return;

    // Read MBAP header
    uint8_t hdr[MBAP_SIZE];
    if (client.readBytes(hdr, MBAP_SIZE) < MBAP_SIZE) return;

    uint16_t trans_id = ((uint16_t)hdr[0] << 8) | hdr[1];
    // hdr[2-3] = protocol id (ignored)
    uint16_t pdu_len  = ((uint16_t)hdr[4] << 8) | hdr[5];

    if (pdu_len < 2 || pdu_len > 253) return;   // sanity check

    // Read the PDU (Unit ID + FC + data)
    uint8_t pdu[253];
    if (client.readBytes(pdu, pdu_len) < (int)pdu_len) return;

    uint8_t unit_id = pdu[0];
    uint8_t fc      = pdu[1];

    if (fc != FC_READ_HOLDING && fc != FC_READ_INPUT) {
        send_exception(client, trans_id, unit_id, fc, EX_ILLEGAL_FUNC);
        return;
    }

    // FC03/FC04 request body: start address (2), quantity (2)
    if (pdu_len < 6) {
        send_exception(client, trans_id, unit_id, fc, EX_ILLEGAL_VAL);
        return;
    }

    uint16_t addr  = ((uint16_t)pdu[2] << 8) | pdu[3];
    uint16_t count = ((uint16_t)pdu[4] << 8) | pdu[5];

    if (count == 0 || count > 125) {
        send_exception(client, trans_id, unit_id, fc, EX_ILLEGAL_VAL);
        return;
    }

    // Resolve slave_id and reg_start from unit_id and addr
    uint8_t  slave_id;
    uint16_t reg_start;

    if (unit_id >= 1 && unit_id <= GATEWAY_MAX_SLAVES) {
        // Unit ID encodes the slave directly; addr is the register offset
        slave_id  = unit_id;
        reg_start = addr;
    } else {
        // Flat address space: addr = (slave-1)*GATEWAY_REGS_PER_SLAVE + reg
        slave_id  = (uint8_t)(addr / GATEWAY_REGS_PER_SLAVE) + 1;
        reg_start = addr % GATEWAY_REGS_PER_SLAVE;
    }

    if (slave_id > GATEWAY_MAX_SLAVES ||
        (uint32_t)reg_start + count > GATEWAY_REGS_PER_SLAVE) {
        send_exception(client, trans_id, unit_id, fc, EX_ILLEGAL_ADDR);
        return;
    }

    // Build FC03/04 normal response
    // MBAP(6) + unit(1) + fc(1) + byte_count(1) + data(2*count)
    uint16_t resp_data_bytes = 2 * count;
    uint16_t mbap_len_field  = 3 + resp_data_bytes; // unit+fc+bytecount+data
    uint16_t total            = MBAP_SIZE + 1 + 1 + 1 + resp_data_bytes;

    uint8_t resp[MBAP_SIZE + 3 + 125 * 2]; // max 259 bytes
    resp[0] = (trans_id >> 8) & 0xFF;
    resp[1] =  trans_id       & 0xFF;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = (mbap_len_field >> 8) & 0xFF;
    resp[5] =  mbap_len_field       & 0xFF;
    resp[6] = unit_id;
    resp[7] = fc;
    resp[8] = (uint8_t)resp_data_bytes;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val = reg_map_get(slave_id, reg_start + i);
        resp[9 + 2 * i]     = (val >> 8) & 0xFF;
        resp[9 + 2 * i + 1] =  val       & 0xFF;
    }

    client.write(resp, total);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool modbus_tcp_init()
{
    GatewayConfig &cfg = config_get();
    s_server = new WiFiServer(cfg.tcp_port);
    s_server->begin();
    s_server->setNoDelay(true);
    Serial.printf("[TCP] Modbus TCP server listening on port %u\n", cfg.tcp_port);
    return true;
}

void modbus_tcp_update()
{
    if (!s_server) return;

    // Accept new connections
    if (s_server->hasClient()) {
        WiFiClient incoming = s_server->accept();
        bool placed = false;
        for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
            if (!s_clients[i] || !s_clients[i].connected()) {
                s_clients[i] = incoming;
                placed = true;
                Serial.printf("[TCP] Client %d connected from %s\n",
                              i, incoming.remoteIP().toString().c_str());
                break;
            }
        }
        if (!placed) {
            incoming.stop();
            Serial.println("[TCP] Max clients reached; connection rejected");
        }
    }

    // Service existing clients
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (s_clients[i]) {
            if (s_clients[i].connected()) {
                service_client(s_clients[i]);
            } else {
                s_clients[i].stop();
            }
        }
    }
}
