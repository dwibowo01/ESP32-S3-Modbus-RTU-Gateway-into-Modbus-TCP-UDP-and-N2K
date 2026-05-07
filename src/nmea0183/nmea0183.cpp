/**
 * nmea0183.cpp
 *
 * NMEA 0183 listener / parser for the RS-485 port.
 *
 * Architecture
 * ------------
 * Each call to nmea0183_update() reads all pending bytes from Serial1 into
 * a line-buffer.  When a complete sentence is detected ('\n') it is:
 *   1. Checksum-validated (XOR of bytes between '$' and '*').
 *   2. Split into comma-separated fields.
 *   3. Dispatched to the appropriate sentence handler which writes the
 *      parsed value(s) into reg_map.
 *   4. Optionally broadcast verbatim over UDP.
 *
 * Sentence identification uses only the last three characters of the
 * talker+sentence-ID field (e.g. "GPRMC" → "RMC"), so any talker prefix
 * (GP, HC, SD, VW, II, …) is accepted transparently.
 *
 * Degree format
 * -------------
 * NMEA position fields use DDDMM.MMMMM format.  Conversion:
 *   decimal_degrees = floor(raw/100) + (raw mod 100) / 60
 */

#include "nmea0183.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include "../defs.h"
#include <WiFiUdp.h>
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

#define NMEA_BUF_SIZE  128

static char    s_buf[NMEA_BUF_SIZE];
static uint8_t s_len     = 0;
static bool    s_in_sent = false;   // true once '$' has been seen

static WiFiUDP s_udp_raw;
static bool    s_udp_ready = false;

// ---------------------------------------------------------------------------
// Internal utilities
// ---------------------------------------------------------------------------

/** Validate NMEA checksum.  Returns true if correct or absent. */
static bool validate_checksum(const char *sentence)
{
    // sentence starts with '$', ends before '\0'
    const char *p   = sentence + 1;    // skip '$'
    const char *star = strchr(p, '*');
    if (!star) return true;            // no checksum present – accept

    uint8_t calc = 0;
    while (p < star) calc ^= (uint8_t)*p++;

    unsigned int given = 0;
    sscanf(star + 1, "%02X", &given);
    return calc == (uint8_t)given;
}

/**
 * Split a mutable C-string at ',' and '*' delimiters.
 * Returns number of fields.  fields[0] is the talker+sentence type,
 * fields[1..] are the data fields.
 */
static int split_fields(char *s, char *fields[], int max_fields)
{
    int n = 0;
    fields[n++] = s;
    for (char *p = s; *p && n < max_fields; p++) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    return n;
}

/**
 * Parse NMEA degree-minute string (DDDMM.MMMMM or DDMM.MMMMM) with
 * hemisphere indicator ('N','S','E','W') into a signed decimal degree.
 */
static double parse_nmea_latlon(const char *field, const char *hemi)
{
    if (!field || field[0] == '\0') return 0.0;
    double raw  = atof(field);
    int    deg  = (int)(raw / 100.0);
    double min  = raw - deg * 100.0;
    double dec  = deg + min / 60.0;
    if (hemi && (hemi[0] == 'S' || hemi[0] == 'W')) dec = -dec;
    return dec;
}

/**
 * Split decimal degrees into integer part and fractional part ×10000,
 * preserving sign in the integer half.
 */
static void split_latlon(double dec,
                          int16_t  *out_int,
                          uint16_t *out_frac)
{
    *out_int  = (int16_t)dec;
    double frac = fabs(dec - (double)*out_int);
    *out_frac = (uint16_t)(frac * 10000.0 + 0.5);
}

// ---------------------------------------------------------------------------
// Raw UDP broadcast
// ---------------------------------------------------------------------------

static void broadcast_raw(const char *sentence)
{
    GatewayConfig &cfg = config_get();
    if (!cfg.nmea_udp_raw_enabled || !s_udp_ready) return;

    s_udp_raw.beginPacket(IPAddress(255, 255, 255, 255),
                          cfg.nmea_udp_raw_port);
    s_udp_raw.print(sentence);
    s_udp_raw.print("\r\n");
    s_udp_raw.endPacket();
}

// ---------------------------------------------------------------------------
// Sentence handlers
// ---------------------------------------------------------------------------

static void handle_hdt(char *fields[], int n)
{
    // $xxHDT,hhh.h,T*cs
    if (n < 2 || fields[1][0] == '\0') return;
    GatewayConfig &cfg = config_get();
    double hdg = atof(fields[1]);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_heading_reg,
                (uint16_t)(hdg * 10.0 + 0.5));
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_hdg(char *fields[], int n)
{
    // $xxHDG,hhh.h,deviation,dir,variation,dir*cs
    if (n < 2 || fields[1][0] == '\0') return;
    GatewayConfig &cfg = config_get();
    double hdg = atof(fields[1]);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_heading_reg,
                (uint16_t)(hdg * 10.0 + 0.5));
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_dbt(char *fields[], int n)
{
    // $xxDBT,f,f,m,M,F,F*cs  – metres in fields[3]
    if (n < 4 || fields[3][0] == '\0') return;
    GatewayConfig &cfg = config_get();
    double depth_m = atof(fields[3]);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_depth_reg,
                (uint16_t)(depth_m * 100.0 + 0.5));
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_vhw(char *fields[], int n)
{
    // $xxVHW,T,T,M,M,kn,N,km,K*cs – knots in fields[5]
    if (n < 6 || fields[5][0] == '\0') return;
    GatewayConfig &cfg = config_get();
    double spd_kn = atof(fields[5]);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_speed_reg,
                (uint16_t)(spd_kn * 100.0 + 0.5));
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_gll(char *fields[], int n)
{
    // $xxGLL,lat,N/S,lon,E/W,hhmmss.ss,A/V,mode*cs
    if (n < 5 || fields[1][0] == '\0' || fields[3][0] == '\0') return;
    GatewayConfig &cfg = config_get();

    double lat = parse_nmea_latlon(fields[1], fields[2]);
    double lon = parse_nmea_latlon(fields[3], fields[4]);

    int16_t  lat_i; uint16_t lat_f;
    int16_t  lon_i; uint16_t lon_f;
    split_latlon(lat, &lat_i, &lat_f);
    split_latlon(lon, &lon_i, &lon_f);

    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_int_reg,  (uint16_t)lat_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_frac_reg, lat_f);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_int_reg,  (uint16_t)lon_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_frac_reg, lon_f);
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_gga(char *fields[], int n)
{
    // $xxGGA,time,lat,N/S,lon,E/W,fix,sat,hdop,alt,M,...*cs
    if (n < 6 || fields[2][0] == '\0' || fields[4][0] == '\0') return;
    if (fields[6][0] == '0') return;   // fix quality = 0 → no fix

    GatewayConfig &cfg = config_get();

    double lat = parse_nmea_latlon(fields[2], fields[3]);
    double lon = parse_nmea_latlon(fields[4], fields[5]);

    int16_t  lat_i; uint16_t lat_f;
    int16_t  lon_i; uint16_t lon_f;
    split_latlon(lat, &lat_i, &lat_f);
    split_latlon(lon, &lon_i, &lon_f);

    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_int_reg,  (uint16_t)lat_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_frac_reg, lat_f);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_int_reg,  (uint16_t)lon_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_frac_reg, lon_f);
    reg_map_mark_valid(cfg.n2k_src_slave);
}

static void handle_rmc(char *fields[], int n)
{
    // $xxRMC,time,A/V,lat,N/S,lon,E/W,spd,cog,date,...*cs
    if (n < 8) return;
    if (fields[2][0] != 'A') return;   // status must be Active
    if (fields[3][0] == '\0' || fields[5][0] == '\0') return;

    GatewayConfig &cfg = config_get();

    double lat    = parse_nmea_latlon(fields[3], fields[4]);
    double lon    = parse_nmea_latlon(fields[5], fields[6]);
    double spd_kn = atof(fields[7]);

    int16_t  lat_i; uint16_t lat_f;
    int16_t  lon_i; uint16_t lon_f;
    split_latlon(lat, &lat_i, &lat_f);
    split_latlon(lon, &lon_i, &lon_f);

    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_int_reg,  (uint16_t)lat_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lat_frac_reg, lat_f);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_int_reg,  (uint16_t)lon_i);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_lon_frac_reg, lon_f);
    reg_map_set(cfg.n2k_src_slave, cfg.n2k_speed_reg,
                (uint16_t)(spd_kn * 100.0 + 0.5));
    reg_map_mark_valid(cfg.n2k_src_slave);
}

// ---------------------------------------------------------------------------
// Sentence dispatcher
// ---------------------------------------------------------------------------

static void process_sentence(char *raw)
{
    if (!validate_checksum(raw)) {
        Serial.printf("[NMEA] Bad checksum: %s\n", raw);
        return;
    }

    // Work on a copy because split_fields mutates in place
    char work[NMEA_BUF_SIZE];
    strncpy(work, raw + 1, sizeof(work) - 1);   // skip leading '$'
    work[sizeof(work) - 1] = '\0';

    char *fields[20];
    int   n = split_fields(work, fields, 20);
    if (n < 1) return;

    // Sentence type is last 3 chars of fields[0]  (e.g. "GPRMC" → "RMC")
    const char *type = fields[0];
    size_t      tlen = strlen(type);
    if (tlen < 3) return;
    const char *id = type + tlen - 3;

    if      (strcmp(id, "HDT") == 0) handle_hdt(fields, n);
    else if (strcmp(id, "HDG") == 0) handle_hdg(fields, n);
    else if (strcmp(id, "DBT") == 0) handle_dbt(fields, n);
    else if (strcmp(id, "DBS") == 0) handle_dbt(fields, n); // same layout
    else if (strcmp(id, "VHW") == 0) handle_vhw(fields, n);
    else if (strcmp(id, "GLL") == 0) handle_gll(fields, n);
    else if (strcmp(id, "GGA") == 0) handle_gga(fields, n);
    else if (strcmp(id, "RMC") == 0) handle_rmc(fields, n);

    broadcast_raw(raw);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool nmea0183_init()
{
    GatewayConfig &cfg = config_get();

    if (cfg.rs485_mode != RS485_MODE_NMEA0183) {
        return false;
    }

    Serial1.begin(cfg.nmea_baud, SERIAL_8N1,
                  cfg.rtu_rx_pin, cfg.rtu_tx_pin);
    s_len     = 0;
    s_in_sent = false;

    if (cfg.nmea_udp_raw_enabled) {
        s_udp_raw.begin(cfg.nmea_udp_raw_port);
        s_udp_ready = true;
        Serial.printf("[NMEA] Raw UDP on port %u\n", cfg.nmea_udp_raw_port);
    }

    Serial.printf("[NMEA] NMEA 0183 listener init  baud=%u  RX=GPIO%u  TX=GPIO%u\n",
                  cfg.nmea_baud, cfg.rtu_rx_pin, cfg.rtu_tx_pin);
    return true;
}

void nmea0183_update()
{
    GatewayConfig &cfg = config_get();
    if (cfg.rs485_mode != RS485_MODE_NMEA0183) return;

    while (Serial1.available()) {
        char c = (char)Serial1.read();

        if (c == '$') {
            // Start of new sentence – reset buffer
            s_buf[0]  = '$';
            s_len     = 1;
            s_in_sent = true;
            continue;
        }

        if (!s_in_sent) continue;   // skip until first '$'

        if (c == '\r') continue;    // ignore CR

        if (c == '\n') {
            if (s_len > 1) {
                s_buf[s_len] = '\0';
                process_sentence(s_buf);
            }
            s_len     = 0;
            s_in_sent = false;
            continue;
        }

        if (s_len < NMEA_BUF_SIZE - 1) {
            s_buf[s_len++] = c;
        } else {
            // Buffer overflow – discard and wait for next sentence
            s_len     = 0;
            s_in_sent = false;
        }
    }
}
