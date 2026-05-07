#pragma once

/**
 * wifi_mgr.h
 *
 * Manages the WiFi connection.
 *
 * Behaviour:
 *   1. If cfg.wifi_ssid is non-empty, attempt to connect in STA mode.
 *      Retry for WIFI_CONNECT_TIMEOUT_MS ms.
 *   2. If STA connection fails (or no SSID is configured), start a
 *      software Access Point so the user can reach the web config UI and
 *      set credentials.
 */

#include <Arduino.h>

/** Connect to the configured AP or start fallback AP.  Always returns true. */
bool   wifi_init();

/** Returns true while connected in Station mode. */
bool   wifi_is_connected();

/** Returns true when running as a soft-AP (configuration mode). */
bool   wifi_is_ap_mode();

/** IP address string (STA or soft-AP). */
String wifi_get_ip();
