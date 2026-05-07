#pragma once

/**
 * nmea0183.h
 *
 * NMEA 0183 listener for the RS-485 port (UART1 / Serial1).
 *
 * Active only when GatewayConfig::rs485_mode == RS485_MODE_NMEA0183.
 * The RS-485 hardware pins (rtu_tx_pin, rtu_rx_pin, rtu_de_pin) are reused
 * with the NMEA-specific baud rate (nmea_baud, default 4800).
 *
 * Supported sentences
 * -------------------
 *   $xxHDT  – True Heading
 *   $xxHDG  – Magnetic Heading (used when HDT is absent)
 *   $xxDBT  – Depth Below Transducer (metres)
 *   $xxDBS  – Depth Below Surface (metres)
 *   $xxVHW  – Water Speed (knots field)
 *   $xxGLL  – Geographic Position Lat/Lon
 *   $xxGGA  – GPS Fix Data (position)
 *   $xxRMC  – Recommended Minimum (position + speed)
 *
 * Data flow
 * ---------
 * Parsed values are written to reg_map using n2k_src_slave as the virtual
 * slot ID and the existing n2k_*_reg indices as register offsets.  This
 * makes the data immediately available to:
 *   • Modbus TCP server  (reads reg_map directly)
 *   • UDP broadcast      (serialises reg_map to JSON)
 *   • N2K forwarding     (reads reg_map, sends PGNs via CAN)
 *
 * When nmea_udp_raw_enabled is true, each valid sentence is also sent
 * verbatim as a UDP broadcast on nmea_udp_raw_port (default 10110).
 *
 * Register encoding (same as RTU / N2K forwarding convention)
 * -----------------------------------------------------------
 *   heading_reg  : heading in 0.1 ° units  (uint16)
 *   depth_reg    : water depth in cm        (uint16)
 *   speed_reg    : speed in 0.01 kn units   (uint16)
 *   lat_int_reg  : latitude integer degrees  (int16 stored as uint16)
 *   lat_frac_reg : latitude fractional ×10000 (uint16)
 *   lon_int_reg  : longitude integer degrees (int16 stored as uint16)
 *   lon_frac_reg : longitude fractional ×10000 (uint16)
 */

/**
 * Initialise Serial1 for NMEA 0183 reception.
 * No-op (returns false) when rs485_mode != RS485_MODE_NMEA0183.
 */
bool nmea0183_init();

/**
 * Drain Serial1, parse any complete sentences, update reg_map.
 * No-op when rs485_mode != RS485_MODE_NMEA0183.
 * Must be called from loop() as frequently as possible.
 */
void nmea0183_update();
