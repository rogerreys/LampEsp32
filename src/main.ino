#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <esp_sleep.h>

// ESP32
// #define LEDS_PIN GPIO_NUM_15
// #define BUTTON_PIN_LED GPIO_NUM_19 // Pin del botón
// #define BUTTON_PIN_RST GPIO_NUM_18
// #define BUTTON_PIN_PWR GPIO_NUM_4
// ESP32 C3
#define LEDS_PIN GPIO_NUM_1
#define BUTTON_PIN_LED GPIO_NUM_2 // Pin del botón
#define BUTTON_PIN_RST GPIO_NUM_3
#define BUTTON_PIN_PWR GPIO_NUM_0

#define DNS_NAME "lamp.local"
#define NUMPIXELS 50
#define TIME 100
#define DELAYVAL 500

WiFiManager wm;
DNSServer dnsServer;
WebServer server(80);
Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// Variables globales para el color
uint32_t colSkyBlue = pixels.Color(0, 200, 200);
uint32_t colGreen = pixels.Color(0, 255, 0);
uint32_t colPurple = pixels.Color(151, 1, 247);
uint32_t colYellow = pixels.Color(251, 188, 5);
uint32_t colOrange = pixels.Color(246, 83, 20);
// Variables de estado
int currentMode = 3;       // Modo actual del juego de luces
bool lastButtonState = HIGH; // Estado anterior del botón
bool lastButtonStatePwr = HIGH;
bool espState = true; // Estado del ESP32 (encendido o apagado)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // Tiempo para evitar rebotes
// Variables para efectos sin delay
unsigned long previousMillis = 0;
int stepCounter = 0;

/***********************************
 * AP WiFi Manager
 ************************************/
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entrando en modo de configuración AP");
  Serial.println(WiFi.softAPIP());                      // Muestra la dirección IP del AP
  Serial.println(myWiFiManager->getConfigPortalSSID()); // Muestra el nombre del AP
}

/***********************************
 * PAGINAS WEB WEBSERVER
 ************************************/
void handleRoot()
{
  File file = SPIFFS.open("/index.html", "r");
  if (!file)
  {
    server.send(500, "text/plain", "Error al abrir index.html");
    return;
  }

  server.streamFile(file, "text/html");
  file.close();
}
void handleImage()
{
  File file = SPIFFS.open("/chroma.png", "r");
  if (!file)
  {
    server.send(500, "text/plain", "Error al abrir chroma.png");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}
void handleScript()
{
  File file = SPIFFS.open("/script.min.js", "r");
  if (!file)
  {
    server.send(500, "text/plain", "Error al abrir script.min.js");
    return;
  }
  server.streamFile(file, "application/javascript");
  file.close();
}
void handleColor()
{
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b"))
  {
    String r = server.arg("r");
    String g = server.arg("g");
    String b = server.arg("b");
    String opt = server.arg("opt");

    // Convierte los valores a enteros
    int rValue = r.toInt();
    int gValue = g.toInt();
    int bValue = b.toInt();

    // Serial.printf("RGB_int: %s, %s, %s, %s\n", r, g, b, opt);

    if (opt == "3" || opt == "4" || opt == "5")
    {
      currentMode = 6;
    }
    // Almacena el color y activa colorWipe
    colSkyBlue = pixels.Color(rValue, gValue, bValue);
    // Crear un objeto JSON
    char jsonResponse[100]; // Buffer para almacenar la respuesta JSON
    snprintf(jsonResponse, sizeof(jsonResponse), "{\"r\": %d, \"g\": %d, \"b\": %d}", rValue, gValue, bValue);
    // Envía la respuesta JSON al cliente
    server.send(200, "application/json", jsonResponse);
  }
  else if (server.hasArg("option"))
  {
    String data = server.arg("option");
    String validOptions[] = {"1", "2", "3", "4", "5", "6"};

    if (std::find(std::begin(validOptions), std::end(validOptions), data) != std::end(validOptions))
    {
      server.send(200, "text/plain", "opcion recibido");
      if (data == "1")
      {
        // Serial.printf("opcion:%s - colorWipe - Los Leds se encienden consecutivamente desdel 0 hasta el último\n", data);
        currentMode = 1;
      }
      else if (data == "2")
      {
        // Serial.printf("opcion:%s - theaterChase - Los leds se encienden y apagan alternativamente uno si y uno no\n", data);
        currentMode = 2;
      }
      if (data == "3")
      {
        // Serial.printf("opcion:%s - rainbow - Todos los leds van pasando progresivamente por todos los colores\n", data);
        currentMode = 3;
      }
      if (data == "4")
      {
        // Serial.printf("opcion:%s - rainbowCycle - Ciclo de arcoiris progresivo en cada pixel\n", data);
        currentMode = 4;
      }
      if (data == "5")
      {
        // Serial.printf("opcion:%s - theaterChaseRainbow - Todos los leds se apagan y encienden rápidamente\n", data);
        currentMode = 5;
      }
      if (data == "6")
      {
        // Serial.printf("opcion:%s - full color\n", data);
        currentMode = 6;
      }
      server.send(200, "application/json", "Exito");
    }
  }
  else
  {
    server.send(400, "text/plain", "Parametros incorrectos");
  }
}

/***********************************
 * SETUP
 ************************************/
void setup()
{
  // debug_init();
  pinMode(BUTTON_PIN_LED, INPUT_PULLUP);
  pinMode(BUTTON_PIN_RST, INPUT_PULLUP);
  pinMode(BUTTON_PIN_PWR, INPUT_PULLUP);

  Serial.begin(115200);
  // Configuracion Pixel
  pixels.begin();
  pixels.show();

  // Detectar si el ESP32 se despertó de deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("ESP32 despertó por el botón");
  }
  else
  {
    Serial.println("ESP32 iniciando normalmente");
    // Indicación visual de conexión exitosa
    indicateColor(colPurple);
    fullColor(colPurple);
  }
  // Configurar el GPIO para despertar cuando pase de HIGH -> LOW
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN_PWR, ESP_GPIO_WAKEUP_GPIO_LOW); // ESP32C3
  // esp_sleep_enable_ext0_wakeup(BUTTON_PIN_PWR, 0); // ESP32

  if (!SPIFFS.begin(true))
  {
    Serial.println("Error al montar SPIFFS");
    return;
  }
  Serial.println("SPIFFS montado correctamente");

  // Configuracion punto de acceso a "LamparaIoT"
  wm.setConfigPortalTimeout(20);       // Tiempo de espera en segundos
  wm.setAPCallback(configModeCallback); // Callback cuando se inicia el AP
  wm.setCustomHeadElement("<title>Configuración LamparaIoT</title>");

  // Intenta conectarse a una red guardada, si falla, inicia el portal de configuración
  if (!wm.autoConnect("LamparaIoT"))
  {
    Serial.println("No se pudo conectar, reiniciando...");
    delay(100);
    // ESP.restart();
  }
  if (WiFi.localIP())
  {
    // Si llega aquí, está conectado a la red WiFi
    Serial.printf("Conectado a la red WiFi\nDirección IP:%s\n", WiFi.localIP().toString().c_str());
    // Indicación visual de conexión exitosa
    indicateColor(colGreen);

    server.on("/", handleRoot);
    server.on("/chroma.png", handleImage);
    server.on("/script.min.js", handleScript);
    server.on("/api", handleColor);

    server.begin();

    // Inicia el servidor DNS
    if (dnsServer.start(53, DNS_NAME, WiFi.localIP()))
    {
      Serial.printf("Servidor DNS iniciado correctamente: %s -> %s\n", WiFi.localIP().toString().c_str(), DNS_NAME);
    }
    else
    {
      Serial.println("Error al iniciar el servidor DNS");
    }
  }
  else
  {
    Serial.println("Sin conexion a la red");
    indicateColor(colYellow);
  }
}

/***********************************
 * LOOP
 ************************************/
void loop()
{
  if (WiFi.localIP())
  {
    // Procesa las solicitudes DNS
    dnsServer.processNextRequest();

    // Maneja las solicitudes HTTP
    server.handleClient();
  }
  // Detecta si se presionó el botón
  handleButtonPress();

  // Actualiza efectos sin bloquear el loop
  unsigned long currentMillis = millis();

  switch (currentMode)
  {
  case 1:
    colorWipe(colSkyBlue, TIME, currentMillis);
    break;
  case 2:
    theaterChase(colSkyBlue, TIME, currentMillis);
    break;
  case 3:
    rainbow(TIME, currentMillis);
    break;
  case 4:
    rainbowCycle(TIME, currentMillis);
    break;
  case 5:
    theaterChaseRainbow(TIME, currentMillis);
    break;
  case 6:
    fullColor(colSkyBlue);
    break;
  }
}

/***********************************
 * Btn
 ************************************/
// Función para detectar el botón con debounce
void handleButtonPress()
{
  bool buttonState = digitalRead(BUTTON_PIN_LED);
  bool buttonStatePwr = digitalRead(BUTTON_PIN_PWR);

  if (buttonState == LOW && lastButtonState == HIGH)
  {
    if (millis() - lastDebounceTime > debounceDelay)
    {
      currentMode = (currentMode + 1) % 5; // Cambia de modo (0-4)
      // Serial.printf("BTN modo: %d\n", currentMode);
      lastDebounceTime = millis();
      stepCounter = 0; // Reiniciar pasos para nuevos efectos
    }
  }
  if (digitalRead(BUTTON_PIN_RST) == LOW)
  {                // Si se presiona el botón
    delay(200);    // Pequeña espera para evitar rebotes
    ESP.restart(); // Reinicia el ESP32
  }
  if (buttonStatePwr == LOW && lastButtonStatePwr == HIGH)
  {
    if (digitalRead(BUTTON_PIN_PWR) == LOW)
    {
      Serial.println("Apagando ESP32...");
      delay(500); // Pequeña espera
      indicateColor(colOrange);
      esp_deep_sleep_start(); // Entrar en modo de bajo consumo
    }
  }
  lastButtonState = buttonState;
  lastButtonStatePwr = buttonStatePwr;
}

/***********************************
 * EFECTROS LUCES
 ************************************/
// Conectado
void indicateColor(uint32_t c)
{
  // Secuencia de encendido y apagado dos veces
  for (int i = 0; i < 2; i++)
  {
    fullColor(c);                     // Enciende los LEDs en verde
    delay(500);                       // Espera 500 ms
    fullColor(pixels.Color(0, 0, 0)); // Apaga los LEDs
    delay(500);                       // Espera 500 ms
  }
}
// Full color
void fullColor(uint32_t c)
{
  pixels.fill(c);
  pixels.show();
}
// Efecto ColorWipe sin delay
void colorWipe(uint32_t c, uint8_t wait, unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    if (stepCounter < pixels.numPixels())
    {
      pixels.setPixelColor(stepCounter, c);
      pixels.show();
      stepCounter++;
    }
    else
    {
      pixels.clear();
      stepCounter = 0;
    }
  }
}
// Efecto Rainbow sin delay
void rainbow(uint8_t wait, unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    for (int i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel((i + stepCounter) & 255));
    }
    pixels.show();
    stepCounter++;
  }
}
// Efecto RainbowCycle sin delay
void rainbowCycle(uint8_t wait, unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    for (int i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + stepCounter) & 255));
    }
    pixels.show();
    stepCounter++;
  }
}
// Efecto TheaterChase sin delay
void theaterChase(uint32_t c, uint8_t wait, unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    for (int q = 0; q < 3; q++)
    {
      handleButtonPress();
      for (int i = 0; i < pixels.numPixels(); i += 3)
      {
        pixels.setPixelColor(i + q, c);
      }
      pixels.show();
      delay(wait);
      for (int i = 0; i < pixels.numPixels(); i += 3)
      {
        pixels.setPixelColor(i + q, 0);
      }
      pixels.show();
    }
    stepCounter++;
  }
}
// Efecto TheaterChaseRainbow sin delay
void theaterChaseRainbow(uint8_t wait, unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    for (int q = 0; q < 3; q++)
    {
      handleButtonPress();
      for (int i = 0; i < pixels.numPixels(); i += 3)
      {
        pixels.setPixelColor(i + q, Wheel((i + stepCounter) % 255));
      }
      pixels.show();
      delay(wait);
      for (int i = 0; i < pixels.numPixels(); i += 3)
      {
        pixels.setPixelColor(i + q, 0);
      }
    }
    stepCounter++;
  }
}
// Función Wheel para colores arcoíris
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
