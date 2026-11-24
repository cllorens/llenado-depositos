// Medidor de llenado de deposito IBC de 1000 L
// Seeed XIAO ESP32-C6 + DFRobot A02YYUW (alimentado desde D0, TX del sensor a D2)
// + tension de las pilas en D1 (divisor 1:2).
//
// Ciclo normal (USAR_DEEP_SLEEP = 1): despierta -> arranca el WiFi y a la vez mide ->
// publica en MQTT -> duerme INTERVALO_S. Con el interruptor "Mantenimiento" de HA en ON
// se queda despierto, mide cada INTERVALO_MANT_S y acepta OTA. Al ponerlo en OFF vuelve a dormir.
// MODO_DEPURACION (settings.h): no duerme y publica cada segundo.
//
// Calibracion desde HA: pulsador "Calibrar lleno" (la distancia medida pasa a ser la de lleno),
// numeros "Capacidad llena", "Distancia lleno" y "Distancia vacío". Las ordenes van con retain
// (la placa las lee al despertar) y los valores se guardan en NVS.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ha_mqtt_discover.h>
#include <esp_sleep.h>
#include <stdarg.h>

#include "settings.h"
#include "a02yyuw.h"

WiFiClient netClient;
PubSubClient mqtt(netClient);
WiFiUDP udp;
IPAddress broadcastIP;
Preferences prefs;

// Se conservan durante el deep sleep
RTC_DATA_ATTR uint32_t rtcCiclos = 0;
RTC_DATA_ATTR int32_t  rtcCanal = 0;      // canal y BSSID del AP para reconectar rapido
RTC_DATA_ATTR uint8_t  rtcBssid[6];

bool gMantenimiento = false;
bool gOtaIniciado = false;
uint32_t gUltimaMedida = 0;
A02Medida gMedida = {};                   // ultima medida (la usa "Calibrar lleno")

// Calibracion (NVS)
struct Calibracion {
  float vacioMm;
  float llenoMm;
  float capacidadL;
} gCal;

// Topics
#define BASE_SENSOR MQTTBASE + "/sensor/" DEPOSITO_ID
const String T_DIST    = BASE_SENSOR "/distancia";
const String T_NIVEL   = BASE_SENSOR "/nivel";
const String T_PCT     = BASE_SENSOR "/llenado";
const String T_LITROS  = BASE_SENSOR "/volumen";
const String T_RSSI    = BASE_SENSOR "/rssi";
const String T_IP      = BASE_SENSOR "/ip";
const String T_ESTADO  = BASE_SENSOR "/estado";
const String T_BAT_V   = BASE_SENSOR "/bateria_v";
const String T_BAT_PCT = BASE_SENSOR "/bateria";
const String T_MANT    = MQTTBASE + "/switch/" DEPOSITO_ID "/mantenimiento";
const String T_CAL_BTN = MQTTBASE + "/button/" DEPOSITO_ID "/calibrar_lleno";
const String T_CAPAC   = MQTTBASE + "/number/" DEPOSITO_ID "/capacidad";
const String T_D_LLENO = MQTTBASE + "/number/" DEPOSITO_ID "/dist_lleno";
const String T_D_VACIO = MQTTBASE + "/number/" DEPOSITO_ID "/dist_vacio";

// ---------------------------------------------------------------------------
// LOG: Serial + UDP broadcast (como el inversor)
// ---------------------------------------------------------------------------
void sendLog(const char* fmt, ...) {
  char msg[256];
  va_list ap; va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  Serial.println(msg);
  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(broadcastIP, logPort);
    udp.write((const uint8_t*)LOG_source, strlen(LOG_source));
    udp.write((const uint8_t*)msg, strlen(msg));
    udp.endPacket();
  }
}

// ---------------------------------------------------------------------------
// Antena del XIAO ESP32-C6: GPIO3 = activar conmutador RF (a 0), GPIO14 = 0 ceramica / 1 externa
// ---------------------------------------------------------------------------
void antena_init() {
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);
  pinMode(WIFI_ANT_CONFIG, OUTPUT);
  digitalWrite(WIFI_ANT_CONFIG, ANTENA_EXTERNA ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// Calibracion en NVS
// ---------------------------------------------------------------------------
void cal_cargar() {
  prefs.begin("deposito", true);
  gCal.vacioMm    = prefs.getFloat("vacio", DIST_VACIO_MM);
  gCal.llenoMm    = prefs.getFloat("lleno", DIST_LLENO_MM);
  gCal.capacidadL = prefs.getFloat("capac", CAPACIDAD_L);
  prefs.end();
}

void cal_guardar() {
  prefs.begin("deposito", false);
  prefs.putFloat("vacio", gCal.vacioMm);
  prefs.putFloat("lleno", gCal.llenoMm);
  prefs.putFloat("capac", gCal.capacidadL);
  prefs.end();
}

void publicar_valor(const String &base, float v, const char* fmt) {
  char s[16];
  snprintf(s, sizeof(s), fmt, v);
  mqtt.publish((base + "/state").c_str(), s, true);
}

void cal_publicar() {
  publicar_valor(T_CAPAC,   gCal.capacidadL, "%.0f");
  publicar_valor(T_D_LLENO, gCal.llenoMm,    "%.0f");
  publicar_valor(T_D_VACIO, gCal.vacioMm,    "%.0f");
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
void wifi_iniciar() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
#ifdef FIXEDIP
  WiFi.config(local_IP, gateway, subnet, dns);
#endif
  if (rtcCanal > 0) WiFi.begin(wifi_ssid, wifi_password, rtcCanal, rtcBssid);
  else              WiFi.begin(wifi_ssid, wifi_password);
}

bool wifi_esperar(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > timeoutMs) return false;
    // si el canal/BSSID guardado ya no vale, a mitad de tiempo probar con busqueda normal
    if (rtcCanal > 0 && millis() - t0 > timeoutMs / 2) {
      rtcCanal = 0;
      WiFi.disconnect();
      WiFi.begin(wifi_ssid, wifi_password);
    }
    delay(20);
  }
  rtcCanal = WiFi.channel();
  memcpy(rtcBssid, WiFi.BSSID(), 6);
  broadcastIP = WiFi.localIP() | ~WiFi.subnetMask();
  udp.begin(logPort);
  return true;
}

// ---------------------------------------------------------------------------
// Sueño
// ---------------------------------------------------------------------------
[[noreturn]] void dormir(uint32_t segundos) {
  sendLog("A dormir %lu s (despierto %lu ms)", (unsigned long)segundos, (unsigned long)millis());
  if (mqtt.connected()) { mqtt.loop(); mqtt.disconnect(); }
  delay(50);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  a02_apagar(PIN_SENSOR_VCC, PIN_SENSOR_TX);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)segundos * 1000000ULL);
  esp_deep_sleep_start();
  while (true) {}
}

bool modo_despierto() { return !USAR_DEEP_SLEEP || gMantenimiento; }

// ---------------------------------------------------------------------------
// MQTT: ordenes de HA
// ---------------------------------------------------------------------------
// Numero recibido: valida rango, guarda si cambia y confirma el estado
void orden_numero(const String &base, float &valor, const String &msg, float min, float max, const char* nombre) {
  if (msg.length() == 0) return;
  float v = msg.toFloat();
  if (v < min || v > max) {
    sendLog("%s: valor %s fuera de rango (%.0f..%.0f)", nombre, msg.c_str(), min, max);
  } else if (fabsf(v - valor) > 0.01f) {
    valor = v;
    cal_guardar();
    sendLog("%s = %.0f (guardado)", nombre, v);
  }
  publicar_valor(base, valor, "%.0f");
}

void calibrar_lleno() {
  // borrar la orden retenida para que no se repita en el siguiente despertar
  mqtt.publish((T_CAL_BTN + "/set").c_str(), "", true);
  if (gMedida.validas == 0) {
    sendLog("Calibrar lleno: sin medida valida, no se cambia nada");
    mqtt.publish((T_ESTADO + "/state").c_str(), "Calibración fallida: sin medida", true);
    return;
  }
  if (gMedida.mm + 50 > gCal.vacioMm) {
    sendLog("Calibrar lleno: %u mm no es menor que la distancia de vacio (%.0f mm)", gMedida.mm, gCal.vacioMm);
    mqtt.publish((T_ESTADO + "/state").c_str(), "Calibración fallida: distancia mayor que vacío", true);
    return;
  }
  gCal.llenoMm = gMedida.mm;
  cal_guardar();
  // dejar tambien el valor como orden retenida del numero, para que una orden antigua no lo pise
  char s[16]; snprintf(s, sizeof(s), "%.0f", gCal.llenoMm);
  mqtt.publish((T_D_LLENO + "/set").c_str(), s, true);
  publicar_valor(T_D_LLENO, gCal.llenoMm, "%.0f");
  sendLog("Calibrado LLENO: %.0f mm = %.0f L", gCal.llenoMm, gCal.capacidadL);
  char e[48]; snprintf(e, sizeof(e), "Calibrado lleno: %.0f mm", gCal.llenoMm);
  mqtt.publish((T_ESTADO + "/state").c_str(), e, true);
}

void callback(char* topic, byte* payload, unsigned int length) {
  // copiar antes de publicar nada: PubSubClient reutiliza el buffer
  String t(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (t == T_MANT + "/set") {
    bool on = (msg == "ON");
    if (on != gMantenimiento) sendLog("Mantenimiento: %s", on ? "ON" : "OFF");
    gMantenimiento = on;
    Switch_publish_state(&mqtt, T_MANT, gMantenimiento);
  } else if (t == T_CAL_BTN + "/set") {
    if (msg == "PRESS") calibrar_lleno();
  } else if (t == T_CAPAC + "/set") {
    orden_numero(T_CAPAC, gCal.capacidadL, msg, 1, 3000, "Capacidad llena");
  } else if (t == T_D_LLENO + "/set") {
    orden_numero(T_D_LLENO, gCal.llenoMm, msg, A02_MIN_MM, A02_MAX_MM, "Distancia lleno");
  } else if (t == T_D_VACIO + "/set") {
    orden_numero(T_D_VACIO, gCal.vacioMm, msg, A02_MIN_MM, A02_MAX_MM, "Distancia vacio");
  }
}

// ---------------------------------------------------------------------------
// MQTT: autodiscover y conexion
// ---------------------------------------------------------------------------
void discovery() {
  String mac = WiFi.macAddress();
  int exp = MODO_DEPURACION ? 60 : (3 * INTERVALO_S + 60);   // sin datos -> "no disponible"
  const char* mod = "IBC 1000 L + A02YYUW";
  const char* fab = "Charlie";
  const char* dev = DEPOSITO_NOMBRE;

  HASensor_discover(&mqtt, T_DIST,    "Distancia",     "mm",  "measurement", "distance",        dev, mod, fab, mac, "mdi:arrow-expand-vertical", exp);
  HASensor_discover(&mqtt, T_NIVEL,   "Nivel",         "cm",  "measurement", "distance",        dev, mod, fab, mac, "mdi:waves-arrow-up", exp);
  HASensor_discover(&mqtt, T_PCT,     "Llenado",       "%",   "measurement", "",                dev, mod, fab, mac, "mdi:storage-tank", exp);
  HASensor_discover(&mqtt, T_LITROS,  "Volumen",       "L",   "measurement", "volume_storage",  dev, mod, fab, mac, "mdi:water", exp);
  HASensor_discover(&mqtt, T_BAT_PCT, "Batería",       "%",   "measurement", "battery",         dev, mod, fab, mac, "mdi:battery", exp);
  HASensor_discover(&mqtt, T_BAT_V,   "Tensión pilas", "V",   "measurement", "voltage",         dev, mod, fab, mac, "mdi:flash-triangle", exp, "diagnostic");
  HASensor_discover(&mqtt, T_RSSI,    "WiFi RSSI",     "dBm", "measurement", "signal_strength", dev, mod, fab, mac, "mdi:wifi", exp, "diagnostic");
  HASensor_discover(&mqtt, T_IP,      "IP",            "",    "",            "",                dev, mod, fab, mac, "mdi:ip-network", exp, "diagnostic");
  HASensor_discover(&mqtt, T_ESTADO,  "Estado sensor", "",    "",            "",                dev, mod, fab, mac, "mdi:information-outline", exp, "diagnostic");
  // retain_cmd: la orden queda en el broker y la placa la lee al despertar
  Switch_discover(&mqtt, T_MANT, "Mantenimiento", gMantenimiento, dev, mod, fab, mac, "mdi:wrench", true);
  Button_discover(&mqtt, T_CAL_BTN, "Calibrar lleno", dev, mod, fab, mac, "mdi:car-coolant-level");
  Number_discover(&mqtt, T_CAPAC,   "Capacidad llena", 1, 3000, 1, "L",  dev, mod, fab, mac, "mdi:water-plus");
  Number_discover(&mqtt, T_D_LLENO, "Distancia lleno", A02_MIN_MM, A02_MAX_MM, 1, "mm", dev, mod, fab, mac, "mdi:arrow-collapse-up");
  Number_discover(&mqtt, T_D_VACIO, "Distancia vacío", A02_MIN_MM, A02_MAX_MM, 1, "mm", dev, mod, fab, mac, "mdi:arrow-collapse-down");
  cal_publicar();
  sendLog("Autodiscover enviado");
}

bool mqtt_conectar(int intentos) {
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(callback);
  for (int i = 0; i < intentos; i++) {
    if (mqtt.connect(clientID, mqtt_user, mqtt_password)) {
      mqtt.subscribe((T_MANT + "/set").c_str());
      mqtt.subscribe((T_CAL_BTN + "/set").c_str());
      mqtt.subscribe((T_CAPAC + "/set").c_str());
      mqtt.subscribe((T_D_LLENO + "/set").c_str());
      mqtt.subscribe((T_D_VACIO + "/set").c_str());
      return true;
    }
    sendLog("MQTT fallo, rc=%d", mqtt.state());
    delay(1000);
  }
  return false;
}

// Recibe las ordenes retenidas (mantenimiento, calibracion)
void leer_retenidos(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) { mqtt.loop(); delay(10); }
}

// ---------------------------------------------------------------------------
// Medida y publicacion
// ---------------------------------------------------------------------------
void medir() {
#if MODO_DEPURACION
  // sensor siempre alimentado y medida corta para poder publicar cada segundo
  gMedida = a02_medir(PIN_SENSOR_VCC, PIN_SENSOR_TX, SENSOR_ARRANQUE_MS, 3, 800, false);
#else
  gMedida = a02_medir(PIN_SENSOR_VCC, PIN_SENSOR_TX, SENSOR_ARRANQUE_MS, SENSOR_MUESTRAS, SENSOR_TIMEOUT_MS);
#endif
}

float leer_bateria() {
  uint32_t suma = 0;
  for (int i = 0; i < BAT_MUESTRAS; i++) suma += analogReadMilliVolts(PIN_BATERIA);
  return (float)suma / BAT_MUESTRAS / 1000.0f * BAT_FACTOR;
}

void publicar() {
  const A02Medida &m = gMedida;
  char v[24];

  float vb = leer_bateria();
  float pb = (vb - BAT_V_VACIA) / (BAT_V_LLENA - BAT_V_VACIA) * 100.0f;
  if (pb < 0) pb = 0;
  if (pb > 100) pb = 100;
  snprintf(v, sizeof(v), "%.2f", vb);  HASensor_publish_string(&mqtt, T_BAT_V, v);
  snprintf(v, sizeof(v), "%.0f", pb);  HASensor_publish_string(&mqtt, T_BAT_PCT, v);

  sendLog("Sensor: %s, %u mm (min %u, max %u), validas %u, errores %u | pilas %.2f V (%.0f %%)",
          a02_estado_str(m.estado), m.mm, m.minimo, m.maximo, m.validas, m.errores, vb, pb);

  const char* estado = a02_estado_str(m.estado);
  if (m.validas > 0) {
    float util  = gCal.vacioMm - gCal.llenoMm;
    float nivel = gCal.vacioMm - m.mm;                 // mm de agua sobre el fondo
    if (nivel < 0) nivel = 0;
    snprintf(v, sizeof(v), "%u", m.mm);          HASensor_publish_string(&mqtt, T_DIST, v);
    snprintf(v, sizeof(v), "%.1f", nivel / 10);  HASensor_publish_string(&mqtt, T_NIVEL, v);
    if (util >= 10) {
      float pct = nivel / util * 100.0f;
      if (pct > 100) pct = 100;
      float litros = pct / 100.0f * gCal.capacidadL;
      snprintf(v, sizeof(v), "%.1f", pct);       HASensor_publish_string(&mqtt, T_PCT, v);
      snprintf(v, sizeof(v), "%.0f", litros);    HASensor_publish_string(&mqtt, T_LITROS, v);
      sendLog("Nivel %.1f cm, %.1f %%, %.0f L", nivel / 10, pct, litros);
    } else {
      estado = "Calibración incorrecta (lleno >= vacío)";
    }
  }
  snprintf(v, sizeof(v), "%d", (int)WiFi.RSSI());  HASensor_publish_string(&mqtt, T_RSSI, v);
  HASensor_publish_string(&mqtt, T_IP, WiFi.localIP().toString());
  HASensor_publish_string(&mqtt, T_ESTADO, estado);
  mqtt.loop();
}

// ---------------------------------------------------------------------------
// OTA (solo despierto)
// ---------------------------------------------------------------------------
void setupOTA() {
  if (gOtaIniciado) return;
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() { sendLog("OTA Start"); });
  ArduinoOTA.onEnd([]() { sendLog("OTA End"); });
  ArduinoOTA.onError([](ota_error_t error) { sendLog("OTA Error[%u]", error); });
  ArduinoOTA.begin();
  gOtaIniciado = true;
  sendLog("OTA listo en %s", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  rtcCiclos++;
  bool arranqueFrio = (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER);

  antena_init();
  cal_cargar();
  wifi_iniciar();                       // conecta mientras se mide
  medir();

  if (!wifi_esperar(WIFI_TIMEOUT_MS)) {
    rtcCanal = 0;
    Serial.println("Sin WiFi");
    if (USAR_DEEP_SLEEP) dormir(INTERVALO_S);
    ESP.restart();
  }
  sendLog("%s: ciclo %lu, %s, WiFi en %lu ms, IP %s, RSSI %d%s", DEPOSITO_NOMBRE, (unsigned long)rtcCiclos,
          arranqueFrio ? "arranque" : "despertar", (unsigned long)millis(),
          WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(),
          MODO_DEPURACION ? " [MODO DEPURACION: no duerme, datos cada 1 s]" : "");
  sendLog("Calibracion: vacio %.0f mm, lleno %.0f mm = %.0f L", gCal.vacioMm, gCal.llenoMm, gCal.capacidadL);

  if (!mqtt_conectar(3)) {
    if (USAR_DEEP_SLEEP) dormir(INTERVALO_S);
    ESP.restart();
  }
  leer_retenidos(400);                  // mantenimiento y ordenes de calibracion
  if (arranqueFrio || rtcCiclos % DISCOVERY_CADA_CICLOS == 0) discovery();
  publicar();

  if (!modo_despierto()) dormir(INTERVALO_S);

  Switch_publish_state(&mqtt, T_MANT, gMantenimiento);
  setupOTA();
  gUltimaMedida = millis();
}

void loop() {
  ArduinoOTA.handle();

  if (!mqtt.connected()) {
    static uint32_t ultimoIntento = 0;
    if (millis() - ultimoIntento > 5000) {
      ultimoIntento = millis();
      if (mqtt_conectar(1)) sendLog("MQTT reconectado");
    }
  }
  mqtt.loop();

  // Mantenimiento puesto a OFF desde HA -> volver al ciclo de sueño
  if (!modo_despierto()) dormir(INTERVALO_S);

  uint32_t periodo = MODO_DEPURACION ? INTERVALO_DEPURACION_MS
                   : (gMantenimiento ? INTERVALO_MANT_S : INTERVALO_S) * 1000UL;
  if (millis() - gUltimaMedida >= periodo) {
    gUltimaMedida = millis();
    medir();
    if (mqtt.connected()) publicar();
  }
  delay(5);
}
