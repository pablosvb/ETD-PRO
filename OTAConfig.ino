#ifndef OTACONFIG_H
#define OTACONFIG_H

#include <HTTPClient.h>
#include <Update.h>

const char* firmwareUrl = "https://raw.githubusercontent.com/tu_usuario/tu_repositorio/main/firmware.bin";
const char* versionUrl = "https://raw.githubusercontent.com/tu_usuario/tu_repositorio/main/version.txt";
const char* currentVersion = "1.0.0"; // Versión actual de tu firmware

void checkForUpdate() {
  HTTPClient http;
  http.begin(versionUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String newVersion = http.getString();
    newVersion.trim();

    if (newVersion.equals(currentVersion)) {
      Serial.println("El firmware está actualizado.");
    } else {
      Serial.println("Nueva versión disponible: " + newVersion);
      updateFirmware();
    }
  } else {
    Serial.println("No se pudo verificar la versión.");
  }
  http.end();
}

void updateFirmware() {
  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    WiFiClient* client = http.getStreamPtr();
    size_t size = http.getSize();
    
    if (Update.begin(size)) {
      Update.writeStream(*client);
      if (Update.end(true)) {
        Serial.println("Actualización exitosa, reiniciando...");
        ESP.restart();
      } else {
        Serial.println("Error en la actualización.");
      }
    } else {
      Serial.println("No se pudo iniciar la actualización.");
    }
  } else {
    Serial.println("No se encontró firmware en la URL.");
  }
  http.end();
}

#endif
