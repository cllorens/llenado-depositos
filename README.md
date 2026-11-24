# Medidor de llenado de depósitos IBC para Home Assistant

Firmware para medir el nivel de agua de depósitos IBC de 1000 L con un sensor ultrasónico
estanco en el tapón superior. Publica nivel, porcentaje, litros y estado de las pilas por MQTT,
con **autodiscover de Home Assistant** (las entidades aparecen solas). Funciona a pilas: mide,
publica y entra en deep sleep.

- **Placa:** Seeed Studio XIAO ESP32-C6
- **Sensor:** DFRobot A02YYUW (SEN0311), ultrasónico estanco, UART, 30–4500 mm
- **Entorno:** PlatformIO + Arduino 3.x (plataforma [pioarduino](https://github.com/pioarduino/platform-espressif32))

## Funcionamiento

1. Despierta cada `INTERVALO_S` (300 s por defecto).
2. Arranca el WiFi y, mientras conecta, alimenta el sensor desde un GPIO y toma 9 lecturas (mediana).
3. Publica en MQTT, apaga el sensor y vuelve a dormir.

El sensor no tiene modo de bajo consumo, así que se alimenta desde el pin D0 y solo está encendido
durante la medida.

## Conexiones

| A02YYUW | XIAO ESP32-C6 |
|---|---|
| VCC (rojo) | D0 (GPIO0): alimentación conmutada (el sensor gasta ≤ 8 mA) |
| GND (negro) | GND |
| TX (blanco) | D2 (GPIO2): datos a 9600 8N1 |
| RX (amarillo) | sin conectar (así el sensor da la medida filtrada) |

**Pilas (3 × AA) al pin 5V (VBUS):**

```
pila+ ──┬── diodo Schottky (1N5817 / SS14) ──► 5V del XIAO
        │
       1 MΩ
        ├──────────► D1 (GPIO1, ADC)
       1 MΩ   100 nF
        │       │
GND ────┴───────┘
```

- El **diodo** evita que el USB (5 V) cargue las pilas alcalinas si se conectan los dos a la vez.
- El **divisor 1:2** va del lado de las pilas, antes del diodo, para medir su tensión real. El ADC del
  C6 lee hasta ~3,1 V. Consume unos 2 µA. El condensador de 100 nF es necesario para que el ADC lea
  bien con resistencias tan altas.

## Entidades en Home Assistant

| Entidad | Tipo | Descripción |
|---|---|---|
| Distancia | sensor (mm) | Distancia del sensor a la superficie del agua |
| Nivel | sensor (cm) | Altura de agua sobre el fondo |
| Llenado | sensor (%) | 0 % = vacío, 100 % = "lleno" calibrado |
| Volumen | sensor (L) | Litros estimados |
| Batería | sensor (%) | Carga de las pilas |
| Tensión pilas | diagnóstico (V) | |
| WiFi RSSI | diagnóstico (dBm) | |
| IP | diagnóstico | IP obtenida por DHCP |
| Estado sensor | diagnóstico | OK / Sin respuesta / Fuera de rango / ... |
| Mantenimiento | interruptor | ON: no duerme, mide cada 10 s y acepta OTA |
| Calibrar lleno | pulsador | La distancia actual pasa a ser la de "lleno" |
| Capacidad llena | número (L) | Litros que hay cuando está "lleno" |
| Distancia lleno | número (mm) | Distancia con el depósito lleno |
| Distancia vacío | número (mm) | Distancia hasta el fondo |

Si no llegan datos en 3 intervalos, los sensores pasan a "no disponible".

## Calibración

Desde Home Assistant, sin tocar el código:

1. **Distancia vacío:** distancia del sensor al fondo (medida a mano o con el depósito vacío).
2. Con el depósito lleno hasta donde se suele llenar, pulsar **Calibrar lleno**.
3. **Capacidad llena:** litros que hay en ese punto (normalmente no se llena a tope; por ejemplo 950 L).

Los valores se guardan en la flash (NVS) y sobreviven a reinicios y actualizaciones. Las órdenes se
publican con *retain*: si la placa está dormida, se aplican en el siguiente despertar.

El volumen se calcula de forma lineal (un IBC es prácticamente un prisma):
`llenado = (vacío − distancia) / (vacío − lleno)`, limitado a 0–100 %.

## Instalación

1. Copiar `include/secrets.h.ejemplo` como `include/secrets.h` y poner los datos del WiFi y del broker
   MQTT. `secrets.h` está en `.gitignore`.
2. Revisar `include/settings.h`: intervalo, pilas (`BAT_FACTOR`, `BAT_V_LLENA`, `BAT_V_VACIA`),
   antena (`ANTENA_EXTERNA`) y `MODO_DEPURACION`.
3. Compilar y grabar:

```bash
pio run -e deposito1 -t upload                                  # por USB
pio run -e deposito1_ota -t upload --upload-port <IP>           # por WiFi (Mantenimiento en ON)
```

Hay un entorno por depósito (`deposito1`, `deposito2`...). Cada uno cambia `DEPOSITO_NUM`, y con él
el nombre del dispositivo, el hostname y los topics MQTT. Para añadir otro, se copia un bloque de
`platformio.ini`.

### Modo depuración

Con `MODO_DEPURACION 1` en `settings.h`, la placa no duerme, deja el sensor encendido, publica cada
segundo y acepta OTA. Para la instalación definitiva, ponerlo a 0.

### Log

Por el USB (115200) y por UDP broadcast al puerto 1024 de la red local.

## Notas

- **Plataforma:** se usa pioarduino 54.03.21 (Arduino 3.2.1). La plataforma oficial `espressif32`
  no soporta el ESP32-C6 con Arduino.
- **esptool:** el esptool 5.0.0-dev1 de esa versión falla con `click` ≥ 8.2 (error
  `get_metavar() missing ... 'ctx'`). Solución: instalar click 8.1.8 en el Python de PlatformIO:
  `~/.platformio/penv/Scripts/python -m pip install click==8.1.8`.
- **USB:** el XIAO ESP32-C6 usa el USB nativo del chip (USB Serial/JTAG) y no necesita drivers.
  Mientras duerme, el puerto desaparece. Para grabar en ese caso: mantener BOOT, pulsar RESET y
  soltar BOOT. Después de grabar en ese modo hay que **pulsar RESET**, porque el reset por RTS no lo
  saca del modo descarga.
- **Antena:** el XIAO C6 tiene un conmutador de RF. GPIO3 lo activa y GPIO14 elige entre la antena
  cerámica y la externa (U.FL).
- `lib/ha_mqtt_discover`: funciones de autodiscover de Home Assistant sobre PubSubClient (sensor,
  interruptor, número, pulsador...).
