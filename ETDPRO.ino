#include <Arduino.h>
#include "WifiConfig.h"
#include "OTAConfig.h"

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    checkForUpdate();
  }
}

void loop() {
  // Tu código principal va aquí
}
