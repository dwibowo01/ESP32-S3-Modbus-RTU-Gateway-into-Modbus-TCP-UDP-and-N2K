#pragma once

/**
 * config.h
 *
 * Persistent gateway configuration stored in ESP32 NVS (Non-Volatile Storage).
 *
 * All runtime-tuneable parameters live in GatewayConfig.  The web UI reads and
 * writes this struct via the /api/config REST endpoint; config_save() persists
 * it so the values survive power cycles.
 */

#include <Arduino.h>
#include "../defs.h"

// ---------------------------------------------------------------------------
// Configuration structure
// ---------------------------------------------------------------------------

struct GatewayConfig {
    // --- WiFi ---------------------------------------------------------------
    char     wifi_ssid[33];        ///< AP SSID to connect to (STA mode)
    char     wifi_pass[65];        ///< AP password

    // --- RS-485 operating mode ---------------------------------------------
    uint8_t  rs485_mode;           ///< RS485_MODE_MODBUS_RTU or RS485_MODE_NMEA0183

    // --- Modbus RTU master (RS-485) ----------------------------------------
    uint32_t rtu_baud;             ///< Serial baud rate (default 9600)
    uint8_t  rtu_tx_pin;           ///< UART TX / RS-485 DI  (default GPIO 17)
    uint8_t  rtu_rx_pin;           ///< UART RX / RS-485 RO  (default GPIO 18)
    uint8_t  rtu_de_pin;           ///< RS-485 DE/RE control  (default GPIO 16)
    uint8_t  rtu_slave_ids[GATEWAY_MAX_SLAVES]; ///< Slave IDs to poll
    uint8_t  rtu_slave_count;      ///< Number of valid entries in rtu_slave_ids
    uint16_t rtu_reg_start;        ///< First holding-register address to poll
    uint16_t rtu_reg_count;        ///< Number of registers to poll (max GATEWAY_REGS_PER_SLAVE)
    uint32_t rtu_poll_ms;          ///< Interval between polls per slave (ms)

    // --- NMEA 0183 listener (RS-485, same TX/RX/DE pins as RTU) ------------
    uint32_t nmea_baud;            ///< NMEA 0183 baud rate (default 4800)
    bool     nmea_udp_raw_enabled; ///< Broadcast raw NMEA sentences over UDP
    uint16_t nmea_udp_raw_port;    ///< UDP port for raw NMEA sentences (default 10110)

    // --- Modbus TCP server --------------------------------------------------
    uint16_t tcp_port;             ///< Listening port (default 502)

    // --- UDP broadcast ------------------------------------------------------
    uint16_t udp_port;             ///< Destination port (default 1502)
    uint32_t udp_interval_ms;      ///< Broadcast interval (ms, default 2000)

    // --- NMEA 2000 forwarding -----------------------------------------------
    bool     n2k_enabled;          ///< Master enable for N2K forwarding
    uint8_t  n2k_src_slave;        ///< Slave ID (Modbus) or virtual slot (NMEA 0183) to read from
    uint16_t n2k_heading_reg;      ///< Register index → heading in 0.1 ° units
    uint16_t n2k_depth_reg;        ///< Register index → water depth in cm
    uint16_t n2k_speed_reg;        ///< Register index → speed in 0.01 kn units
    uint16_t n2k_lat_int_reg;      ///< Register index → latitude integer degrees (signed)
    uint16_t n2k_lat_frac_reg;     ///< Register index → latitude fractional part ×10 000
    uint16_t n2k_lon_int_reg;      ///< Register index → longitude integer degrees (signed)
    uint16_t n2k_lon_frac_reg;     ///< Register index → longitude fractional part ×10 000
    uint32_t n2k_interval_ms;      ///< N2K transmit interval (ms, default 1000)
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Populate the in-RAM config with factory defaults (does NOT save). */
void           config_init();

/** Load persisted config from NVS; falls back to defaults on first boot. */
void           config_load();

/** Persist the current in-RAM config to NVS. */
void           config_save();

/** Reset in-RAM config to factory defaults and persist. */
void           config_reset_defaults();

/** Return a reference to the in-RAM config struct. */
GatewayConfig &config_get();
