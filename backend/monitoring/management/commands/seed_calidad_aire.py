"""Datos demo del proyecto de calidad de aire: 3 ESP32 (CH4, CO2, PM10), el VDF del PLC, el ESP32 real
con sensor CCS811 y usuarios de ejemplo.

Uso:
    docker compose exec web python manage.py seed_calidad_aire
    docker compose exec web python manage.py seed_calidad_aire --password MiClave123

Idempotente: se puede volver a correr sin duplicar ni pisar lo que ya exista
(solo se crean los usuarios/dispositivos/sensores que falten). Los tokens de los
dispositivos se toman de SIMULATOR_DEVICE_TOKENS (.env) para que el simulador
publique sin copiar nada a mano; si falta alguno se genera y se imprime la línea
lista para pegar en el .env.

Los umbrales de alerta deben coincidir con los de simulator/simulate.py (y, con
hardware real, con los que use Node-RED/PLC).

Los dispositivos reales (REAL_DEVICES) no los alimenta el simulador: el comando
imprime su token para pegarlo en el config.h del firmware (firmware/esp32_ccs811).
"""
import os
import secrets
import string

from django.contrib.auth.models import User
from django.core.management.base import BaseCommand
from django.db import transaction

from monitoring.models import Device, Profile, Sensor, Widget

PLACEHOLDER_TOKEN = "paste-token-here"

# slug, nombre, ubicación, [(variable, unidad, tipo de gadget, mín, máx, umbral de alerta)]
DEVICES = [
    ("esp32-ch4", "ESP32 CH4 (demo)", "", [("ch4", "ppm", Widget.TYPE_LINE_CHART, 0, 300, 100)]),
    ("esp32-co2", "ESP32 CO2 (demo)", "", [("co2", "ppm", Widget.TYPE_LINE_CHART, 0, 3000, 1000)]),
    ("esp32-pm10", "ESP32 PM10 (demo)", "", [("pm10", "µg/m³", Widget.TYPE_LINE_CHART, 0, 250, 50)]),
    (
        "plc-ventilador",
        "VDF de PLC",
        "Ventilación",
        [
            ("frecuencia_hz", "Hz", Widget.TYPE_METER, 0, 50, None),
            ("sensores_en_alerta", "de 3", Widget.TYPE_NUMERIC, 0, 3, None),
        ],
    ),
]

# Hardware real (firmware/esp32_ccs811): mismo formato que DEVICES
REAL_DEVICES = [
    (
        "esp32-ccs811",
        "ESP32 CCS811 (real)",
        "",
        [
            ("co2", "ppm", Widget.TYPE_LINE_CHART, 400, 3000, 1000),
            ("tvoc", "ppb", Widget.TYPE_LINE_CHART, 0, 1200, 500),
        ],
    ),
]

# username, rol, activo
USERS = [
    ("usuario1", Profile.ROLE_OWNER, True),
    ("usuario2", Profile.ROLE_VIEWER, True),
    ("usuario3", Profile.ROLE_OWNER, False),  # sin acceso: el login es rechazado
]


def parse_env_tokens(raw):
    tokens = {}
    for pair in raw.split(","):
        if ":" not in pair:
            continue
        slug, token = pair.split(":", 1)
        slug, token = slug.strip(), token.strip()
        if slug and token and token != PLACEHOLDER_TOKEN:
            tokens[slug] = token
    return tokens


def random_password():
    alphabet = string.ascii_letters + string.digits
    return "".join(secrets.choice(alphabet) for _ in range(10)) + "7"


class Command(BaseCommand):
    help = "Crea los dispositivos, gadgets y usuarios demo del proyecto de calidad de aire."

    def add_arguments(self, parser):
        parser.add_argument(
            "--password",
            help="Contraseña para los usuarios demo nuevos (por defecto se genera una distinta por usuario).",
        )

    @transaction.atomic
    def handle(self, *args, **options):
        env_tokens = parse_env_tokens(os.environ.get("SIMULATOR_DEVICE_TOKENS", ""))

        created_credentials = {}
        users = {}
        for username, role, active in USERS:
            user, created = User.objects.get_or_create(username=username, defaults={"email": f"{username}@example.com"})
            if created:
                password = options["password"] or random_password()
                user.set_password(password)
                user.is_active = active
                user.save()
                created_credentials[username] = password
            Profile.objects.update_or_create(user=user, defaults={"role": role})
            users[username] = user

        owner = users["usuario1"]
        device_tokens = []
        real_device_tokens = []
        real_slugs = {slug for slug, *_ in REAL_DEVICES}
        for slug, name, location, sensors in DEVICES + REAL_DEVICES:
            device, _ = Device.objects.get_or_create(
                slug=slug, defaults={"owner": owner, "name": name, "tag": slug, "location": location}
            )
            token = env_tokens.get(slug)
            if token and device.auth_token != token:
                device.auth_token = token
                device.save(update_fields=["auth_token"])
            if slug in real_slugs:
                real_device_tokens.append((slug, device.auth_token))
            else:
                device_tokens.append(f"{slug}:{device.auth_token}")

            for variable, unit, widget_type, low, high, alert in sensors:
                sensor, _ = Sensor.objects.get_or_create(device=device, variable=variable, defaults={"unit": unit})
                Widget.objects.get_or_create(
                    sensor=sensor,
                    defaults={
                        "widget_type": widget_type,
                        "min_value": low,
                        "max_value": high,
                        "alert_threshold": alert,
                    },
                )

        self.stdout.write(self.style.SUCCESS("Datos demo de calidad de aire listos."))
        for username, password in created_credentials.items():
            self.stdout.write(f"  usuario nuevo: {username} / {password}")
        if not created_credentials:
            self.stdout.write("  usuarios: ya existían (no se tocaron las contraseñas).")

        if any(slug not in env_tokens for slug, *_ in DEVICES):
            self.stdout.write("")
            self.stdout.write(self.style.WARNING("Faltan tokens en el .env. Pega esta línea y reinicia el simulador:"))
            self.stdout.write(f"SIMULATOR_DEVICE_TOKENS={','.join(device_tokens)}")
            self.stdout.write("  docker compose up -d --force-recreate simulator")

        for slug, token in real_device_tokens:
            self.stdout.write("")
            self.stdout.write(f"Token de {slug} (pégalo como DEVICE_TOKEN en el config.h del firmware):")
            self.stdout.write(f"  {token}")
