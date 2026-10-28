# Firmware ESP32 + CJMCU-811 (CCS811) — ECS IoT

Lee eCO2 (ppm) y TVOC (ppb) de un sensor CCS811 (módulo CJMCU-811) y los
publica cada 5 s en la plataforma siguiendo el "Contrato MQTT" del `README.md`
raíz:

```
ecs/esp32-ccs811/co2   {"value": 412, "timestamp": "...", "token": "..."}
ecs/esp32-ccs811/tvoc  {"value": 3,   "timestamp": "...", "token": "..."}
```

Es un proyecto [PlatformIO](https://platformio.org/) (VS Code + extensión
PlatformIO IDE); las librerías se descargan solas al compilar.

## Conexiones

```
      CJMCU-811 (CCS811)                    ESP32 DevKit
   ┌──────────────────────┐            ┌──────────────────┐
   │ VCC  ●───────────────┼────────────┼──● 3V3           │
   │ GND  ●───────────────┼─────┬──────┼──● GND           │
   │ SCL  ●───────────────┼─────┼──────┼──● GPIO 22 (SCL) │
   │ SDA  ●───────────────┼─────┼──────┼──● GPIO 21 (SDA) │
   │ WAK  ●───────────────┼─────┤      │                  │
   │ INT  ●  (sin conectar)│    │      │                  │
   │ RST  ●  (sin conectar)│    │      │                  │
   │ ADD  ●───────────────┼─────┘      │                  │
   └──────────────────────┘  (a GND)   └──────────────────┘
```

| CJMCU-811 | ESP32 | Nota |
|---|---|---|
| VCC | 3V3 | No usar 5V |
| GND | GND | |
| SDA | GPIO 21 | |
| SCL | GPIO 22 | |
| WAK | GND | **Obligatorio**: al aire el sensor no responde |
| ADD | GND | Dirección I2C 0x5A (a 3V3 sería 0x5B) |
| INT, RST | — | Sin conectar |

## Puesta en marcha

1. Registra el dispositivo en la plataforma. Lo más rápido es
   `docker compose exec web python manage.py seed_calidad_aire`, que crea
   `esp32-ccs811` con sus gadgets de CO2 y TVOC e imprime su token.
2. Copia `include/config.example.h` a `include/config.h` y completa el token,
   la IP de la PC que corre Docker, el puerto `MOSQUITTO_HOST_PORT` y la
   contraseña MQTT de tu `.env`. `config.h` está en `.gitignore`.
3. Abre esta carpeta (`firmware/esp32_ccs811`) en VS Code y usa la barra de
   PlatformIO: ✓ compilar, → subir, 🔌 monitor serie (115200 baud).
4. La primera vez, si no conoce ninguna red, el ESP32 crea el WiFi
   `esp32-ccs811`: conéctate desde el celular y elige tu red en el portal.

En el monitor serie verás algo así:

```
Escaneando bus I2C...
  Dispositivo en 0x5A
WiFi conectado, IP: 192.168.1.50
Conectando a MQTT 192.168.1.100:11883... conectado.
eCO2:  412 ppm | TVOC:    3 ppb | Calidad: Buena
-> ecs/esp32-ccs811/co2 {"value":412,"timestamp":"...","token":"..."}
```

## Notas

- El CCS811 tarda ~20 min en estabilizarse y ~48 h de uso la primera vez
  para dar valores fiables. 400 ppm es su mínimo de eCO2.
- El eCO2 es una estimación a partir de los compuestos orgánicos (TVOC), no
  una medida directa de CO2.
- **"Ningún dispositivo encontrado"** en el escáner I2C: revisa WAK a GND,
  SDA/SCL no invertidos y VCC a 3V3.
- El binario ocupa ~96% de la partición de app por defecto; si agregas más
  funciones, usa `board_build.partitions = huge_app.csv` en `platformio.ini`.
