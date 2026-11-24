#pragma once
#include <Arduino.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Deposito (DEPOSITO_NUM lo pone platformio.ini en cada entorno)
// ---------------------------------------------------------------------------
#ifndef DEPOSITO_NUM
#define DEPOSITO_NUM 1
#endif

#define STR_(x) #x
#define STR(x)  STR_(x)

#define DEPOSITO_ID      "deposito_" STR(DEPOSITO_NUM)   // topics MQTT y client ID
#define DEPOSITO_NOMBRE  "Depósito " STR(DEPOSITO_NUM)   // nombre del dispositivo en HA
#define OTA_HOSTNAME     "Deposito-" STR(DEPOSITO_NUM)

// ---------------------------------------------------------------------------
// Pines (Seeed XIAO ESP32-C6)
// ---------------------------------------------------------------------------
#define PIN_SENSOR_VCC   D0     // alimentacion del A02YYUW (el sensor gasta <= 8 mA)
#define PIN_SENSOR_TX    D2     // TX del sensor -> el ESP32 lee aqui
// El RX del sensor (amarillo) queda SIN CONECTAR: asi da la medida filtrada.
#define PIN_BATERIA      D1     // GPIO1 (ADC1_CH1): tension de las pilas por divisor resistivo

// Antena: 0 = ceramica integrada, 1 = conector U.FL externo
#define ANTENA_EXTERNA   0

// ---------------------------------------------------------------------------
// Baterias: 3 pilas AA alcalinas. Divisor 1:2 (pila+ -- 1 M -- D1 -- 1 M -- GND,
// con 100 nF de D1 a GND: el ADC necesita una fuente de baja impedancia al muestrear).
// El ADC del C6 lee hasta ~3,1 V: con 1:2 llega a 6,2 V de pila.
// ---------------------------------------------------------------------------
#define BAT_FACTOR       2.0f   // Vpila = Vpin * BAT_FACTOR (ajustar comparando con un polimetro)
#define BAT_V_LLENA      4.8f   // 3 x 1,6 V (pilas nuevas)   -> 100 %
#define BAT_V_VACIA      3.3f   // 3 x 1,1 V (agotadas)       -> 0 %
#define BAT_MUESTRAS     16

// ---------------------------------------------------------------------------
// Calibracion del deposito: VALORES INICIALES. Se cambian desde Home Assistant
// ("Calibrar lleno", "Capacidad llena", "Distancia lleno", "Distancia vacío") y se guardan
// en la flash de la placa (NVS), asi que sobreviven a reinicios y a grabar firmware.
// Distancias desde la cara del sensor (en el tapon) hasta la superficie del agua.
// ---------------------------------------------------------------------------
#define DIST_VACIO_MM    1050   // distancia con el deposito vacio (hasta el fondo)
#define DIST_LLENO_MM    120    // distancia con el deposito lleno
#define CAPACIDAD_L      1000   // litros que hay cuando esta "lleno" (no se llena a tope)

// ---------------------------------------------------------------------------
// Ciclo de medida
// ---------------------------------------------------------------------------
// MODO DEPURACION: no duerme, sensor siempre alimentado, publica cada segundo y acepta OTA.
// >>> Poner a 0 para la instalacion definitiva <<<
#define MODO_DEPURACION        0
#define INTERVALO_DEPURACION_MS 1000

#define USAR_DEEP_SLEEP        (!MODO_DEPURACION)  // 1: mide, publica y duerme; 0: siempre despierto (con OTA)
#define INTERVALO_S            300   // segundos entre medidas
#define INTERVALO_MANT_S       10    // segundos entre medidas en modo mantenimiento / despierto
#define DISCOVERY_CADA_CICLOS  12    // reenvia el autodiscover cada N despertares (y al encender)
#define SENSOR_ARRANQUE_MS     600   // espera tras dar alimentacion al sensor
#define SENSOR_MUESTRAS        9     // lecturas para la mediana
#define SENSOR_TIMEOUT_MS      3000  // tiempo maximo leyendo el sensor
#define WIFI_TIMEOUT_MS        15000

// ---------------------------------------------------------------------------
// Red
// ---------------------------------------------------------------------------
// WiFi y MQTT (SSID, contraseñas, broker) estan en secrets.h, que NO se sube al repositorio.
// Copiar include/secrets.h.ejemplo como include/secrets.h y rellenarlo.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Falta include/secrets.h: copiar include/secrets.h.ejemplo como include/secrets.h y rellenarlo"
#endif

const char* wifi_ssid     = WIFI_SSID;
const char* wifi_password = WIFI_PASSWORD;

// IP por DHCP. Para IP fija, descomentar FIXEDIP y poner la direccion.
//#define FIXEDIP 1
#ifdef FIXEDIP
IPAddress local_IP(192, 168, 115, 15 + DEPOSITO_NUM);
IPAddress gateway(192, 168, 115, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);
#endif

// ---------------------------------------------------------------------------
// MQTT (Home Assistant)
// ---------------------------------------------------------------------------
const char* mqtt_server   = MQTT_SERVER;
const uint16_t mqtt_port  = MQTT_PORT;
const char* mqtt_user     = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;
const char* clientID      = DEPOSITO_ID;
String MQTTBASE = "homeassistant";

// ---------------------------------------------------------------------------
// LOG por UDP broadcast (mismo puerto que el inversor)
// ---------------------------------------------------------------------------
const uint16_t logPort   = 1024;
const char* LOG_source   = OTA_HOSTNAME ": ";
