/**
 * reg_map.cpp
 *
 * Flat two-dimensional array:  s_regs[slave_index][reg_index]
 * s_valid[slave_index] tracks whether we have ever received a good poll.
 */

#include "reg_map.h"
#include <string.h>

static uint16_t s_regs[GATEWAY_MAX_SLAVES][GATEWAY_REGS_PER_SLAVE];
static bool     s_valid[GATEWAY_MAX_SLAVES];

void reg_map_init()
{
    memset(s_regs,  0, sizeof(s_regs));
    memset(s_valid, 0, sizeof(s_valid));
}

void reg_map_set(uint8_t slave_id, uint16_t reg_idx, uint16_t value)
{
    if (slave_id < 1 || slave_id > GATEWAY_MAX_SLAVES) return;
    if (reg_idx  >= GATEWAY_REGS_PER_SLAVE)            return;
    s_regs[slave_id - 1][reg_idx] = value;
}

uint16_t reg_map_get(uint8_t slave_id, uint16_t reg_idx)
{
    if (slave_id < 1 || slave_id > GATEWAY_MAX_SLAVES) return 0;
    if (reg_idx  >= GATEWAY_REGS_PER_SLAVE)            return 0;
    return s_regs[slave_id - 1][reg_idx];
}

bool reg_map_is_valid(uint8_t slave_id)
{
    if (slave_id < 1 || slave_id > GATEWAY_MAX_SLAVES) return false;
    return s_valid[slave_id - 1];
}

void reg_map_mark_valid(uint8_t slave_id)
{
    if (slave_id < 1 || slave_id > GATEWAY_MAX_SLAVES) return;
    s_valid[slave_id - 1] = true;
}

void reg_map_invalidate(uint8_t slave_id)
{
    if (slave_id < 1 || slave_id > GATEWAY_MAX_SLAVES) return;
    s_valid[slave_id - 1] = false;
}
