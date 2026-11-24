#pragma once
#include <Arduino.h>

// DFRobot A02YYUW (SEN0311): ultrasonico estanco, UART 9600 8N1, alcance 30..4500 mm.
// Trama de 4 bytes: 0xFF, distancia alta, distancia baja, suma = (0xFF + H + L) & 0xFF.
// Con su pin RX alto o sin conectar envia el valor filtrado (~cada 100-300 ms).

#define A02_MIN_MM 30
#define A02_MAX_MM 4500

enum A02Estado {
  A02_OK = 0,
  A02_SIN_RESPUESTA,    // no llega ninguna trama valida
  A02_FUERA_DE_RANGO,   // llegan tramas pero con distancias fuera de 30..4500 mm
  A02_POCAS_MUESTRAS    // menos de 3 lecturas validas
};

struct A02Medida {
  A02Estado estado;
  uint16_t  mm;          // mediana de las lecturas validas
  uint16_t  minimo, maximo;
  uint8_t   validas;     // tramas con distancia en rango
  uint8_t   tramas;      // tramas con checksum correcto
  uint8_t   errores;     // tramas con checksum incorrecto
};

// Enciende el sensor (si no lo estaba), lee por pinRx (TX del sensor) y lo apaga si apagar = true.
// Si se deja encendido, la siguiente llamada no espera el arranque.
A02Medida a02_medir(int pinVcc, int pinRx, uint32_t arranqueMs, uint8_t muestras,
                    uint32_t timeoutMs, bool apagar = true);

// Deja la alimentacion a 0 (retenida durante el deep sleep) y el pin de datos sin tirar de nada.
void a02_apagar(int pinVcc, int pinRx);

const char* a02_estado_str(A02Estado e);
