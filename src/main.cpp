#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "EmonLib.h"

EnergyMonitor emon;
AsyncWebServer server(80);

// AP credentials
const char* ssid     = "SmartEnergyMeter";
const char* password = "111111111";

// Calibration (tune voltage and current here)
#define vCalibration 225     // Adjust for your mains scaling
#define currCalibration 48   // Software scaling for SCT-103 + burden

// Offsets
#define VOLTAGE_OFFSET -3.0     // Subtract 3V
#define CURRENT_OFFSET -0.4     // Subtract 0.4A

// Energy accumulation
float kWh = 0;
unsigned long lastMillis = 0;

// Measurement struct
struct Measurement {
  float Vrms;
  float Irms;
  float Power;
};

// Take one measurement
Measurement measureSensors() {
  emon.calcVI(20, 200);  // measure ~10 cycles

  // Apply offsets
  float Vrms = emon.Vrms + VOLTAGE_OFFSET;
  float Irms = emon.Irms + CURRENT_OFFSET;

  // Avoid negatives when idle
  if (Vrms < 0) Vrms = 0;
  if (Irms < 0) Irms = 0;

  // Recompute power from adjusted values
  float power = Vrms * Irms;

  // Integrate energy
  kWh += power * (millis() - lastMillis) / 3600000.0;
  lastMillis = millis();

  Measurement m;
  m.Vrms = Vrms;
  m.Irms = Irms;
  m.Power = power;
  return m;
}

// Minimal responsive web page
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Smart Meter</title>
<style>
body{background:#111;color:#0ff;font-family:sans-serif;text-align:center;margin:0;padding:0;}
h1{padding:10px;}
.card{margin:10px;padding:15px;background:#222;border-radius:10px;display:inline-block;min-width:140px;}
.val{font-size:2rem;color:#0ff;}
</style>
</head>
<body>
<h1>Smart Meter</h1>
<div class="card">Voltage<br><span id="voltage" class="val">0.00 V</span></div>
<div class="card">Current<br><span id="current" class="val">0.000 A</span></div>
<div class="card">Power<br><span id="power" class="val">0.00 W</span></div>
<div class="card">Energy<br><span id="kwh" class="val">0.000 kWh</span></div>

<script>
async function update(){
  try{
    const r = await fetch('/data');
    const j = await r.json();
    document.getElementById('voltage').innerText = j.Vrms.toFixed(2)+' V';
    document.getElementById('current').innerText = j.Irms.toFixed(3)+' A';
    document.getElementById('power').innerText = j.Power.toFixed(2)+' W';
    document.getElementById('kwh').innerText = j.kWh.toFixed(3)+' kWh';
  }catch(e){console.log(e);}
  setTimeout(update,200); // fast refresh ~5 fps
}
update();
</script>
</body>
</html>
)rawliteral";
}

void setup() {
  Serial.begin(115200);

  // Initialize EmonLib
  emon.voltage(34, vCalibration, 1.7);  // Voltage pin
  emon.current(35, currCalibration);    // Current pin

  lastMillis = millis();

  // Start AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); 
  Serial.println(WiFi.softAPIP());

  // Web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200,"text/html",htmlPage());
  });

  // JSON data
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    Measurement m = measureSensors();
    String js = "{";
    js += "\"Vrms\":"+String(m.Vrms,2)+",";
    js += "\"Irms\":"+String(m.Irms,3)+",";
    js += "\"Power\":"+String(m.Power,2)+",";
    js += "\"kWh\":"+String(kWh,3);
    js += "}";
    request->send(200,"application/json",js);
  });

  server.begin();
}

void loop() {
  // nothing needed, async handles everything
}
