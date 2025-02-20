#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

#define PIN 2
#define NUMPIXELS 6
#define TIME 100

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500

const char *ssid = "REYES_CLARO";
const char *password = "172540871820";

WebServer server(80);

void handleRoot()
{
  if (!SPIFFS.exists("/index.html"))
  {
    Serial.println("El archivo index.html no existe en SPIFFS");
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }

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
  if (!SPIFFS.exists("/chroma.png"))
  {
    Serial.println("El archivo chroma.png no existe en SPIFFS");
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
  File file = SPIFFS.open("/chroma.png", "r");
  if (!file)
  {
    server.send(500, "text/plain", "Error al abrir chroma.png");
    return;
  }
  server.streamFile(file, "image/webp");
  file.close();
}
void handleScript()
{
  if (!SPIFFS.exists("/script.min.js"))
  {
    Serial.println("El archivo script.min.js no existe en SPIFFS");
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
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

    Serial.printf("RGB: %d, %d, %d\n", r, g, b);
    // Crear un objeto JSON
    String jsonResponse = "{\"r\": " + r + ", \"g\": " + g + ", \"b\": " + b + "}";

    server.send(200, "application/json", jsonResponse);
  }
  else if (server.hasArg("option"))
  {
    String data = server.arg("option");
    String validOptions[] = {"a", "b", "c", "d", "e"};

    if (std::find(std::begin(validOptions), std::end(validOptions), data) != std::end(validOptions))
    {
      server.send(200, "text/plain", "opcion recibido");
      if (data == "a")
      {
        Serial.println("Los Leds se encienden consecutivamente desdel 0 hasta el último");
        colorWipe(pixels.Color(255, 0, 0), TIME); // Los Leds se encienden consecutivamente desdel 0 hasta el último
      }
      else if (data == "b")
      {
        Serial.println("Los leds se encienden y apagan alternativamente uno si y uno no");
        theaterChase(pixels.Color(127, 0, 0), TIME); // Los leds se encienden y apagan alternativamente uno si y uno no
      }
      if (data == "c")
      {
        Serial.println("Todos los leds van pasando progresivamente por todos los colores");
        rainbow(TIME); // Todos los leds van pasando progresivamente por todos los colores
      }
      if (data == "d")
      {
        Serial.println("Ciclo de arcoiris progresivo en cada pixel");
        rainbowCycle(TIME); // Ciclo de arcoiris progresivo en cada pixel
      }
      if (data == "e")
      {
        Serial.println("Todos los leds se apagan y encienden rápidamente");
        theaterChaseRainbow(TIME); // Todos los leds se apagan y encienden rápidamente
      }
    }
  }
  else
  {
    server.send(400, "text/plain", "Parametros incorrectos");
  }
}

/************ MAIN BUCLE ************/
void setup()
{
  Serial.begin(115200);
  Serial.println("---Init---");

  // Montamos SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("Error al montar SPIFFS");
    return;
  }
  Serial.println("SPIFFS montado correctamente");

  // Conectar a WiFi
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20)
  {
    Serial.printf("Intento %d - Estado WiFi: %d\n", intentos, WiFi.status());
    delay(1000);
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Conectado a la red WiFi");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("No se pudo conectar a WiFi");
    ESP.restart();
  }

  // Rutas del servidor
  server.on("/", handleRoot);
  server.on("/chroma.png", handleImage);
  server.on("/script.min.js", handleScript);
  server.on("/api", handleColor);

  // Inicia el servidor web
  server.begin();
}

void loop()
{
  server.handleClient();
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait)
{
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
  {
    pixels.setPixelColor(i, c);
    pixels.show();
    // delay(wait);
  }
}

void rainbow(uint8_t wait)
{
  uint16_t i, j;
  for (j = 0; j < 256; j++)
  {
    for (i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel((i + j) & 255));
    }
    pixels.show();
    // delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait)
{
  uint16_t i, j;
  for (j = 0; j < 256 * 5; j++)
  { // 5 cycles of all colors on wheel
    for (i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
    }
    pixels.show();
    // delay(wait);
  }
}

// Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait)
{
  for (int j = 0; j < 10; j++)
  { // do 10 cycles of chasing
    for (int q = 0; q < 3; q++)
    {
      for (int i = 0; i < pixels.numPixels(); i = i + 3)
      {
        pixels.setPixelColor(i + q, c); // turn every third pixel on
      }
      pixels.show();
      // delay(wait);
      for (int i = 0; i < pixels.numPixels(); i = i + 3)
      {
        pixels.setPixelColor(i + q, 0); // turn every third pixel off
      }
    }
  }
}

// Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait)
{
  for (int j = 0; j < 256; j++)
  { // cycle all 256 colors in the wheel
    for (int q = 0; q < 3; q++)
    {
      for (int i = 0; i < pixels.numPixels(); i = i + 3)
      {
        pixels.setPixelColor(i + q, Wheel((i + j) % 255)); // turn every third pixel on
      }
      pixels.show();
      // delay(wait);
      for (int i = 0; i < pixels.numPixels(); i = i + 3)
      {
        pixels.setPixelColor(i + q, 0); // turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
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