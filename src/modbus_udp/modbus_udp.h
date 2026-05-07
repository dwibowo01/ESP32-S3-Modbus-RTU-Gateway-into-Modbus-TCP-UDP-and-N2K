#pragma once

/**
 * modbus_udp.h
 *
 * Periodic UDP broadcast of the gateway register map.
 *
 * Every udp_interval_ms milliseconds a JSON packet is broadcast to
 * 255.255.255.255 on cfg.udp_port.  The payload lists all configured
 * slaves and their current (polled) register values:
 *
 * {
 *   "ts":   <millis>,
 *   "slaves": [
 *     { "id": 1, "valid": true,  "regs": [v0, v1, ...] },
 *     { "id": 2, "valid": false, "regs": [0,  0,  ...] }
 *   ]
 * }
 */

/** Open the UDP socket.  Call after WiFi is up. */
bool modbus_udp_init();

/** Broadcast the register map if the interval has elapsed. Call from loop(). */
void modbus_udp_update();
