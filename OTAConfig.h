// OTAUpdate.h
#ifndef OTAUPDATE_H
#define OTAUPDATE_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

// URLs del repositorio de GitHub
const char* githubVersionURL = "https://raw.githubusercontent.com/pablosvb/ETD-PRO/refs/heads/main/firmaware/version.txt";
const char* firmwareBinURL = "https://raw.githubusercontent.com/pablosvb/ETD-PRO/refs/heads/main/firmaware/firmware.bin";
//token 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
// Versión actual del firmware
const String currentVersion = "0.1.0";

void updateFirmware() {
  WiFiClientSecure client;
  client.setInsecure(); // Configura el certificado raíz

  HTTPClient http;
  Serial.println("Descargando el nuevo firmware...");

  if (http.begin(client, firmwareBinURL)) { // Inicia la conexión HTTP usando WiFiClientSecure
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      bool isValidContentType = http.header("Content-Type") == "application/octet-stream";

      if (contentLength > 0 && isValidContentType) {
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
          Serial.println("Comenzando la actualización...");
          size_t written = Update.writeStream(http.getStream());

          if (written == contentLength) {
            Serial.println("Actualización completada. Reiniciando...");
            if (Update.end()) {
              Serial.println("Actualización exitosa");
              ESP.restart();
            } else {
              Serial.println("Error al finalizar la actualización: " + String(Update.getError()));
            }
          } else {
            Serial.println("Error al escribir el firmware. Bytes escritos: " + String(written));
          }
        } else {
          Serial.println("No hay suficiente espacio para la actualización.");
        }
      } else {
        Serial.println("Contenido no válido.");
      }
    } else {
      Serial.println("Error al descargar el firmware. Código HTTP: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("No se pudo conectar al servidor.");
  }
}

void checkForUpdates() {
  WiFiClientSecure client;
  client.setInsecure(); // Configura el certificado raíz

  HTTPClient http;
  Serial.println("Comprobando actualizaciones...");

  if (http.begin(client, githubVersionURL)) { // Inicia la conexión HTTP usando WiFiClientSecure
    int httpCode = http.GET();
    Serial.println("HttpCode Version: " + httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String newVersion = http.getString();
      Serial.println("new version: " + newVersion);
      newVersion.trim();

      Serial.println("Versión disponible en GitHub: " + newVersion);
      Serial.println("Versión actual del dispositivo: " + currentVersion);

      if (newVersion != currentVersion) {
        Serial.println("Nueva versión disponible. Iniciando actualización...");
        http.end();

        // Llamar a la función para actualizar el firmware
        updateFirmware();
      } else {
        Serial.println("La versión actual es la más reciente.");
      }
    } else {
      Serial.println("Error al comprobar actualizaciones. Código HTTP: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("No se pudo conectar al servidor para comprobar actualizaciones.");
  }
}



#endif
