/*
 * ESP32 + CJMCU-811 (sensor CCS811) -> plataforma ECS IoT por MQTT
 *
 * Publica cada PUBLISH_INTERVAL_MS en:
 *   ecs/<slug>/co2   {"value": <ppm>, "timestamp": "...", "token": "..."}
 *   ecs/<slug>/tvoc  {"value": <ppb>, "timestamp": "...", "token": "..."}
 *
 * Configuración (slug, token, broker, WiFi) en include/config.h
 *
 * Conexiones:
 *   CJMCU-811   ESP32
 *   VCC     ->  3V3
 *   GND     ->  GND
 *   SDA     ->  GPIO 21
 *   SCL     ->  GPIO 22
 *   WAK     ->  GND   (¡obligatorio! si queda al aire el sensor no responde)
 *   ADD     ->  GND   (dirección I2C 0x5A; a 3V3 sería 0x5B)
 *   RST, INT -> sin conectar
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Adafruit_CCS811.h"

#include "config.h"

#define PIN_SDA      21
#define PIN_SCL      22
#define CCS811_ADDR  0x5A   // Cambia a 0x5B si ADD está a 3V3

Adafruit_CCS811 ccs;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

uint16_t ultimoEco2 = 0;
uint16_t ultimoTvoc = 0;
bool hayLectura = false;
unsigned long ultimoEnvio = 0;

const char* calidadAire(uint16_t eco2) {
  if (eco2 < 800)  return "Buena";
  if (eco2 < 1000) return "Aceptable";
  if (eco2 < 1500) return "Mala";
  return "Muy mala (ventilar)";
}

void escanearI2C() {
  Serial.println("Escaneando bus I2C...");
  byte encontrados = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Dispositivo en 0x%02X\n", addr);
      encontrados++;
    }
  }
  if (encontrados == 0) Serial.println("  Ningún dispositivo encontrado. Revisa el cableado y WAK a GND.");
}

void conectarWiFi() {
  if (strlen(WIFI_SSID) > 0) {
    Serial.printf("Conectando a WiFi %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi conectado, IP: %s\n", WiFi.localIP().toString().c_str());
      return;
    }
    Serial.println("No se pudo conectar con WIFI_SSID, se abre el portal de configuración.");
  }

  // Si no hay WiFi guardado, crea la red "DEVICE_AP_TAG": conéctate desde el
  // celular, se abre una página para elegir tu WiFi y escribir la contraseña.
  WiFiManager wifiManager;
  Serial.printf("Si es la primera vez, conéctate a la red WiFi \"%s\" para configurarlo.\n", DEVICE_AP_TAG);
  if (!wifiManager.autoConnect(DEVICE_AP_TAG)) {
    Serial.println("No se pudo conectar a WiFi, reiniciando...");
    delay(3000);
    ESP.restart();
  }
  Serial.printf("WiFi conectado, IP: %s\n", WiFi.localIP().toString().c_str());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("Conectando a MQTT %s:%d... ", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    String clientId = String(DEVICE_SLUG) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("conectado.");
    } else {
      Serial.printf("falló (rc=%d), reintento en 5 s\n", mqttClient.state());
      delay(5000);
    }
  }
}

String horaISO() {
  struct tm t;
  if (!getLocalTime(&t, 100)) return "";   // sin hora aún: el servidor usa la de recepción
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
  return String(buf);
}

void publicar(const char* variable, float valor) {
  StaticJsonDocument<192> doc;
  doc["value"] = valor;
  String ts = horaISO();
  if (ts.length() > 0) doc["timestamp"] = ts;
  doc["token"] = DEVICE_TOKEN;

  char payload[192];
  size_t len = serializeJson(doc, payload);

  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s/%s", MQTT_TOPIC_PREFIX, DEVICE_SLUG, variable);
  bool ok = mqttClient.publish(topic, (uint8_t*)payload, len, false);
  Serial.printf("-> %s %s %s\n", topic, payload, ok ? "" : "(ERROR)");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nESP32 + CCS811 (CJMCU-811) -> ECS IoT");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);       // 100 kHz: el CCS811 usa clock stretching
  Wire.setTimeOut(1000);

  escanearI2C();

  if (!ccs.begin(CCS811_ADDR, &Wire)) {
    Serial.println("ERROR: no se encontró el CCS811. Revisa conexiones y dirección.");
    while (true) delay(1000);
  }
  ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);

  conectarWiFi();
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);

  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();

  // El sensor da un dato nuevo cada segundo; guardamos el último
  if (ccs.available() && !ccs.readData()) {
    ultimoEco2 = ccs.geteCO2();   // ppm (400 - 8192)
    ultimoTvoc = ccs.getTVOC();   // ppb (0 - 1187)
    hayLectura = true;
  }

  if (hayLectura && millis() - ultimoEnvio >= PUBLISH_INTERVAL_MS) {
    ultimoEnvio = millis();
    Serial.printf("eCO2: %4u ppm | TVOC: %4u ppb | Calidad: %s\n",
                  ultimoEco2, ultimoTvoc, calidadAire(ultimoEco2));
    publicar("co2", ultimoEco2);
    publicar("tvoc", ultimoTvoc);
  }

  delay(50);
}
