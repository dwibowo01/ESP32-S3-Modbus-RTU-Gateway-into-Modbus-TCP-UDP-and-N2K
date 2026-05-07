#pragma once

/**
 * webconfig.h
 *
 * HTTP configuration and monitoring server (port 80).
 *
 * Endpoints
 * ----------
 * GET  /              – Full HTML configuration UI
 * GET  /api/config    – Current config as JSON
 * POST /api/config    – Update config from JSON body (saved to NVS)
 * GET  /api/regs      – Live register values as JSON
 * POST /api/reset     – Reset config to factory defaults
 *
 * The server must be polled via webconfig_update() every loop iteration.
 */

/** Register URL handlers and start the HTTP server on port 80. */
bool webconfig_init();

/** Process one pending HTTP client request.  Call from loop(). */
void webconfig_update();
