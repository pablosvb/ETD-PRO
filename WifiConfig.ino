#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <WiFi.h>

const char* ssid = "ETD-Pro";
const char* password = "12345678";
unsigned long startMillis;
unsigned long timeoutMillis = 5000;

void connectToWiFi() {
  WiFi.begin(ssid, password);
  startMillis = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMillis) < timeoutMillis) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado a la red WiFi!");
  } else {
    Serial.println("No se pudo conectar a la red WiFi, iniciando en modo offline.");
  }
}

#endif
