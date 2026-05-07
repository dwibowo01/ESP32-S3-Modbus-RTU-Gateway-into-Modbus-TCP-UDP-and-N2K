#pragma once

/**
 * modbus_tcp.h
 *
 * Modbus TCP server (gateway read-only view of the register map).
 *
 * Implemented directly over WiFiServer / WiFiClient using the standard
 * MBAP + PDU framing so no additional Modbus library is required.
 *
 * Supported function codes
 * ------------------------
 *   FC 03 – Read Holding Registers
 *   FC 04 – Read Input Registers  (mapped to the same data as FC 03)
 *
 * Unit-ID / address routing
 * -------------------------
 * If the MBAP Unit ID is 1-8 (i.e. a valid slave ID), the register
 * address in the PDU is a 0-based index into that slave's block.
 *
 * If the Unit ID is 0x00 or 0xFF (broadcast / unspecified), the flat
 * address scheme is used instead:
 *   tcp_address = (slave_id - 1) * GATEWAY_REGS_PER_SLAVE + reg_index
 *
 * Up to MAX_TCP_CLIENTS simultaneous connections are accepted.
 */

/**
 * Create the WiFiServer and start listening on cfg.tcp_port.
 * Call after WiFi is up.
 */
bool modbus_tcp_init();

/**
 * Accept new connections and service pending client requests.
 * Call from loop().
 */
void modbus_tcp_update();
