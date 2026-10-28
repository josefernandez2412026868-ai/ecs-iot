// Copia este archivo a "config.h" (misma carpeta) y completa tus valores.
// config.h queda ignorado por git (contiene el token del dispositivo) — ver .gitignore.
#pragma once

// --- Identidad del dispositivo (la genera la plataforma al registrar el Device) ---
// `seed_calidad_aire` crea "esp32-ccs811" e imprime su token, o regístralo desde
// Dispositivos -> "+ Agregar sensor".
#define DEVICE_SLUG   "esp32-ccs811"
#define DEVICE_TOKEN  "paste-token-here"
#define DEVICE_AP_TAG "esp32-ccs811"   // nombre del WiFi de configuración (WiFiManager)

// --- WiFi (opcional). Vacío = se usa el portal de configuración de WiFiManager ---
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// --- Broker MQTT ---
// Local (docker-compose): IP de la PC que corre Docker en tu red (no "localhost")
// y el puerto MOSQUITTO_HOST_PORT de tu .env.
#define MQTT_BROKER_HOST  "192.168.1.100"
#define MQTT_BROKER_PORT  11883
#define MQTT_USERNAME     "ecs"                     // MQTT_USERNAME de tu .env
#define MQTT_PASSWORD     "change-me-mqtt-password" // MQTT_PASSWORD de tu .env
#define MQTT_TOPIC_PREFIX "ecs"                     // MQTT_TOPIC_PREFIX de tu .env

#define PUBLISH_INTERVAL_MS 5000

// --- NTP (para el timestamp de cada lectura) ---
#define NTP_SERVER     "pool.ntp.org"
#define GMT_OFFSET_SEC 0
#define DST_OFFSET_SEC 0
