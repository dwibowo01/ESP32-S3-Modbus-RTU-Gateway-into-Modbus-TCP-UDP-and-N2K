#pragma once

/**
 * defs.h
 *
 * Shared compile-time constants used across all gateway modules.
 * Change these values here; every module picks them up automatically.
 */

// Maximum number of Modbus RTU slave devices the gateway can poll.
#define GATEWAY_MAX_SLAVES      8

// Number of holding registers cached per slave (0-based index 0..63).
#define GATEWAY_REGS_PER_SLAVE  64

// RS-485 operating mode (GatewayConfig::rs485_mode)
#define RS485_MODE_MODBUS_RTU   0   ///< Modbus RTU master (default)
#define RS485_MODE_NMEA0183     1   ///< NMEA 0183 listener
