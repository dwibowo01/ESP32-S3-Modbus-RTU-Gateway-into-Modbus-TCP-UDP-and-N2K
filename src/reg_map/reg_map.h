#pragma once

/**
 * reg_map.h
 *
 * Shared in-RAM register map.
 *
 * The Modbus RTU master writes here after each successful poll.
 * The Modbus TCP server, UDP broadcast, and N2K forwarding modules read
 * from here independently.
 *
 * Slave IDs are 1-based (Modbus convention).  Internally they index a
 * 0-based array, so valid slave_id values are 1 .. GATEWAY_MAX_SLAVES.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../defs.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Initialise all entries to 0 and mark every slave as invalid. */
void     reg_map_init();

/**
 * Write a single register value for the given slave.
 *
 * @param slave_id  Modbus slave address (1 .. GATEWAY_MAX_SLAVES)
 * @param reg_idx   0-based register index within the slave's block
 * @param value     Register value to store
 */
void     reg_map_set(uint8_t slave_id, uint16_t reg_idx, uint16_t value);

/**
 * Read a single register value.  Returns 0 if the address is out of range.
 *
 * @param slave_id  Modbus slave address (1 .. GATEWAY_MAX_SLAVES)
 * @param reg_idx   0-based register index within the slave's block
 */
uint16_t reg_map_get(uint8_t slave_id, uint16_t reg_idx);

/** Return true if at least one successful poll has been received for this slave. */
bool     reg_map_is_valid(uint8_t slave_id);

/** Mark slave data as valid (called by the RTU master after a good response). */
void     reg_map_mark_valid(uint8_t slave_id);

/** Mark slave data as stale / invalid (called on RTU timeout or error). */
void     reg_map_invalidate(uint8_t slave_id);
