/**
 * canbus.cpp
 *
 * CAN bus module implementation for the ESP32-S3 gateway.
 *
 * Uses the ESP32 built-in TWAI controller with an external ISO-1050
 * galvanically isolated CAN transceiver for NMEA 2000 communication at
 * 250 kbit/s.
 *
 * The NMEA2000_esp32 library drives the TWAI peripheral so that the rest of
 * the application can work purely through the tNMEA2000 API.
 */

#include "canbus.h"

#include <NMEA2000_CAN.h>   // Selects the ESP32 TWAI driver automatically
#include <N2kMessages.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static bool s_ready = false;

// ---------------------------------------------------------------------------
// NMEA 2000 message handler
// ---------------------------------------------------------------------------

/**
 * Called by the NMEA 2000 library for every received message that matches a
 * registered PGN.  Extend this function (or register additional handlers via
 * NMEA2000.SetMsgHandler) to process incoming N2K messages.
 */
static void on_n2k_message(const tN2kMsg &msg)
{
    switch (msg.PGN) {
        case 127250UL: {  // Vessel Heading
            double heading;
            tN2kHeadingReference ref;
            double deviation;
            double variation;
            if (ParseN2kHeading(msg, heading, deviation, variation, ref)) {
                Serial.printf("[N2K] Heading: %.1f deg\n",
                              RadToDeg(heading));
            }
            break;
        }

        case 128267UL: {  // Water Depth
            double depth;
            double offset;
            double range;
            if (ParseN2kWaterDepth(msg, depth, offset, range)) {
                Serial.printf("[N2K] Water depth: %.2f m\n", depth);
            }
            break;
        }

        case 129025UL: {  // Position – Rapid Update
            double lat;
            double lon;
            if (ParseN2kPositionRapid(msg, lat, lon)) {
                Serial.printf("[N2K] Position: %.6f, %.6f\n", lat, lon);
            }
            break;
        }

        default:
            Serial.printf("[N2K] PGN %lu received (len %u)\n",
                          msg.PGN, msg.DataLen);
            break;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool canbus_init(void)
{
    // -----------------------------------------------------------------------
    // Configure TWAI pin mapping for the ISO-1050 transceiver.
    // The NMEA2000_CAN macro expands to the NMEA2000_esp32 object which
    // directly controls the TWAI peripheral.
    // -----------------------------------------------------------------------
    NMEA2000.SetCANTxPin(CAN_TX_PIN);
    NMEA2000.SetCANRxPin(CAN_RX_PIN);

    // -----------------------------------------------------------------------
    // Device identity (ISO Address Claiming)
    // -----------------------------------------------------------------------
    NMEA2000.SetProductInformation(
        "ESP32-S3-GW-001",          // Manufacturer model serial code
        100,                         // Product code
        N2K_DEVICE_NAME,             // Model ID
        "1.0.0 (2026-05-07)",        // Software version
        "1.0.0"                      // Model version
    );

    NMEA2000.SetDeviceInformation(
        N2K_UNIQUE_NUMBER,
        135,   // Device function: Gateway
        25,    // Device class: Inter/Intranetwork Device
        N2K_MANUFACTURER_CODE
    );

    // -----------------------------------------------------------------------
    // Library configuration
    // -----------------------------------------------------------------------
    // Forward all received messages to our handler.
    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode);
    NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);   // human-readable to Serial
    NMEA2000.SetForwardStream(&Serial);
    NMEA2000.EnableForward(false);                    // disable raw forwarding; use handler

    NMEA2000.SetMsgHandler(on_n2k_message);

    // -----------------------------------------------------------------------
    // Open the bus
    // -----------------------------------------------------------------------
    if (!NMEA2000.Open()) {
        Serial.println("[CAN] ERROR: Failed to open NMEA 2000 / TWAI bus");
        s_ready = false;
        return false;
    }

    s_ready = true;
    Serial.printf("[CAN] ISO-1050 TWAI ready  TX=GPIO%d  RX=GPIO%d\n",
                  (int)CAN_TX_PIN, (int)CAN_RX_PIN);
    return true;
}

void canbus_update(void)
{
    if (s_ready) {
        NMEA2000.ParseMessages();
    }
}

tNMEA2000 &canbus_nmea2000(void)
{
    return NMEA2000;
}

bool canbus_send(tN2kMsg &msg)
{
    if (!s_ready) {
        return false;
    }
    return NMEA2000.SendMsg(msg);
}

bool canbus_is_ready(void)
{
    return s_ready;
}
