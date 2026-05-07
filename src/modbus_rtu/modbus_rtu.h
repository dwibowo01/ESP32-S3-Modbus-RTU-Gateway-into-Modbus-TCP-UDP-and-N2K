#pragma once

/**
 * modbus_rtu.h
 *
 * Modbus RTU master over RS-485.
 *
 * Uses UART1 (Serial1) with hardware flow-control via a DE/RE pin connected
 * to the RS-485 transceiver.  All configured slave IDs are polled in a
 * round-robin fashion at the configured interval; results land in reg_map.
 *
 * Relies on the emelianov/modbus-esp8266 library (ModbusRTU class).
 */

/**
 * Configure UART1, attach the ModbusRTU master, and start Serial1.
 * Must be called from setup() after config_load().
 *
 * @return true always (Serial1 begin does not return an error code).
 */
bool modbus_rtu_init();

/**
 * Drive the RTU state machine and queue the next poll when due.
 * Must be called from loop() as frequently as possible.
 */
void modbus_rtu_update();
