/**
 * webconfig.cpp
 *
 * Provides a browser-accessible configuration and monitoring UI over HTTP.
 * Uses the built-in ESP32 WebServer class (no extra library needed).
 *
 * The HTML page is stored in PROGMEM as a raw-string literal so it does
 * not consume RAM at runtime.
 */

#include "webconfig.h"
#include "../config/config.h"
#include "../reg_map/reg_map.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

static WebServer s_web(80);

// ---------------------------------------------------------------------------
// Embedded HTML (PROGMEM)
// ---------------------------------------------------------------------------

static const char HTML_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Gateway</title>
<style>
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#eef2f7;margin:0;padding:16px;max-width:900px}
h1{color:#003366;margin-bottom:4px}
.sub{color:#555;font-size:.9rem;margin-bottom:20px}
h2{color:#003366;border-bottom:2px solid #003366;padding-bottom:4px;margin-top:28px}
.card{background:#fff;border-radius:8px;padding:16px;margin:12px 0;box-shadow:0 1px 4px rgba(0,0,0,.1)}
.row{display:flex;flex-wrap:wrap;gap:12px}
.row label{flex:1;min-width:180px}
label{display:block;margin-top:10px;font-size:.9rem;font-weight:600;color:#333}
label span{display:block;font-weight:400;color:#666;font-size:.8rem;margin-bottom:2px}
input[type=text],input[type=number],input[type=password]{
  width:100%;padding:7px 9px;border:1px solid #ccc;border-radius:5px;font-size:.9rem;margin-top:3px}
input[type=text]:focus,input[type=number]:focus,input[type=password]:focus{
  outline:none;border-color:#0055aa}
.cb-row{display:flex;align-items:center;gap:8px;margin-top:12px;font-weight:600}
.cb-row input{width:auto}
.actions{margin-top:24px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.btn{padding:10px 24px;border:none;border-radius:6px;cursor:pointer;font-size:.95rem;font-weight:600}
.btn-save{background:#003366;color:#fff}.btn-save:hover{background:#0044aa}
.btn-reset{background:#cc2200;color:#fff}.btn-reset:hover{background:#ee3300}
#status{font-weight:600;font-size:.9rem}
.ok{color:#008800}.err{color:#cc0000}
#livedata{font-family:monospace;background:#1a1a2e;color:#00e676;padding:16px;
  border-radius:8px;min-height:90px;white-space:pre;overflow:auto;font-size:.82rem}
.badge{background:#dde8ff;color:#003366;border-radius:12px;
  font-size:.75rem;padding:2px 9px;font-weight:700;margin-left:6px;vertical-align:middle}
</style>
</head>
<body>
<h1>&#9889; ESP32-S3 Modbus Gateway</h1>
<div class="sub">Configure, monitor, and manage the Modbus RTU &#8594; TCP / UDP / NMEA 2000 gateway.</div>

<!-- WiFi -->
<h2>WiFi</h2>
<div class="card">
  <div class="row">
    <label>SSID<input id="ssid" type="text" maxlength="32"></label>
    <label>Password<input id="pass" type="password" maxlength="64"></label>
  </div>
</div>

<!-- Modbus RTU -->
<h2>Modbus RTU Master <span class="badge">RS-485</span></h2>
<div class="card">
  <div class="row">
    <label>Baud Rate<input id="rtu_baud" type="number" min="1200" max="115200"></label>
    <label>TX GPIO<span>UART TX / RS-485 DI</span><input id="rtu_tx" type="number" min="0" max="48"></label>
    <label>RX GPIO<span>UART RX / RS-485 RO</span><input id="rtu_rx" type="number" min="0" max="48"></label>
    <label>DE/RE GPIO<span>RS-485 driver enable</span><input id="rtu_de" type="number" min="0" max="48"></label>
  </div>
  <div class="row">
    <label>Slave IDs<span>Comma-separated, e.g. 1,2,3</span><input id="rtu_slaves" type="text"></label>
    <label>Start Register<input id="rtu_reg_start" type="number" min="0" max="65535"></label>
    <label>Register Count<span>Max 64</span><input id="rtu_reg_count" type="number" min="1" max="64"></label>
    <label>Poll Interval (ms)<input id="rtu_poll_ms" type="number" min="100"></label>
  </div>
</div>

<!-- Modbus TCP -->
<h2>Modbus TCP Server <span class="badge">Read-only</span></h2>
<div class="card">
  <div class="row">
    <label>TCP Port<input id="tcp_port" type="number" min="1" max="65535"></label>
  </div>
  <p style="font-size:.82rem;color:#666;margin:8px 0 0">
    FC03 / FC04 supported. Set MBAP Unit ID = slave address (1-8) to read that
    slave's registers. Unit ID 0 / 0xFF uses flat addressing:
    tcp_addr = (slave&#8722;1)&#215;64 + reg.
  </p>
</div>

<!-- UDP -->
<h2>UDP Broadcast</h2>
<div class="card">
  <div class="row">
    <label>UDP Port<input id="udp_port" type="number" min="1" max="65535"></label>
    <label>Interval (ms)<input id="udp_interval" type="number" min="100"></label>
  </div>
  <p style="font-size:.82rem;color:#666;margin:8px 0 0">
    Broadcasts JSON to 255.255.255.255 with all slave register values.
  </p>
</div>

<!-- N2K -->
<h2>NMEA 2000 Forwarding <span class="badge">CAN bus</span></h2>
<div class="card">
  <label class="cb-row">
    <input id="n2k_en" type="checkbox"> Enable N2K forwarding
  </label>
  <div class="row" style="margin-top:6px">
    <label>Source Slave ID<input id="n2k_slave" type="number" min="1" max="8"></label>
    <label>Transmit Interval (ms)<input id="n2k_interval" type="number" min="100"></label>
  </div>
  <div class="row">
    <label>Heading Reg<span>PGN 127250 &nbsp;&#215;0.1&deg;</span><input id="n2k_head" type="number" min="0" max="63"></label>
    <label>Depth Reg<span>PGN 128267 &nbsp;cm</span><input id="n2k_depth" type="number" min="0" max="63"></label>
    <label>Speed Reg<span>PGN 128259 &nbsp;&#215;0.01 kn</span><input id="n2k_speed" type="number" min="0" max="63"></label>
  </div>
  <div class="row">
    <label>Latitude Int Reg<span>PGN 129025 &nbsp;signed deg</span><input id="n2k_lat_i" type="number" min="0" max="63"></label>
    <label>Latitude Frac Reg<span>&#247;10000</span><input id="n2k_lat_f" type="number" min="0" max="63"></label>
    <label>Longitude Int Reg<span>signed deg</span><input id="n2k_lon_i" type="number" min="0" max="63"></label>
    <label>Longitude Frac Reg<span>&#247;10000</span><input id="n2k_lon_f" type="number" min="0" max="63"></label>
  </div>
</div>

<!-- Actions -->
<div class="actions">
  <button class="btn btn-save" onclick="saveConfig()">&#128190; Save Configuration</button>
  <button class="btn btn-reset" onclick="confirmReset()">&#128260; Reset to Defaults</button>
  <span id="status"></span>
</div>

<!-- Live data -->
<h2>Live Register Data <span class="badge">auto-refresh 2 s</span></h2>
<div id="livedata">Loading&#8230;</div>

<script>
async function loadConfig(){
  try{
    const r=await fetch('/api/config');
    if(!r.ok)return;
    const c=await r.json();
    set('ssid',c.wifi_ssid||'');
    set('pass',c.wifi_pass||'');
    set('rtu_baud',c.rtu_baud||9600);
    set('rtu_tx',c.rtu_tx_pin||17);
    set('rtu_rx',c.rtu_rx_pin||18);
    set('rtu_de',c.rtu_de_pin||16);
    set('rtu_slaves',(c.rtu_slave_ids||[1]).join(','));
    set('rtu_reg_start',c.rtu_reg_start??0);
    set('rtu_reg_count',c.rtu_reg_count||16);
    set('rtu_poll_ms',c.rtu_poll_ms||1000);
    set('tcp_port',c.tcp_port||502);
    set('udp_port',c.udp_port||1502);
    set('udp_interval',c.udp_interval_ms||2000);
    document.getElementById('n2k_en').checked=!!c.n2k_enabled;
    set('n2k_slave',c.n2k_src_slave||1);
    set('n2k_interval',c.n2k_interval_ms||1000);
    set('n2k_head',c.n2k_heading_reg??0);
    set('n2k_depth',c.n2k_depth_reg??1);
    set('n2k_speed',c.n2k_speed_reg??2);
    set('n2k_lat_i',c.n2k_lat_int_reg??3);
    set('n2k_lat_f',c.n2k_lat_frac_reg??4);
    set('n2k_lon_i',c.n2k_lon_int_reg??5);
    set('n2k_lon_f',c.n2k_lon_frac_reg??6);
  }catch(e){console.error('loadConfig',e);}
}
function set(id,v){const el=document.getElementById(id);if(el)el.value=v;}
function num(id){return parseInt(document.getElementById(id).value)||0;}

async function saveConfig(){
  const slaves=document.getElementById('rtu_slaves').value
    .split(',').map(s=>parseInt(s.trim())).filter(v=>v>=1&&v<=247);
  const body={
    wifi_ssid:document.getElementById('ssid').value,
    wifi_pass:document.getElementById('pass').value,
    rtu_baud:num('rtu_baud'),rtu_tx_pin:num('rtu_tx'),
    rtu_rx_pin:num('rtu_rx'),rtu_de_pin:num('rtu_de'),
    rtu_slave_ids:slaves,
    rtu_reg_start:num('rtu_reg_start'),rtu_reg_count:num('rtu_reg_count'),
    rtu_poll_ms:num('rtu_poll_ms'),
    tcp_port:num('tcp_port'),
    udp_port:num('udp_port'),udp_interval_ms:num('udp_interval'),
    n2k_enabled:document.getElementById('n2k_en').checked,
    n2k_src_slave:num('n2k_slave'),n2k_interval_ms:num('n2k_interval'),
    n2k_heading_reg:num('n2k_head'),n2k_depth_reg:num('n2k_depth'),
    n2k_speed_reg:num('n2k_speed'),
    n2k_lat_int_reg:num('n2k_lat_i'),n2k_lat_frac_reg:num('n2k_lat_f'),
    n2k_lon_int_reg:num('n2k_lon_i'),n2k_lon_frac_reg:num('n2k_lon_f')
  };
  const st=document.getElementById('status');
  try{
    const r=await fetch('/api/config',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(body)
    });
    if(r.ok){st.className='ok';st.textContent='\u2713 Saved \u2014 reboot to apply all changes';}
    else    {st.className='err';st.textContent='\u2717 Save failed (HTTP '+r.status+')';}
  }catch(e){st.className='err';st.textContent='\u2717 '+e;}
}

function confirmReset(){
  if(!confirm('Reset all settings to factory defaults?'))return;
  fetch('/api/reset',{method:'POST'}).then(()=>setTimeout(()=>location.reload(),600));
}

async function loadLive(){
  try{
    const r=await fetch('/api/regs');
    if(r.ok){
      const d=await r.json();
      document.getElementById('livedata').textContent=JSON.stringify(d,null,2);
    }
  }catch(e){document.getElementById('livedata').textContent='Connection error: '+e;}
}

loadConfig();
loadLive();
setInterval(loadLive,2000);
</script>
</body>
</html>
)rawliteral";

// ---------------------------------------------------------------------------
// CORS helper
// ---------------------------------------------------------------------------

static void add_cors_headers()
{
    s_web.sendHeader("Access-Control-Allow-Origin",  "*");
    s_web.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    s_web.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ---------------------------------------------------------------------------
// Handler: GET /
// ---------------------------------------------------------------------------

static void handle_root()
{
    s_web.send_P(200, "text/html", HTML_PAGE);
}

// ---------------------------------------------------------------------------
// Handler: GET /api/config
// ---------------------------------------------------------------------------

static void handle_get_config()
{
    GatewayConfig &cfg = config_get();

    JsonDocument doc;
    doc["wifi_ssid"]         = cfg.wifi_ssid;
    doc["wifi_pass"]         = cfg.wifi_pass;
    doc["rtu_baud"]          = cfg.rtu_baud;
    doc["rtu_tx_pin"]        = cfg.rtu_tx_pin;
    doc["rtu_rx_pin"]        = cfg.rtu_rx_pin;
    doc["rtu_de_pin"]        = cfg.rtu_de_pin;
    JsonArray ids = doc["rtu_slave_ids"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.rtu_slave_count; i++) ids.add(cfg.rtu_slave_ids[i]);
    doc["rtu_reg_start"]     = cfg.rtu_reg_start;
    doc["rtu_reg_count"]     = cfg.rtu_reg_count;
    doc["rtu_poll_ms"]       = cfg.rtu_poll_ms;
    doc["tcp_port"]          = cfg.tcp_port;
    doc["udp_port"]          = cfg.udp_port;
    doc["udp_interval_ms"]   = cfg.udp_interval_ms;
    doc["n2k_enabled"]       = cfg.n2k_enabled;
    doc["n2k_src_slave"]     = cfg.n2k_src_slave;
    doc["n2k_heading_reg"]   = cfg.n2k_heading_reg;
    doc["n2k_depth_reg"]     = cfg.n2k_depth_reg;
    doc["n2k_speed_reg"]     = cfg.n2k_speed_reg;
    doc["n2k_lat_int_reg"]   = cfg.n2k_lat_int_reg;
    doc["n2k_lat_frac_reg"]  = cfg.n2k_lat_frac_reg;
    doc["n2k_lon_int_reg"]   = cfg.n2k_lon_int_reg;
    doc["n2k_lon_frac_reg"]  = cfg.n2k_lon_frac_reg;
    doc["n2k_interval_ms"]   = cfg.n2k_interval_ms;

    String json;
    serializeJson(doc, json);
    add_cors_headers();
    s_web.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Handler: POST /api/config
// ---------------------------------------------------------------------------

static void handle_post_config()
{
    if (!s_web.hasArg("plain")) {
        s_web.send(400, "text/plain", "Missing request body");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_web.arg("plain"));
    if (err) {
        s_web.send(400, "text/plain",
                   String("JSON parse error: ") + err.c_str());
        return;
    }

    GatewayConfig &cfg = config_get();

    // WiFi
    if (doc["wifi_ssid"].is<const char *>())
        strncpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid) - 1);
    if (doc["wifi_pass"].is<const char *>())
        strncpy(cfg.wifi_pass, doc["wifi_pass"], sizeof(cfg.wifi_pass) - 1);

    // RTU
    if (!doc["rtu_baud"].isNull())       cfg.rtu_baud       = doc["rtu_baud"].as<uint32_t>();
    if (!doc["rtu_tx_pin"].isNull())     cfg.rtu_tx_pin     = doc["rtu_tx_pin"].as<uint8_t>();
    if (!doc["rtu_rx_pin"].isNull())     cfg.rtu_rx_pin     = doc["rtu_rx_pin"].as<uint8_t>();
    if (!doc["rtu_de_pin"].isNull())     cfg.rtu_de_pin     = doc["rtu_de_pin"].as<uint8_t>();
    if (doc["rtu_slave_ids"].is<JsonArray>()) {
        JsonArray arr = doc["rtu_slave_ids"].as<JsonArray>();
        uint8_t cnt = 0;
        for (JsonVariant v : arr) {
            if (cnt >= GATEWAY_MAX_SLAVES) break;
            cfg.rtu_slave_ids[cnt++] = v.as<uint8_t>();
        }
        cfg.rtu_slave_count = cnt;
    }
    if (!doc["rtu_reg_start"].isNull())  cfg.rtu_reg_start  = doc["rtu_reg_start"].as<uint16_t>();
    if (!doc["rtu_reg_count"].isNull())  cfg.rtu_reg_count  = doc["rtu_reg_count"].as<uint16_t>();
    if (!doc["rtu_poll_ms"].isNull())    cfg.rtu_poll_ms    = doc["rtu_poll_ms"].as<uint32_t>();

    // TCP / UDP
    if (!doc["tcp_port"].isNull())       cfg.tcp_port       = doc["tcp_port"].as<uint16_t>();
    if (!doc["udp_port"].isNull())       cfg.udp_port       = doc["udp_port"].as<uint16_t>();
    if (!doc["udp_interval_ms"].isNull())cfg.udp_interval_ms= doc["udp_interval_ms"].as<uint32_t>();

    // N2K
    if (!doc["n2k_enabled"].isNull())    cfg.n2k_enabled    = doc["n2k_enabled"].as<bool>();
    if (!doc["n2k_src_slave"].isNull())  cfg.n2k_src_slave  = doc["n2k_src_slave"].as<uint8_t>();
    if (!doc["n2k_heading_reg"].isNull())cfg.n2k_heading_reg= doc["n2k_heading_reg"].as<uint16_t>();
    if (!doc["n2k_depth_reg"].isNull())  cfg.n2k_depth_reg  = doc["n2k_depth_reg"].as<uint16_t>();
    if (!doc["n2k_speed_reg"].isNull())  cfg.n2k_speed_reg  = doc["n2k_speed_reg"].as<uint16_t>();
    if (!doc["n2k_lat_int_reg"].isNull())cfg.n2k_lat_int_reg= doc["n2k_lat_int_reg"].as<uint16_t>();
    if (!doc["n2k_lat_frac_reg"].isNull())cfg.n2k_lat_frac_reg=doc["n2k_lat_frac_reg"].as<uint16_t>();
    if (!doc["n2k_lon_int_reg"].isNull())cfg.n2k_lon_int_reg= doc["n2k_lon_int_reg"].as<uint16_t>();
    if (!doc["n2k_lon_frac_reg"].isNull())cfg.n2k_lon_frac_reg=doc["n2k_lon_frac_reg"].as<uint16_t>();
    if (!doc["n2k_interval_ms"].isNull())cfg.n2k_interval_ms= doc["n2k_interval_ms"].as<uint32_t>();

    config_save();

    add_cors_headers();
    s_web.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// Handler: GET /api/regs
// ---------------------------------------------------------------------------

static void handle_get_regs()
{
    GatewayConfig &cfg = config_get();

    JsonDocument doc;
    doc["ts"] = millis();
    JsonArray slaves = doc["slaves"].to<JsonArray>();

    for (uint8_t s = 0; s < cfg.rtu_slave_count; s++) {
        uint8_t sid = cfg.rtu_slave_ids[s];
        JsonObject obj = slaves.add<JsonObject>();
        obj["id"]    = sid;
        obj["valid"] = reg_map_is_valid(sid);
        JsonArray regs = obj["regs"].to<JsonArray>();
        for (uint16_t r = 0; r < cfg.rtu_reg_count; r++) {
            regs.add(reg_map_get(sid, r));
        }
    }

    String json;
    serializeJson(doc, json);
    add_cors_headers();
    s_web.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Handler: POST /api/reset
// ---------------------------------------------------------------------------

static void handle_reset()
{
    config_reset_defaults();
    config_save();
    add_cors_headers();
    s_web.send(200, "application/json",
               "{\"ok\":true,\"msg\":\"Defaults restored – reboot to apply\"}");
}

// ---------------------------------------------------------------------------
// Handler: OPTIONS (CORS preflight)
// ---------------------------------------------------------------------------

static void handle_options()
{
    add_cors_headers();
    s_web.send(204);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool webconfig_init()
{
    s_web.on("/",           HTTP_GET,     handle_root);
    s_web.on("/api/config", HTTP_GET,     handle_get_config);
    s_web.on("/api/config", HTTP_POST,    handle_post_config);
    s_web.on("/api/config", HTTP_OPTIONS, handle_options);
    s_web.on("/api/regs",   HTTP_GET,     handle_get_regs);
    s_web.on("/api/regs",   HTTP_OPTIONS, handle_options);
    s_web.on("/api/reset",  HTTP_POST,    handle_reset);
    s_web.onNotFound([]() {
        s_web.send(404, "text/plain", "Not found");
    });
    s_web.begin(80);
    Serial.println("[WEB] Config UI on port 80");
    return true;
}

void webconfig_update()
{
    s_web.handleClient();
}
