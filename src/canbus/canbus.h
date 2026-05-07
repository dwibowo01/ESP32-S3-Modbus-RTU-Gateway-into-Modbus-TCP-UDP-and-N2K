#pragma once

/**
 * canbus.h
 *
 * CAN bus module for ESP32-S3 gateway.
 *
 * Hardware: ISO-1050 galvanically isolated CAN transceiver connected to the
 * ESP32 built-in TWAI (Two-Wire Automotive Interface) controller.
 *
 * Wiring:
 *   ISO-1050 TXD  <-->  ESP32 CAN_TX_PIN  (default GPIO 5)
 *   ISO-1050 RXD  <-->  ESP32 CAN_RX_PIN  (default GPIO 4)
 *   ISO-1050 CANH/CANL connected to the CAN bus differential pair.
 *
 * Protocol: NMEA 2000 (250 kbit/s, 29-bit extended identifiers).
 */

#include <Arduino.h>
#include <NMEA2000.h>
#include <N2kMessages.h>

// ---------------------------------------------------------------------------
// Pin defaults (can be overridden in platformio.ini via build_flags)
// ---------------------------------------------------------------------------
#ifndef CAN_TX_PIN
#define CAN_TX_PIN GPIO_NUM_5
#endif

#ifndef CAN_RX_PIN
#define CAN_RX_PIN GPIO_NUM_4
#endif

// ---------------------------------------------------------------------------
// NMEA 2000 device identity defaults
// ---------------------------------------------------------------------------
#ifndef N2K_DEVICE_NAME
#define N2K_DEVICE_NAME "ESP32-S3 Gateway"
#endif

#ifndef N2K_MANUFACTURER_CODE
#define N2K_MANUFACTURER_CODE 135   // Use a registered manufacturer code in production
#endif

#ifndef N2K_UNIQUE_NUMBER
#define N2K_UNIQUE_NUMBER 1         // Unique device serial number
#endif

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Initialise the CAN bus module.
 *
 * Configures the ESP32 TWAI peripheral and opens the NMEA 2000 connection.
 * Must be called once from setup() before any other canbus_* function.
 *
 * @return true on success, false if the TWAI driver failed to start.
 */
bool canbus_init(void);

/**
 * Service the CAN bus module.
 *
 * Drives the NMEA 2000 stack (message reception, heartbeat, address claiming).
 * Must be called repeatedly from loop().
 */
void canbus_update(void);

/**
 * Return a reference to the underlying tNMEA2000 object.
 *
 * Allows the rest of the application to register message handlers or send
 * PGNs directly via the NMEA 2000 library.
 */
tNMEA2000 &canbus_nmea2000(void);

/**
 * Send a raw NMEA 2000 message.
 *
 * @param msg  Fully populated tN2kMsg to transmit.
 * @return true if the message was queued for transmission.
 */
bool canbus_send(tN2kMsg &msg);

/**
 * Return true when the TWAI driver is running and the NMEA 2000 bus is open.
 */
bool canbus_is_ready(void);
