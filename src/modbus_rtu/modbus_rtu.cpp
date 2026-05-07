/**
 * modbus_rtu.cpp
 *
 * Modbus RTU master implementation.
 *
 * Polling strategy
 * ----------------
 * Each call to modbus_rtu_update() drives the library task loop.  When no
 * transaction is in-flight and the poll interval has elapsed, we queue a
 * FC03 (Read Holding Registers) request for the next slave in round-robin
 * order.  The response callback writes the register values into reg_map.
 *
 * Address mapping
 * ---------------
 * rtu_reg_start  – first Modbus register address polled from every slave
 * rtu_reg_count  – how many consecutive registers to read
 * The results are stored at reg_map indices 0 .. (rtu_reg_count-1).
 */

#include "modbus_rtu.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include "../defs.h"
#include <ModbusRTU.h>
#include <Arduino.h>

static ModbusRTU s_rtu;

// Receive buffer – must remain valid until the callback fires
static uint16_t s_buf[GATEWAY_REGS_PER_SLAVE];

static bool     s_pending    = false;
static uint8_t  s_slave_idx  = 0;   // index into cfg.rtu_slave_ids[]
static uint8_t  s_cur_slave  = 1;   // current slave address being polled
static uint32_t s_last_poll  = 0;

// ---------------------------------------------------------------------------
// RTU response callback
// ---------------------------------------------------------------------------

static bool on_rtu_data(Modbus::ResultCode result, uint16_t /*trans_id*/,
                        void * /*data*/)
{
    // 'data' is the same pointer as s_buf passed to readHreg(); the library
    // has already filled it with the received register values.
    GatewayConfig &cfg = config_get();

    if (result == Modbus::EX_SUCCESS) {
        uint16_t count = min((uint16_t)cfg.rtu_reg_count,
                             (uint16_t)GATEWAY_REGS_PER_SLAVE);
        // Store results at 0-based indices 0..count-1 in the slave's block.
        // rtu_reg_start is only the Modbus address sent on the wire; the
        // reg_map slot is always 0-based.
        for (uint16_t i = 0; i < count; i++) {
            reg_map_set(s_cur_slave, i, s_buf[i]);
        }
        reg_map_mark_valid(s_cur_slave);
        Serial.printf("[RTU] Slave %u: %u regs from addr %u OK\n",
                      s_cur_slave, count, cfg.rtu_reg_start);
    } else {
        reg_map_invalidate(s_cur_slave);
        Serial.printf("[RTU] Slave %u: error 0x%02X\n",
                      s_cur_slave, (uint8_t)result);
    }

    s_pending = false;
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool modbus_rtu_init()
{
    GatewayConfig &cfg = config_get();

    if (cfg.rs485_mode != RS485_MODE_MODBUS_RTU) {
        Serial.println("[RTU] Skipped – RS-485 mode is NMEA 0183");
        return false;
    }

    Serial1.begin(cfg.rtu_baud, SERIAL_8N1,
                  cfg.rtu_rx_pin, cfg.rtu_tx_pin);

    s_rtu.begin(&Serial1, cfg.rtu_de_pin);
    s_rtu.master();

    s_slave_idx = 0;
    s_pending   = false;
    s_last_poll = 0;

    Serial.printf("[RTU] Init  baud=%u  TX=GPIO%u  RX=GPIO%u  DE=GPIO%u\n",
                  cfg.rtu_baud, cfg.rtu_tx_pin,
                  cfg.rtu_rx_pin, cfg.rtu_de_pin);
    return true;
}

void modbus_rtu_update()
{
    GatewayConfig &cfg = config_get();
    if (cfg.rs485_mode != RS485_MODE_MODBUS_RTU) return;

    // Always drive the library state machine first
    s_rtu.task();

    if (s_pending) return;

    if (cfg.rtu_slave_count == 0) return;

    uint32_t now = millis();
    if (now - s_last_poll < cfg.rtu_poll_ms) return;

    // Advance to next slave in round-robin
    s_slave_idx = (s_slave_idx + 1) % cfg.rtu_slave_count;
    s_cur_slave = cfg.rtu_slave_ids[s_slave_idx];

    uint16_t count = min((uint16_t)cfg.rtu_reg_count,
                         (uint16_t)GATEWAY_REGS_PER_SLAVE);

    if (s_rtu.readHreg(s_cur_slave, cfg.rtu_reg_start,
                       s_buf, count, on_rtu_data)) {
        s_pending   = true;
        s_last_poll = now;
    } else {
        Serial.printf("[RTU] Failed to queue poll for slave %u\n", s_cur_slave);
    }
}
