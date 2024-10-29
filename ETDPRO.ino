// main.ino
#include <Arduino.h>
#include <WiFi.h>
#include "WifiConfig.h"
#include "OTAConfig.h"
#include <FastLED.h>  // Incluimos la librería FastLED
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h" // Añadido para acceder a las funciones LEDC
#include "hal/ledc_types.h"

#define LED_PIN_IND     21       // Pin donde están conectados los leds_IND
#define NUM_leds_IND    1        // Número de leds_IND conectados
#define LED_TYPE_IND    WS2812B  // Tipo de leds_IND que estás usando
#define COLOR_ORDER_IND RGB      // Orden de colores de los leds_IND

#define LED_PIN_LINTERNA    9           // Pin donde están conectados los leds_IND
#define NUM_leds_LINTERNA    2           // Número de leds_IND conectados
#define LED_TYPE_LINTERNA    WS2812B     // Tipo de leds_IND que estás usando
#define COLOR_ORDER_LINTERNA GRB         // Orden de colores de los leds_IND

#define BRIGHTNESS  255         // Brillo de los leds_IND (0-255)
CRGB leds_IND[NUM_leds_IND];
CRGB leds_LINTERNA[NUM_leds_LINTERNA];

#define BUTTON_POWER_PIN 4  // Pin del botón de encendido/apagado
#define BUTTON2_PIN      5  // Pin del segundo botón
#define BUTTON3_PIN      6  // Pin del tercer botón

// Definiciones para el LED IR y el inductor
#define IR_LED_PIN      8   // Pin donde está conectado el LED IR y el inductor
#define INDUCTOR_PIN    7
#define NUM_FREQUENCIES 15// Número de frecuencias disponibles
//Definiciones para PWM
#define PWM_RESOLUTION       LEDC_TIMER_8_BIT    // Resolución de 8 bits
// Definiciones para los canales PWM
#define IR_PWM_CHANNEL       LEDC_CHANNEL_0  // Canal PWM para el LED IR
#define INDUCTOR_PWM_CHANNEL LEDC_CHANNEL_1  // Canal PWM para el inductor

#define PWM_RESOLUTION       LEDC_TIMER_8_BIT    // Resolución de 8 bits
#define NUM_FREQUENCIES      15    // Número de frecuencias disponibles

// Variables para el manejo del botón 2
unsigned long lastButton2Press = 0;
volatile int currentColorIndex = 0;

// Variables para el manejo del botón 3
unsigned long lastButton3Press = 0;
int currentFrequencyIndex = 14;

unsigned long lastEnterSleepTime = 0;

int lastNumber = 0;
const int numColors = 10;

// Estructura para almacenar las frecuencias y su tipo
typedef struct {
    float frequency;      // Frecuencia en Hz
    bool useSoftware;     // true si se genera por software
} FrequencyEntry;

FrequencyEntry frequencyList[NUM_FREQUENCIES] = {
    {0.57, true},
    {0.55, true},
    {0.53, true},
    {0.5,  true},
    {0.47, true},
    {0.45, true},
    {0.39, true},
    {0.34, true},
    {396,  false},
    {417,  false},
    {522,  false},
    {639,  false},
    {852,  false},
    {963,  false},
    {0,    true} // Puedes usar 0 para apagar la señal
};

// Variables para la generación por software
unsigned long lastToggleTimeIR = 0;
unsigned long lastToggleTimeInductor = 0;
bool pinStateIR = false;
bool pinStateInductor = false;



// Variables para manejar las interrupciones
volatile bool enterSleep = false;
volatile bool interruptFlag = false;
volatile bool button3Pressed = false;


void IRAM_ATTR handlePowerButtonInterrupt() {
  enterSleep = true;
  Serial.println("Dentro");
}

void IRAM_ATTR handleChangeColor() {
  interruptFlag = true;
}

void IRAM_ATTR handleChangeFrecuency() {
  button3Pressed = true;
}


void setup() {
  Serial.begin(115200);
  
  // Configurar pines de botones como entradas con pull-up interno
  pinMode(BUTTON_POWER_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  // Configurar interrupción para el botón de encendido/apagado
  attachInterrupt(digitalPinToInterrupt(BUTTON_POWER_PIN), handlePowerButtonInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), handleChangeColor, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON3_PIN), handleChangeFrecuency, RISING);


  // Crear la tarea de generación de señal en el Núcleo 0
    xTaskCreatePinnedToCore(
        signalGeneratorTask,   // Función de la tarea
        "SignalGeneratorTask", // Nombre de la tarea
        2048,                  // Tamaño de la pila en bytes
        NULL,                  // Parámetro para la tarea
        1,                     // Prioridad de la tarea
        NULL,                  // Handle de la tarea
        0                      // Núcleo al que se fija la tarea (0 o 1)
    );

   

  //seteamos la frecuencia a 0 de inicio eligiendo el valor 0 del array
  currentFrequencyIndex = 14;
  
    
  // Inicializar los leds_IND
  FastLED.addLeds<LED_TYPE_IND, LED_PIN_IND, COLOR_ORDER_IND>(leds_IND, NUM_leds_IND);
  FastLED.addLeds<LED_TYPE_LINTERNA,LED_PIN_LINTERNA, COLOR_ORDER_LINTERNA>(leds_LINTERNA, NUM_leds_LINTERNA);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show(); // Apaga los leds_IND inicialmente

  // Establecer color inicial
  currentColorIndex = 10;
  setColorByIndexLinterna(currentColorIndex);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  // Verificar si el ESP32 se despertó del Deep Sleep
   switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Despertado por GPIO (EXT0)");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Despertado por GPIO (EXT1)");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Despertado por Temporizador");
      break;
    default:
      Serial.println("Encendido normal");
      break;
  }

  // Intentar conectar al Wi-Fi
  Serial.println("Conectando al Wi-Fi...");
  //esp_wifi_start();
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  // Intentar conectar durante 15 segundos
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
    Serial.print(".");
    // Hacer parpadear los leds_IND en rojo
    blinkRGB(255,0,0);
    // Verificar si se ha solicitado entrar en modo de bajo consumo
    if (enterSleep) {
      Serial.println("\nBotón de encendido/apagado presionado. Entrando en modo de bajo consumo.");
      enterDeepSleep();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado al Wi-Fi.");
    FastLED.clear();   // Apagar los leds_IND
    fill_solid(leds_IND, NUM_leds_IND, CRGB::Green);
    FastLED[0].showLeds();
    delay(1000);
    FastLED.clear();   // Apagar los leds_IND
    FastLED[0].showLeds();

    checkForUpdates();
  } else {
    Serial.println("\nNo se pudo conectar al Wi-Fi. Iniciando en modo offline.");
    //esp_wifi_stop();
    FastLED.clear();   // Apagar los leds_IND
    FastLED[0].showLeds();
    // Continúa con el programa en modo offline
  }

}
//////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// LOOP /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
void loop() {
  static unsigned long lastButtonPressTime = 0;
  const unsigned long debounceDelay = 200; 
  unsigned long currentTime = millis();
  
  // Verificar si se ha solicitado entrar en modo de bajo consumo
  if (enterSleep) {
    if (currentTime - lastEnterSleepTime > debounceDelay) {
      lastEnterSleepTime = currentTime;
      Serial.println("Botón de encendido/apagado presionado. Entrando en modo de bajo consumo.");
      enterDeepSleep();
    }
    
    
  }
  
 if (interruptFlag) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
     // Incrementar el índice del color
    currentColorIndex = (currentColorIndex + 1) % numColors;
    Serial.println("Botón presionado sin rebote.");
    
     // Establecer el nuevo color utilizando switch-case
    setColorByIndexLinterna(currentColorIndex);
  }
  interruptFlag = false;
  delay(200);
  
}
   // Tiempo de antirrebote en milisegundos
  


  // Manejo del botón 3 para cambiar la frecuencia
  if (button3Pressed) {
    // Antirrebote para el botón 3
    unsigned long currentTime = millis();
     // Antirrebote para el botón 3
    if (currentTime - lastButton3Press > debounceDelay) {
        lastButton3Press = currentTime;

        // Incrementar el índice de la frecuencia
        currentFrequencyIndex = (currentFrequencyIndex + 1) % NUM_FREQUENCIES;

        // Actualizar la frecuencia
        updateFrequency();

        // Mostrar la frecuencia actual en el monitor serie
        FrequencyEntry currentFrequency = frequencyList[currentFrequencyIndex];
        Serial.print("Frecuencia cambiada a: ");
        Serial.print(currentFrequency.frequency);
        Serial.print(" Hz - Modo: ");
        Serial.println(currentFrequency.useSoftware ? "Software" : "PWM");
    }
    button3Pressed = false;
  }

   // Leer datos del monitor serie
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n'); // Leer hasta el final de línea
    inputString.trim(); // Eliminar espacios en blanco
    // Verificar si el usuario ingresó un comando para cambiar el ciclo de trabajo
    if (inputString.startsWith("duty ")) {
        // Extraer el valor después del comando "duty "
        String percentageString = inputString.substring(5);
        float percentage = percentageString.toFloat();

        // Establecer el ciclo de trabajo al porcentaje ingresado
        setDutyCyclePercentage(percentage);
    } 
    if(inputString.startsWith("RGB ")) {
        // Asumir que es un comando para cambiar el color
        String RGBString = inputString.substring(4);
        parseRGBInput(inputString);
    }
  }

  if(currentFrequencyIndex>7 && currentFrequencyIndex<14){
    setColorByIndexSignal(currentFrequencyIndex);
    }

  // Puedes agregar más código aquí si lo deseas

  delay(1); // Pequeño delay para evitar sobrecarga del loop
}


// Función para generar señal por software
void generateSoftwareSignal(float frequency, uint8_t pin, unsigned long &lastToggleTime, bool &pinState) {
    unsigned long interval_ms = (1.0 / frequency) * 500.0; // Intervalo para medio ciclo en ms

    unsigned long currentTime = millis();
    if (currentTime - lastToggleTime >= interval_ms) {
        pinState = !pinState;
        digitalWrite(pin, pinState);
        lastToggleTime = currentTime;
    }
}

// Función para actualizar la frecuencia
void updateFrequency() {
    setColorByIndexSignal(currentFrequencyIndex);
    delay(300);
    FrequencyEntry currentFrequency = frequencyList[currentFrequencyIndex];

    if (currentFrequency.useSoftware || currentFrequency.frequency == 0) {
        // Configurar los pines como salida para control manual
        pinMode(IR_LED_PIN, OUTPUT);
        digitalWrite(IR_LED_PIN, LOW);

        pinMode(INDUCTOR_PIN, OUTPUT);
        digitalWrite(INDUCTOR_PIN, LOW);

        // Detener el PWM en estos canales
        ledc_stop(LEDC_LOW_SPEED_MODE, IR_PWM_CHANNEL, 0);
        ledc_stop(LEDC_LOW_SPEED_MODE, INDUCTOR_PWM_CHANNEL, 0);
    } else {
        // Configurar el temporizador para el PWM
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .duty_resolution  = PWM_RESOLUTION,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = (uint32_t)currentFrequency.frequency,
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ledc_timer_config(&ledc_timer);

        // Configurar el canal PWM para el LED IR
        ledc_channel_config_t ledc_channel_ir = {
            .gpio_num       = IR_LED_PIN,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = IR_PWM_CHANNEL,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = (1 << (PWM_RESOLUTION - 1)), // 50% de ciclo de trabajo
            .hpoint         = 0
        };
        ledc_channel_config(&ledc_channel_ir);

        // Configurar el canal PWM para el inductor
        ledc_channel_config_t ledc_channel_inductor = {
            .gpio_num       = INDUCTOR_PIN,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = INDUCTOR_PWM_CHANNEL,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = (1 << (PWM_RESOLUTION - 1)), // 50% de ciclo de trabajo
            .hpoint         = 0
        };
        ledc_channel_config(&ledc_channel_inductor);

        // Iniciar el PWM
        ledc_set_duty(LEDC_LOW_SPEED_MODE, IR_PWM_CHANNEL, (1 << (PWM_RESOLUTION - 1)));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, IR_PWM_CHANNEL);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, INDUCTOR_PWM_CHANNEL, (1 << (PWM_RESOLUTION - 1)));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, INDUCTOR_PWM_CHANNEL);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
// Función para establecer el color de la linterna según el índice utilizando switch-case
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

void setColorByIndexLinterna(int index) {
  CRGB color;
  String colorName;

  switch (index) {
    case 0:
      color = CRGB(255, 40, 0);   // Naranja
      colorName = "Naranja";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 1:
      color = CRGB(0, 0, 255);     // Azul
      colorName = "Azul";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 2:
      color = CRGB(255, 200, 0);   // Amarillo
      colorName = "Amarillo";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 3:
      color = CRGB(255, 0, 0);     // Rojo
      colorName = "Rojo";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 4:
      color = CRGB(255, 0, 255); // Violeta
      colorName = "Violeta";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 5:
      color = CRGB(0, 255, 0);     // Verde
      colorName = "Verde";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 6:
      color = CRGB(255, 0, 20); // Rosa
      colorName = "Rosa";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 7:
      color = CRGB(255, 255, 255); // Blanco
      colorName = "Blanco";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 8:
      color = CRGB(200, 80, 5);   // Marrón
      colorName = "Marrón";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
    case 9:
      color = CRGB(0, 0, 0);       // Apagado
      colorName = "Apagado";
      delay(100);
      // Establecer el color en los leds_IND
      fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
      FastLED[1].showLeds();
      delay(100);
      break;
  }
  delay(100);
  fill_solid(leds_LINTERNA, NUM_leds_LINTERNA, color);
  FastLED[1].showLeds();
  delay(100);
  // Mostrar el nombre del color en el monitor serie
  Serial.print("Color linterna cambiado a: ");
  Serial.println(colorName);
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
// Función para establecer el color del indicador según el índex utilizando switch-case
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

void setColorByIndexSignal(int index) {
  CRGB color;
  String colorName;

  switch (index) {
    case 0:
      color = CRGB(145, 152, 166);   // Gris
      colorName = "Gris";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 1:
      color = CRGB(0, 255, 0);     // Verde
      colorName = "Verde";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 2:
      color = CRGB(0, 255, 255);   // Azul claro
      colorName = "Azul claro";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 3:
      color = CRGB(255, 40, 0);   // Naranja
      colorName = "Naranja";
      delay(100);
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 4:
      color = CRGB(255, 0, 0); // rojo
      colorName = "Rojo";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 5:
      color = CRGB(255, 200, 0);// Amarillo
      colorName = "Amarillo";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 6:
      color = CRGB(255, 0, 20); // Rosa
      colorName = "Rosa";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 7:
      color = CRGB(0, 0, 255); // Azul Oscuro
      colorName = "Azul oscuro";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
    case 8:
      blinkRGB(255, 0, 255);   // Violeta
      colorName = "Violeta bk";
      break;
    case 9:
      blinkRGB(0, 170, 255);// Azul
      colorName = "Azul bk";
      break;
    case 10:
      blinkRGB(0, 255, 0); // Verde
      colorName = "Verde bk";
      break;
    case 11:
      blinkRGB(255, 200, 0);// Amarillo
      colorName = "Amarillo bk";
      break;
    case 12:
      blinkRGB(255, 0, 20); // naranja
      colorName = "Naranja bk";
      break;
    case 13:
      blinkRGB(255, 0, 0); // Rojo
      colorName = "Rojo bk";
      break;
    case 14:
      color = CRGB(0, 0, 0);//OFF
      colorName = "off";
      delay(100);
      // Establecer el color en el indice
      fill_solid(leds_IND, NUM_leds_IND, color);
      FastLED[0].showLeds();
      delay(100);
      break;
  }

  delay(100);
  // Establecer el color en el indice
  fill_solid(leds_IND, NUM_leds_IND, color);
  FastLED[0].showLeds();
  delay(100);

  // Mostrar el nombre del color en el monitor serie
  Serial.print("Color cambiado a: ");
  Serial.println(colorName);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
// Función para parsear la entrada del usuario y establecer el color
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

void parseRGBInput(String input) {
  input.trim(); // Eliminar espacios en blanco al inicio y al final
  if (input.length() == 0) return; // Si la entrada está vacía, salir

  // Buscar los paréntesis y las comas
  int firstParen = input.indexOf('(');
  int lastParen = input.indexOf(')');
  int firstComma = input.indexOf(',');
  int secondComma = input.indexOf(',', firstComma + 1);

  // Verificar que la entrada tenga el formato correcto
  if (firstParen == -1 || lastParen == -1 || firstComma == -1 || secondComma == -1) {
    Serial.println("Formato incorrecto. Usa el formato (R,G,B)");
    return;
  }

  // Extraer los valores de R, G y B
  String rString = input.substring(firstParen + 1, firstComma);
  String gString = input.substring(firstComma + 1, secondComma);
  String bString = input.substring(secondComma + 1, lastParen);

  // Convertir las cadenas a enteros
  int r = rString.toInt();
  int g = gString.toInt();
  int b = bString.toInt();

  // Validar los valores (0 - 255)
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    Serial.println("Valores fuera de rango. Usa valores entre 0 y 255.");
    return;
  }

  // Establecer el color en los LEDs
  fill_solid(leds_IND, NUM_leds_IND, CRGB(r,g,b));

  // Mostrar el color establecido en el monitor serie
  Serial.print("Color establecido a: (");
  Serial.print(r);
  Serial.print(", ");
  Serial.print(g);
  Serial.print(", ");
  Serial.print(b);
  Serial.println(")");
}


// Función para hacer parpadear los LEDs en rojo
void blinkRGB(uint8_t r, uint8_t g, uint8_t b){
  static bool ledState = false;
  if (ledState) {
    // Apagar LEDs
    delay(100);
    // Establecer el color en el indice
    fill_solid(leds_IND, NUM_leds_IND, CRGB(r,g,b));
    FastLED[0].showLeds();
      
    delay(500);
  } else {
    // Apagar LEDs
    delay(100);
    // Establecer el color en el indice
    fill_solid(leds_IND, NUM_leds_IND, CRGB(r,g,b));
    FastLED[0].showLeds();
    delay(500);
  }
  FastLED[0].showLeds();
  ledState = !ledState;
}

// Función para establecer el ciclo de trabajo basado en un porcentaje
void setDutyCyclePercentage(float percentage) {
    // Limitar el porcentaje al rango válido (0% - 100%)
    if (percentage < 0.0) {
        percentage = 0.0;
    } else if (percentage > 100.0) {
        percentage = 100.0;
    }

    // Calcular el valor máximo de duty
    uint32_t dutyMax = (1 << PWM_RESOLUTION) - 1;

    // Calcular el valor de duty para el porcentaje dado
    uint32_t duty = (uint32_t)((dutyMax * percentage) / 100.0);

    // Actualizar el ciclo de trabajo del LED IR
    ledc_set_duty(LEDC_LOW_SPEED_MODE, IR_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, IR_PWM_CHANNEL);

    // Actualizar el ciclo de trabajo del inductor
    ledc_set_duty(LEDC_LOW_SPEED_MODE, INDUCTOR_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, INDUCTOR_PWM_CHANNEL);

    // Mostrar el ciclo de trabajo actual en el monitor serie
    Serial.print("Ciclo de trabajo ajustado a: ");
    Serial.print(percentage);
    Serial.println("%");
}

void signalGeneratorTask(void *parameter) {
    // Variables locales de la tarea
    unsigned long lastToggleTimeIR = 0;
    unsigned long lastToggleTimeInductor = 0;
    bool pinStateIR = false;
    bool pinStateInductor = false;

    while (true) {
        // Obtenemos la frecuencia actual
        FrequencyEntry currentFrequency = frequencyList[currentFrequencyIndex];

        if (currentFrequency.useSoftware && currentFrequency.frequency > 0) {
            // Generar señal por software para el LED IR
            generateSoftwareSignal(currentFrequency.frequency, IR_LED_PIN, lastToggleTimeIR, pinStateIR);

            // Generar señal por software para el inductor
            generateSoftwareSignal(currentFrequency.frequency, INDUCTOR_PIN, lastToggleTimeInductor, pinStateInductor);
        } else {
            // Si no se usa señal por software, ponemos los pines en bajo
            digitalWrite(IR_LED_PIN, LOW);
            digitalWrite(INDUCTOR_PIN, LOW);
            // Pequeño delay para evitar carga innecesaria
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // Pequeño delay para ceder tiempo a otras tareas
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// Función para entrar en modo de Deep Sleep
void enterDeepSleep() {
  FastLED.clear();   // Apagar los LEDs
  FastLED.show();

   // Esperar hasta que el botón sea liberado
  while (digitalRead(BUTTON_POWER_PIN) == LOW) {
    delay(10); // Pequeño delay para evitar sobrecarga
  }
  // Apagar Wi-Fi y Bluetooth para reducir consumo
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
  enterSleep = false;
}
