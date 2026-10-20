
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MAX30100_PulseOximeter.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <math.h>

// =====================================================
// ESP32-C3 SUPERMINI PIN CONFIGURATION
// =====================================================

#define I2C_SDA       8
#define I2C_SCL       9
#define TEMT6000_PIN  4
#define DHT_PIN       5
#define DS18B20_PIN   6
#define DHT_TYPE      DHT22

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

// =====================================================
// ESP32 ACCESS POINT
// =====================================================

const char* AP_SSID = "ESP32-Health-Monitor";
const char* AP_PASSWORD = "health123";

// Replace these with your 2.4 GHz Wi-Fi credentials.
const char* WIFI_SSID = "Project";
const char* WIFI_PASSWORD = "12345678";

// Public TEST broker: do not send sensitive health information.
const char* MQTT_HOST = "test.mosquitto.org";
const uint16_t MQTT_PORT = 1883;
const uint32_t MQTT_INTERVAL_MS = 5000;
const uint32_t MQTT_RETRY_MS = 10000;
const uint32_t WIFI_RETRY_MS = 15000;

WebServer server(80);
WiFiClient mqttTransport;
PubSubClient mqtt(mqttTransport);
String mqttTopic;
String mqttStatusTopic;
String mqttClientId;
uint32_t lastMQTTPublish = 0;
uint32_t lastMQTTRetry = 0;
uint32_t lastWiFiRetry = 0;
bool mqttEverAttempted = false;
bool wifiEverAttempted = false;


DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature bodyTempSensor(&oneWire);
PulseOximeter pox;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledReady = false;
bool pulseOximeterReady = false;
SemaphoreHandle_t i2cMutex = nullptr;
TaskHandle_t pulseTaskHandle = nullptr;
volatile uint32_t beatCount = 0;
volatile uint32_t lastBeatMs = 0;
volatile uint32_t pulseLastSampleMs = 0;
const uint32_t PULSE_STALE_MS = 8000;


// =====================================================
// SENSOR READINGS
// =====================================================

float roomTemperature = NAN;
float humidity = NAN;
float bodyTemperature = NAN;
float heartRate = NAN;
float spo2 = NAN;
float lightLux = NAN;

// EMA filter values
float roomTempFiltered = 0;
float humidityFiltered = 0;
float bodyTempFiltered = 0;
float heartRateFiltered = 0;
float spo2Filtered = 0;
float lightFiltered = 0;

bool roomTempReady = false;
bool humidityReady = false;
bool bodyTempReady = false;
bool heartRateReady = false;
bool spo2Ready = false;
bool lightReady = false;

// =====================================================
// FILTER SETTINGS
// =====================================================

const float HR_ALPHA = 0.20f;
const float SPO2_ALPHA = 0.15f;
const float ROOM_ALPHA = 0.20f;
const float HUMIDITY_ALPHA = 0.20f;
const float BODY_ALPHA = 0.20f;
const float LIGHT_ALPHA = 0.10f;

// =====================================================
// TIMING
// =====================================================

const uint32_t DHT_INTERVAL_MS = 2500;
const uint32_t BODY_INTERVAL_MS = 2000;
const uint32_t BODY_CONVERSION_MS = 800;
const uint32_t LIGHT_INTERVAL_MS = 1000;
const uint32_t DISPLAY_INTERVAL_MS = 2000;
const uint32_t REPORT_INTERVAL_MS = 5000;

uint32_t lastDHT = 0;
uint32_t lastBodyRequest = 0;
uint32_t lastLight = 0;
uint32_t lastDisplay = 0;
uint32_t lastReport = 0;

bool bodyConversionPending = false;

// =====================================================
// EMA FILTER
// =====================================================

float applyEMA(
  float input,
  float previous,
  float alpha,
  bool &initialized
) {
  if (!isfinite(input)) {
    return previous;
  }

  if (!initialized) {
    initialized = true;
    return input;
  }

  return alpha * input + (1.0f - alpha) * previous;
}

// =====================================================
// JSON HELPERS
// =====================================================


String jsonNumber(float value, uint8_t decimals = 2) {
  if (!isfinite(value)) {
    return "null";
  }

  return String(value, (unsigned int)decimals);
}


// =====================================================
// DASHBOARD
// =====================================================

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#101827">
<title>ESP32 Health Monitor</title>
<style>
*{box-sizing:border-box}
body{margin:0;background:#0b1020;color:#edf3ff;font:15px system-ui,Arial,sans-serif}
main{max-width:850px;margin:auto;padding:20px 14px}
header{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:20px}
h1{font-size:25px;margin:0}
.sub{font-size:12px;color:#9aabc4;margin-top:4px}
.status{font-size:12px;color:#ffc66d;padding:8px 10px;border:1px solid #293750;border-radius:20px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#ffc66d;margin-right:6px}
.dot.ok{background:#4ade80}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.card{background:#141e31;border:1px solid #293750;border-radius:14px;padding:17px;min-width:0}
.label{font-size:12px;color:#9aabc4}
.value{font-size:30px;font-weight:700;margin-top:8px;overflow-wrap:anywhere}
.unit{font-size:13px;color:#9aabc4;font-weight:400}
.note{font-size:11px;color:#9aabc4;margin-top:6px}
.actions{margin-top:15px}
button{background:#4de2d0;color:#07111f;border:0;border-radius:9px;padding:10px 14px;font-weight:650;cursor:pointer}
#details{font-size:12px;line-height:1.8;color:#9aabc4;margin-top:16px;overflow-wrap:anywhere}
footer{font-size:11px;color:#7f90aa;text-align:center;margin-top:22px}
@media(min-width:650px){.grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
</style>
</head>
<body>
<main>
<header>
<div>
<h1>Health Monitor</h1>
<div class="sub">ESP32-C3 live sensor dashboard</div>
</div>
<div class="status"><span id="dot" class="dot"></span><span id="connection">Connecting...</span></div>
</header>

<div class="grid">
<div class="card"><div class="label">Heart rate</div><div class="value"><span id="hr">--</span> <span class="unit">BPM</span></div><div class="note">MAX30100</div></div>
<div class="card"><div class="label">Blood oxygen</div><div class="value"><span id="spo2">--</span> <span class="unit">%</span></div><div class="note">MAX30100</div></div>
<div class="card"><div class="label">Temperature</div><div class="value"><span id="body">--</span> <span class="unit">°C</span></div><div class="note">DS18B20 sensor</div></div>
<div class="card"><div class="label">Room temperature</div><div class="value"><span id="room">--</span> <span class="unit">°C</span></div><div class="note">DHT22</div></div>
<div class="card"><div class="label">Humidity</div><div class="value"><span id="humidity">--</span> <span class="unit">%</span></div><div class="note">DHT22</div></div>
<div class="card"><div class="label">Light level</div><div class="value"><span id="light">--</span> <span class="unit">lux*</span></div><div class="note">TEMT6000 approximate</div></div>
</div>

<div class="actions">
<button id="refresh">Refresh readings</button>
<button id="download">Download CSV</button>
</div>

<div id="details">Waiting for sensor data...</div>
<footer>Prototype readings are not medical-grade measurements.</footer>
</main>
<script>
'use strict';
const $ = id => document.getElementById(id);
let lastData = null;
let busy = false;

function showValue(id, value, digits) {
  const n = Number(value);
  $(id).textContent =
    value === null || value === undefined || value === '' ||
    !Number.isFinite(n) ? '--' : n.toFixed(digits);
}

async function update() {
  if (busy) return;
  busy = true;
  try {
    const response = await fetch('/api?t=' + Date.now(), {
      cache: 'no-store',
      signal: AbortSignal.timeout(5000)
    });
    if (!response.ok) throw new Error('HTTP ' + response.status);

    const data = await response.json();
    lastData = data;

    showValue('hr', data.heartRate, 0);
    showValue('spo2', data.spo2, 0);
    showValue('body', data.bodyTemperature, 1);
    showValue('room', data.roomTemperature, 1);
    showValue('humidity', data.humidity, 1);
    showValue('light', data.lightLux, 1);

    $('connection').textContent =
      'Connected · ' + new Date().toLocaleTimeString();
    $('dot').classList.add('ok');

    $('details').textContent =
      'Uptime: ' + Math.floor(data.uptimeMs / 1000) +
      ' seconds | MQTT: ' + (data.mqttConnected ? 'connected' : 'offline') +
       ' | Topic: ' + data.mqttTopic + ' | Free heap: ' +
      Math.round(data.freeHeap / 1024) + ' KB';
  } catch (error) {
    $('connection').textContent = 'Connection error';
    $('dot').classList.remove('ok');
    $('details').textContent = error.message;
  } finally {
    busy = false;
  }
}

$('refresh').addEventListener('click', update);

$('download').addEventListener('click', () => {
  if (!lastData) return;

  const csv = [
    'timestamp,heartRate,spo2,bodyTemperature,roomTemperature,humidity,lightLux,uptimeMs',
    [
      new Date().toISOString(),
      lastData.heartRate,
      lastData.spo2,
      lastData.bodyTemperature,
      lastData.roomTemperature,
      lastData.humidity,
      lastData.lightLux,
      lastData.uptimeMs
    ].map(v => JSON.stringify(v ?? '')).join(',')
  ].join('\r\n');

  const link = document.createElement('a');
  const url = URL.createObjectURL(
    new Blob([csv], {type:'text/csv;charset=utf-8'})
  );
  link.href = url;
  link.download = 'esp32_health_reading.csv';
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});

update();
setInterval(update, 2000);
</script>
</body>
</html>
)HTML";

// =====================================================
// HTTP HANDLERS
// =====================================================

void handleDashboard() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handleSensorApi() {
  String json;
  json.reserve(300);

  json = "{";
  json += "\"roomTemperature\":" + jsonNumber(roomTemperature) + ",";
  json += "\"humidity\":" + jsonNumber(humidity) + ",";
  json += "\"bodyTemperature\":" + jsonNumber(bodyTemperature) + ",";
  json += "\"heartRate\":" + jsonNumber(heartRate) + ",";
  json += "\"spo2\":" + jsonNumber(spo2) + ",";
  json += "\"lightLux\":" + jsonNumber(lightLux) + ",";
  json += "\"mqttConnected\":";
  json += mqtt.connected() ? "true," : "false,";
  json += "\"mqttTopic\":\"" + mqttTopic + "\",";
  json += "\"wifiConnected\":";
  json += WiFi.status() == WL_CONNECTED ? "true," : "false,";
  json += "\"uptimeMs\":" + String(millis()) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"minFreeHeap\":" + String(ESP.getMinFreeHeap());
  json += "}";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", json);
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  bool started = WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println();
  Serial.println("========== WI-FI DASHBOARD ==========");
  Serial.print("AP startup: ");
  Serial.println(started ? "SUCCESS" : "FAILED");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("=====================================");

  server.on("/", HTTP_GET, handleDashboard);
  server.on("/api", HTTP_GET, handleSensorApi);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Open http://192.168.4.1/");
  });

  server.begin();
  Serial.println("HTTP server started");
}

// =====================================================
// WIFI + MQTT (keep sensor loop responsive)
// =====================================================

void startStationWiFi() {
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
    Serial.println("Set WIFI_SSID and WIFI_PASSWORD first!");
    return;
  }
  Serial.print("Connecting STA to: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiRetry = millis();
  wifiEverAttempted = true;
}

void maintainWiFi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!wifiEverAttempted || now - lastWiFiRetry >= WIFI_RETRY_MS) {
    startStationWiFi();
  }
}

String sensorJSON() {
  String json;
  json.reserve(300);
  json = "{";
  json += "\"roomTemperature\":" + jsonNumber(roomTemperature) + ",";
  json += "\"humidity\":" + jsonNumber(humidity) + ",";
  json += "\"bodyTemperature\":" + jsonNumber(bodyTemperature) + ",";
  json += "\"heartRate\":" + jsonNumber(heartRate) + ",";
  json += "\"spo2\":" + jsonNumber(spo2) + ",";
  json += "\"lightLux\":" + jsonNumber(lightLux) + ",";
  json += "\"uptimeMs\":" + String(millis()) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap());
  json += "}";
  return json;
}

void maintainMQTT(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt.connected()) mqtt.disconnect();
    return;
  }
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if (mqttEverAttempted && now - lastMQTTRetry < MQTT_RETRY_MS) return;
  mqttEverAttempted = true;
  lastMQTTRetry = now;
  Serial.print("MQTT connecting to ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  // QoS 0 Last Will. Non-retained sensor readings; retained online status.
  if (mqtt.connect(mqttClientId.c_str(), mqttStatusTopic.c_str(),
                   0, true, "offline")) {
    Serial.println("MQTT connected!");
    mqtt.publish(mqttStatusTopic.c_str(), "online", true);
    Serial.print("Topic: ");
    Serial.println(mqttTopic);
    lastMQTTPublish = now - MQTT_INTERVAL_MS;
  } else {
    Serial.print("MQTT connect failed, state=");
    Serial.println(mqtt.state());
  }
}

void publishMQTT(uint32_t now) {
  if (!mqtt.connected() || now - lastMQTTPublish < MQTT_INTERVAL_MS) return;
  lastMQTTPublish = now;
  String payload = sensorJSON();
  bool ok = mqtt.publish(mqttTopic.c_str(), payload.c_str(), false);
  Serial.print(ok ? "MQTT published: " : "MQTT publish FAILED: ");
  Serial.println(payload);
}

// =====================================================
// MAX30100 CALLBACK
// =====================================================

void onBeatDetected() {
  beatCount++;
  lastBeatMs = millis();
}

// =====================================================
// DHT22
// =====================================================

void readDHT() {
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (isfinite(newHumidity) &&
      isfinite(newTemperature) &&
      newHumidity >= 0.0f &&
      newHumidity <= 100.0f &&
      newTemperature > -40.0f &&
      newTemperature < 80.0f) {

    humidityFiltered = applyEMA(
      newHumidity, humidityFiltered, HUMIDITY_ALPHA, humidityReady
    );

    roomTempFiltered = applyEMA(
      newTemperature, roomTempFiltered, ROOM_ALPHA, roomTempReady
    );

    humidity = humidityFiltered;
    roomTemperature = roomTempFiltered;
  } else {
    Serial.println("DHT22 read failed");
  }
}

// =====================================================
// DS18B20 NON-BLOCKING CONVERSION
// =====================================================

void startBodyTemperatureConversion() {
  bodyTempSensor.requestTemperatures();
  lastBodyRequest = millis();
  bodyConversionPending = true;
}

void finishBodyTemperatureConversion() {
  float temperature = bodyTempSensor.getTempCByIndex(0);
  bodyConversionPending = false;

  if (temperature != DEVICE_DISCONNECTED_C &&
      isfinite(temperature) &&
      temperature > -55.0f &&
      temperature < 125.0f) {

    bodyTempFiltered = applyEMA(
      temperature, bodyTempFiltered, BODY_ALPHA, bodyTempReady
    );

    bodyTemperature = bodyTempFiltered;
  } else {
    Serial.println("DS18B20 read failed");
  }
}

// =====================================================
// TEMT6000
// =====================================================

void readLight() {
  uint16_t raw = analogRead(TEMT6000_PIN);
  float voltage = (raw / 4095.0f) * 3.3f;

  // Approximate conversion. Calibrate against a lux meter.
  float lux = voltage * 2000.0f;

  lightFiltered = applyEMA(
    lux, lightFiltered, LIGHT_ALPHA, lightReady
  );

  lightLux = lightFiltered;
}

// =====================================================
// MAX30100
// =====================================================

// Only the dedicated pulse task may access the PulseOximeter object.
// It continues running while network connections or DHT reads block loop().
void pulseSamplingTask(void *parameter) {
  uint32_t lastValues = 0;
  while (true) {
    if (pulseOximeterReady &&
        xSemaphoreTake(i2cMutex, 0) == pdTRUE) {
      pox.update();
      pulseLastSampleMs = millis();
      uint32_t now = millis();
      if (now - lastValues >= 1000) {
        lastValues = now;
        float hr = pox.getHeartRate();
        float oxygen = pox.getSpO2();
        bool recentBeat = lastBeatMs != 0 &&
                          (now - lastBeatMs < PULSE_STALE_MS);

        if (recentBeat && isfinite(hr) && hr >= 40.0f && hr <= 220.0f) {
          heartRateFiltered = applyEMA(hr, heartRateFiltered,
                                       HR_ALPHA, heartRateReady);
          heartRate = heartRateFiltered;
        } else if (!recentBeat) {
          heartRate = NAN;
          heartRateReady = false;
        }
        if (recentBeat && isfinite(oxygen) &&
            oxygen >= 70.0f && oxygen <= 100.0f) {
          spo2Filtered = applyEMA(oxygen, spo2Filtered,
                                  SPO2_ALPHA, spo2Ready);
          spo2 = spo2Filtered;
        } else if (!recentBeat) {
          spo2 = NAN;
          spo2Ready = false;
        }
      }
      xSemaphoreGive(i2cMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2) > 0 ? pdMS_TO_TICKS(2) : 1);
  }
}

// =====================================================
// OLED
// =====================================================

void updateDisplay() {
  if (!oledReady) return;

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("T:");
  if (isfinite(roomTemperature)) display.print(roomTemperature, 1);
  else display.print("--");
  display.print("C H:");
  if (isfinite(humidity)) display.print(humidity, 0);
  else display.print("--");
  display.print("%");

  display.setCursor(0, 10);
  display.print("L:");
  if (isfinite(lightLux)) display.print(lightLux, 0);
  else display.print("--");
  display.print(" B:");
  if (isfinite(bodyTemperature)) display.print(bodyTemperature, 1);
  else display.print("--");
  display.print("C");

  display.setCursor(0, 20);
  display.print("HR:");
  if (isfinite(heartRate)) display.print(heartRate, 0);
  else display.print("--");

  display.print(" O2:");
  if (isfinite(spo2)) display.print(spo2, 0);
  else display.print("--");
  display.print("%");

  display.display();
  xSemaphoreGive(i2cMutex);
}

// =====================================================
// SERIAL REPORT
// =====================================================

void printSensorData() {
  Serial.println();
  Serial.println("========== SENSOR DATA ==========");

  Serial.print("Room temperature: ");
  Serial.println(roomTemperature);

  Serial.print("Humidity: ");
  Serial.println(humidity);

  Serial.print("Light lux: ");
  Serial.println(lightLux);

  Serial.print("DS18B20 temperature: ");
  Serial.println(bodyTemperature);

  Serial.print("Heart rate: ");
  Serial.println(heartRate);

  Serial.print("SpO2: ");
  Serial.println(spo2);
  Serial.print("Beats detected: ");
  Serial.println(beatCount);
  Serial.print("Pulse task age (ms): ");
  Serial.println(millis() - pulseLastSampleMs);

  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

  Serial.println("================================");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("ESP32-C3 HEALTH MONITOR");

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Serial.println("I2C initialized");

  // OLED
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (!oledReady) {
    Serial.println("OLED initialization failed");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Health Monitor");
    display.println("Starting...");
    display.display();
    Serial.println("OLED initialized");
  }

  // DHT22
  dht.begin();
  Serial.println("DHT22 initialized");

  // DS18B20
  bodyTempSensor.begin();
  bodyTempSensor.setWaitForConversion(false);

  Serial.print("DS18B20 devices: ");
  Serial.println(bodyTempSensor.getDeviceCount());

  // TEMT6000
  analogReadResolution(12);
  pinMode(TEMT6000_PIN, INPUT);
  Serial.println("TEMT6000 initialized");

  // Create I2C mutex before starting the dedicated sampling task.
  i2cMutex = xSemaphoreCreateMutex();
  if (i2cMutex == nullptr) {
    Serial.println("FATAL: I2C mutex allocation failed");
    while (true) delay(1000);
  }

  // MAX30100
  Serial.print("Initializing MAX30100...");

  pulseOximeterReady = pox.begin();

  if (pulseOximeterReady) {
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
    Serial.println("SUCCESS");
    BaseType_t taskResult = xTaskCreate(
      pulseSamplingTask, "max30100", 4096, nullptr, 3, &pulseTaskHandle);
    if (taskResult != pdPASS) {
      Serial.println("FAILED to start pulse sampling task");
      pulseOximeterReady = false;
    }
  } else {
    Serial.println("FAILED");
  }

  // Start local dashboard and connect to the internet via STA.
  startAccessPoint();
  // Each ESP32 receives its own topic derived from its unique MAC.
  String suffix = WiFi.macAddress();
  suffix.replace(":", "");
  suffix.toLowerCase();
  mqttClientId = "esp32c3-health-" + suffix;
  mqttTopic = "esp32c3/health/" + suffix + "/sensors";
  mqttStatusTopic = "esp32c3/health/" + suffix + "/status";
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(768);
  mqtt.setKeepAlive(20);
  mqtt.setSocketTimeout(1);
  startStationWiFi();

  // Initial readings
  readDHT();
  readLight();
  startBodyTemperatureConversion();

  lastDHT = millis();
  lastLight = millis();
  lastDisplay = millis();
  lastReport = millis();

  Serial.println("Setup complete");
  Serial.println("Connect to ESP32-Health-Monitor");
  Serial.println("Open http://192.168.4.1/");
  Serial.println("API: http://192.168.4.1/api");
  Serial.print("MQTT topic: ");
  Serial.println(mqttTopic);
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  // MAX30100 is sampled in its dedicated higher-priority FreeRTOS task.
  // Handle dashboard requests
  server.handleClient();

  uint32_t now = millis();
  maintainWiFi(now);
  maintainMQTT(now);
  publishMQTT(now);

  if (now - lastDHT >= DHT_INTERVAL_MS) {
    lastDHT = now;
    readDHT();
  }

  if (!bodyConversionPending &&
      now - lastBodyRequest >= BODY_INTERVAL_MS) {
    startBodyTemperatureConversion();
  }

  if (bodyConversionPending &&
      now - lastBodyRequest >= BODY_CONVERSION_MS) {
    finishBodyTemperatureConversion();
  }

  if (now - lastLight >= LIGHT_INTERVAL_MS) {
    lastLight = now;
    readLight();
  }

  if (now - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = now;
    printSensorData();
  }

  if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
    lastDisplay = now;
    updateDisplay();
  }

  delay(1);
}
