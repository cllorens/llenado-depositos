#include "a02yyuw.h"
#include <driver/gpio.h>

static HardwareSerial &sSer = Serial1;
static bool sEncendido = false;
static bool sUartActiva = false;

const char* a02_estado_str(A02Estado e) {
  switch (e) {
    case A02_OK:             return "OK";
    case A02_SIN_RESPUESTA:  return "Sin respuesta";
    case A02_FUERA_DE_RANGO: return "Fuera de rango";
    case A02_POCAS_MUESTRAS: return "Pocas lecturas";
  }
  return "?";
}

static void alimentar(int pinVcc, bool on) {
  gpio_hold_dis((gpio_num_t)pinVcc);           // por si venia retenido del sueño
  pinMode(pinVcc, OUTPUT);
  gpio_set_drive_capability((gpio_num_t)pinVcc, GPIO_DRIVE_CAP_3);
  digitalWrite(pinVcc, on ? HIGH : LOW);
  sEncendido = on;
}

void a02_apagar(int pinVcc, int pinRx) {
  if (sUartActiva) { sSer.end(); sUartActiva = false; }
  pinMode(pinRx, INPUT);
  alimentar(pinVcc, false);
  gpio_hold_en((gpio_num_t)pinVcc);            // se mantiene a 0 durante el deep sleep
}

A02Medida a02_medir(int pinVcc, int pinRx, uint32_t arranqueMs, uint8_t muestras,
                    uint32_t timeoutMs, bool apagar) {
  A02Medida m = {};
  uint16_t buf[32];
  if (muestras > 32) muestras = 32;

  if (!sEncendido) {
    pinMode(pinRx, INPUT);
    alimentar(pinVcc, true);
    delay(arranqueMs);
  }
  if (!sUartActiva) {
    sSer.begin(9600, SERIAL_8N1, pinRx, -1);
    sUartActiva = true;
  }
  while (sSer.available()) sSer.read();        // descarta lo antiguo: solo lecturas nuevas

  uint8_t trama[4];
  uint8_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs && m.validas < muestras) {
    if (!sSer.available()) { delay(2); continue; }
    uint8_t b = sSer.read();
    if (n == 0 && b != 0xFF) continue;         // esperar cabecera
    trama[n++] = b;
    if (n < 4) continue;
    n = 0;
    uint8_t suma = (uint8_t)(trama[0] + trama[1] + trama[2]);
    if (suma != trama[3]) {
      m.errores++;
      // el byte bajo puede ser 0xFF: resincronizar en los bytes siguientes
      for (int i = 1; i < 4; i++) {
        if (trama[i] == 0xFF) {
          int k = 0;
          for (int j = i; j < 4; j++) trama[k++] = trama[j];
          n = k;
          break;
        }
      }
      continue;
    }
    m.tramas++;
    uint16_t mm = ((uint16_t)trama[1] << 8) | trama[2];
    if (mm >= A02_MIN_MM && mm <= A02_MAX_MM) buf[m.validas++] = mm;
  }

  if (apagar) a02_apagar(pinVcc, pinRx);

  if (m.tramas == 0)  { m.estado = A02_SIN_RESPUESTA;  return m; }
  if (m.validas == 0) { m.estado = A02_FUERA_DE_RANGO; return m; }

  // Mediana (ordenacion por insercion, pocas muestras)
  for (int i = 1; i < m.validas; i++) {
    uint16_t v = buf[i]; int j = i - 1;
    while (j >= 0 && buf[j] > v) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  m.minimo = buf[0];
  m.maximo = buf[m.validas - 1];
  m.mm = buf[m.validas / 2];
  m.estado = (m.validas >= 3) ? A02_OK : A02_POCAS_MUESTRAS;
  return m;
}
