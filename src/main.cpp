#include <Arduino.h>

#include "config/config.h"
#include "reg_map/reg_map.h"
#include "wifi_mgr/wifi_mgr.h"
#include "canbus/canbus.h"
#include "modbus_rtu/modbus_rtu.h"
#include "modbus_tcp/modbus_tcp.h"
#include "modbus_udp/modbus_udp.h"
#include "n2k_forward/n2k_forward.h"
#include "webconfig/webconfig.h"

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("[MAIN] ESP32-S3 Modbus Gateway starting...");

    // 1. Load persistent configuration from NVS (falls back to defaults)
    config_init();
    config_load();

    // 2. Initialise the shared register map
    reg_map_init();

    // 3. Connect to WiFi (or start soft-AP for first-time configuration)
    wifi_init();

    // 4. CAN bus – NMEA 2000 via ISO-1050 / TWAI
    if (!canbus_init()) {
        Serial.println("[MAIN] CAN bus init failed – check ISO-1050 wiring");
    }

    // 5. Modbus RTU master (RS-485, UART1)
    modbus_rtu_init();

    // 6. Modbus TCP server (WiFi, port 502 by default)
    modbus_tcp_init();

    // 7. UDP broadcast of register map
    modbus_udp_init();

    // 8. NMEA 2000 forwarding (Modbus regs → CAN PGNs)
    n2k_forward_init();

    // 9. Web configuration UI (port 80)
    webconfig_init();

    Serial.printf("[MAIN] Ready – web UI at http://%s\n",
                  wifi_get_ip().c_str());
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop()
{
    modbus_rtu_update();    // poll RS-485 slaves, update reg_map
    modbus_tcp_update();    // service Modbus TCP clients
    modbus_udp_update();    // periodic UDP JSON broadcast
    n2k_forward_update();   // periodic NMEA 2000 PGN transmit
    canbus_update();        // NMEA 2000 receive / heartbeat
    webconfig_update();     // service web config HTTP clients
}
