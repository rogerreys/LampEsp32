#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include <Preferences.h>

// ESP32 C3
#define LEDS_PIN GPIO_NUM_5
#define BTN_PIN_LED_PWR GPIO_NUM_1
#define BTN_PIN_SET GPIO_NUM_2

#define DNS_NAME "lamp.local"
#define AP_SSID "LamparaIoT"
#define AP_PASSWORD "12345678"
#define NUMPIXELS 50
#define TIME 100
#define DELAYVAL 500
#define OTA_HOSTNAME "ota.lamp.local"
#define OTA_PASSWORD "369741258"

DNSServer dnsServer;
WebServer server(80);
Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);
Preferences preferences;

// Variables globales para el color
uint32_t colSkyBlue = pixels.Color(0, 200, 200);
uint32_t colGreen = pixels.Color(0, 255, 0);
uint32_t colPurple = pixels.Color(151, 1, 247);
uint32_t colYellow = pixels.Color(251, 188, 5);
uint32_t colOrange = pixels.Color(246, 83, 20);
uint32_t colBlack = pixels.Color(0, 0, 0);
// Variables de estado
int currentMode = 3;            // Modo actual del juego de luces
bool lastbtnStateLedPwr = HIGH; // Estado anterior del botón
bool lastbtnStatePinSet = HIGH;
bool espState = true; // Estado del ESP32 (encendido o apagado)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // Tiempo para evitar rebotes
// Variables para efectos sin delay
unsigned long previousMillis = 0;
int stepCounter = 0;

// Pulsador
int estadoPulsador = HIGH;
int estadoAnteriorPulsador = HIGH;
unsigned long tiempoInicioPresionado = 0;
unsigned long tiempoPresionado = 0;
unsigned long tiempoTotal = 0;
bool pulsadorPresionado = false;
const unsigned long tiempoClicLargo = 672;
// Log Web
String logBuffer = ""; // Almacenar logs recientes

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
void handleNotFound()
{
  // Redirigir todas las solicitudes no encontradas a la página principal
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
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
        Serial.printf("opcion:%s - colorWipe - Los Leds se encienden consecutivamente desdel 0 hasta el último\n", data);
        currentMode = 1;
      }
      else if (data == "2")
      {
        Serial.printf("opcion:%s - theaterChase - Los leds se encienden y apagan alternativamente uno si y uno no\n", data);
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
void handleLog()
{
  String html = "<html><body><h1>ESP32 Logs</h1>";
  html += "<div id='logs'>" + logBuffer + "</div>";
  html += "<script>setInterval(function() { location.reload(); }, 5000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/***********************************
 * LOG WEB
 ************************************/
void addLog(String message)
{
  logBuffer += message + "<br>";
  // Limitar tamaño del buffer
  if (logBuffer.length() > 5000)
  {
    logBuffer = logBuffer.substring(logBuffer.length() - 5000);
  }
  Serial.println(message); // También enviar a serial
}
/***********************************
 * SETUP
 ************************************/
void setup()
{
  // debug_init();
  pinMode(BTN_PIN_LED_PWR, INPUT_PULLUP);
  pinMode(BTN_PIN_SET, INPUT_PULLUP);

  Serial.begin(115200);

  // IP por defecto o IP estática si lo prefieres
  WiFi.config(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

  // Configuración OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setPort(3232); // Puerto por defecto de OTA

  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // Serial.println("Iniciando actualización OTA " + type);
    addLog("Iniciando actualización OTA " + type);
    pixels.fill(colYellow); // Indicador visual de actualización
    pixels.show(); });

  ArduinoOTA.onEnd([]()
                   {
    // Serial.println("\nActualización OTA completada");
    addLog("Actualización OTA completada");
    pixels.fill(colGreen); // Indicador visual de éxito
    pixels.show();
    delay(1000); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    // Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
    String progressStr = "Progreso: " + String((progress / (total / 100))) + "%";
    addLog(progressStr);
    // Efecto visual de progreso
    int ledCount = (progress * NUMPIXELS) / total;
    pixels.clear();
    for(int i = 0; i < ledCount; i++) {
      pixels.setPixelColor(i, colYellow);
    }
    pixels.show(); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    String errorMsg = "Error[" + String(error) + "]: ";
    if (error == OTA_AUTH_ERROR) errorMsg += "Error de autenticación";
    else if (error == OTA_BEGIN_ERROR) errorMsg += "Error al iniciar";
    else if (error == OTA_CONNECT_ERROR) errorMsg += "Error de conexión";
    else if (error == OTA_RECEIVE_ERROR) errorMsg += "Error de recepción";
    else if (error == OTA_END_ERROR) errorMsg += "Error al finalizar";
    // Serial.println(errorMsg);
    addLog(errorMsg);
    pixels.fill(colOrange); // Indicador visual de error
    pixels.show(); });

  ArduinoOTA.begin();
  // Serial.println("OTA listo");
  // Serial.print("IP del AP: ");
  // Serial.println(WiFi.softAPIP()); // Debería mostrar 192.168.4.1
  addLog("OTA listo");
  addLog("IP del AP: " + String(WiFi.softAPIP().toString().c_str()));

  // Configuracion Pixel
  pixels.begin();
  pixels.show();
  // Iniciar la memoria no volátil
  preferences.begin("storage", false);

  // Detectar si el ESP32 se despertó de deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
  {
    // Serial.println("ESP32 despertó por el botón");
    addLog("ESP32 despertó por el botón");
  }
  else
  {
    // Serial.println("ESP32 iniciando normalmente");
    addLog("ESP32 iniciando normalmente");
    // Indicación visual de conexión exitosa
    indicateColor(colPurple);
    fullColor(colPurple);
  }
  // Configurar el GPIO para despertar cuando pase de HIGH -> LOW
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_PIN_LED_PWR, ESP_GPIO_WAKEUP_GPIO_LOW); // ESP32C3

  if (!SPIFFS.begin(true))
  {
    Serial.println("Error al montar SPIFFS");
    return;
  }
  Serial.println("SPIFFS montado correctamente");
  Serial.printf("stateInitSet:'%s'\n", preferences.getBool("stateInitSet") ? "true" : "false");

  // Configurar como punto de acceso
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  // Serial.printf("Punto de acceso iniciado. SSID: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  addLog("Punto de acceso iniciado. SSID: " + String(AP_SSID) + ", IP: " + String(WiFi.softAPIP().toString().c_str()));

  // Indicación visual de conexión exitosa
  indicateColor(colGreen);

  server.on("/", handleRoot);
  server.on("/chroma.png", handleImage);
  server.on("/script.min.js", handleScript);
  server.on("/api", handleColor);
  server.on("/log", handleLog);
  server.onNotFound(handleNotFound);

  server.begin();
  addLog("Servidor web iniciado en http://" + String(DNS_NAME));

  // Inicia el servidor DNS
  if (dnsServer.start(53, "*", WiFi.softAPIP()))
  {
    // Serial.printf("Servidor DNS iniciado correctamente: %s -> %s\n", WiFi.softAPIP().toString().c_str(), DNS_NAME);
    addLog("Servidor DNS iniciado correctamente: " + String(WiFi.softAPIP().toString().c_str()) + " -> " + DNS_NAME);
  }
  else
  {
    // Serial.println("Error al iniciar el servidor DNS");
    addLog("Error al iniciar el servidor DNS");
  }
}

/***********************************
 * LOOP
 ************************************/
void loop()
{
  ArduinoOTA.handle(); // Manejar actualizaciones OTA

  if (WiFi.softAPIP())
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
  bool btnStateLedPwr = digitalRead(BTN_PIN_LED_PWR);
  bool btnStatePinSet = digitalRead(BTN_PIN_SET);

  if (btnStateLedPwr == LOW && lastbtnStateLedPwr == HIGH)
  {
    tiempoInicioPresionado = millis(); // Registrar el tiempo inicial
  }
  while (btnStateLedPwr == LOW)
  {
    tiempoPresionado = millis();
    pulsadorPresionado = true;
    btnStateLedPwr = digitalRead(BTN_PIN_LED_PWR);
    if ((tiempoPresionado - tiempoInicioPresionado) > tiempoClicLargo)
    {
      break;
    }
  }
  if (pulsadorPresionado)
  {
    tiempoTotal = tiempoPresionado - tiempoInicioPresionado; // Calcular el tiempo presionado
    // Verificar si fue un clic corto o largo
    if (tiempoTotal < tiempoClicLargo)
    {
      if (millis() - lastDebounceTime > debounceDelay)
      {
        currentMode = (currentMode + 1) % 5; // Cambia de modo (0-4)
        // Serial.printf("BTN modo: %d\n", currentMode);
        addLog("BTN modo: " + String(currentMode));
        lastDebounceTime = millis();
        stepCounter = 0; // Reiniciar pasos para nuevos efectos
      }
    }
    else
    {
      // Serial.println("Apagando ESP32...");
      addLog("Apagando ESP32...");
      delay(500); // Pequeña espera
      indicateColor(colOrange);
      fullColor(colBlack);
      esp_deep_sleep_start(); // Entrar en modo de bajo consumo
    }
    pulsadorPresionado = false; // Reiniciar la bandera
  }
  if (btnStatePinSet == LOW && lastbtnStatePinSet == HIGH)
  {
    // Serial.println("------ REINICIO POR BTN ------");
    addLog("------ REINICIO POR BTN ------");
    preferences.putBool("stateInitSet", true);
    ESP.restart();
  }
  lastbtnStateLedPwr = btnStateLedPwr;
  lastbtnStatePinSet = btnStatePinSet;
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
    fullColor(c);        // Enciende los LEDs en verde
    delay(500);          // Espera 500 ms
    fullColor(colBlack); // Apaga los LEDs
    delay(500);          // Espera 500 ms
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
