#pragma once

/**
 * n2k_forward.h
 *
 * Converts polled Modbus register values into NMEA 2000 PGN messages and
 * transmits them over the CAN bus (via the canbus module).
 *
 * Register-to-PGN mapping (all configurable via web UI)
 * -------------------------------------------------------
 * Reg  n2k_heading_reg  → PGN 127250 Vessel Heading    (raw × 0.1 °)
 * Reg  n2k_depth_reg    → PGN 128267 Water Depth        (raw cm → m)
 * Reg  n2k_speed_reg    → PGN 128259 Boat Speed         (raw × 0.01 kn)
 * Regs n2k_lat_int_reg / n2k_lat_frac_reg
 *      n2k_lon_int_reg / n2k_lon_frac_reg
 *                        → PGN 129025 Position Rapid Update
 *                          lat = lat_int + lat_frac/10000.0  (signed)
 *                          lon = lon_int + lon_frac/10000.0  (signed)
 */

/** Initialise the forwarding module (no hardware setup needed). */
bool n2k_forward_init();

/** Transmit N2K PGNs if data is valid and the interval has elapsed. */
void n2k_forward_update();
