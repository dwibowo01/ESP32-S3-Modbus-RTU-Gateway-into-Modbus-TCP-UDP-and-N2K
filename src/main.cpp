#include <Arduino.h>

#include "canbus/canbus.h"

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("[MAIN] ESP32-S3 Gateway starting...");

    if (!canbus_init()) {
        Serial.println("[MAIN] CAN bus init failed – check ISO-1050 wiring");
    }
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop()
{
    canbus_update();
}
