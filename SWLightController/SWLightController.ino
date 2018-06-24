#include "credentials.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

// Set the PIN that we'll be using on the Wemos to control our LED strip
#define LED_CONTROL_PIN D4

// Set up an objecft to control our NeoPixel LED strip
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed
Adafruit_NeoPixel strip = Adafruit_NeoPixel(45, LED_CONTROL_PIN, NEO_GRB + NEO_KHZ800);

// Set the credentials to get onto our wireless network
// (Note: These are defined in the local credentials.h file so my network secrets aren't in the public git repository.)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

// Create a webserver running off port 80
ESP8266WebServer server(80);

// Create a "this is me" 'ping' response for root
void handleRoot() {
  server.send(200, "text/plain", "The Imperial Star Wars display light controller is alive!");
}

// Deal with our wonderful 404 page, that any good application should have.
void handleNotFound(){
  String message = "<html><title>Looking?  Found someone, you have, I would say, hmmmm?</title>";
  message += "<body style='background-color:#151515; color:#888888;'><div align='center'>";
  message += "<img src='https://static-mh.content.disney.io/starwars/assets/errors/e404-4ced2cd1702d.jpg' alt='404 Error' width='600px' height='403px'>";
  message += "<h2>This URI is not yet operational.</h2><span style='color:white;'>";
  message += "URI: ";
  message += server.uri();
  message += "<br>Method: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "<br>Arguments: ";
  message += server.args();
  message += "<br>";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "<br>";
  } 
  
  message += "</span></div></body></html>";
  server.send(404, "text/html", message);
}

// Handle the setup step of the Arduino lifecycle
void setup() {

  // Initialize the LED strip and show all pixels "off"
  strip.begin();
  strip.show();

  // Set the bps rate for serial data tramsmission
  Serial.begin(115200);
  Serial.println("Booting");

  // Connect to the WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Set the Host Name for this device
  ArduinoOTA.setHostname("ImperialLights");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");


  // Lifecycle callback for starting an Over the Air update
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);  
  });

  // Lifecycle callback for finishing the Over the Air update
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  // Lifecycle callback to show current progress of an Over the Air update
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  // Lifecycle callback to handle an error in the Over the Air Update process
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  // Begin the Arduinio Over The Air Update process
  ArduinoOTA.begin();

  // Set up Multi-cast DNS so we can hit this at imperiallights.local
  if (MDNS.begin("imperiallights")) {
    Serial.println("MDNS responder started");
  }

  // Set up all of our server URIs:
  // Root address
  server.on("/", handleRoot);

  // Method to turn on all lights to a given color
  server.on("/on", lightOn);

  // Method to turn off all lights
  server.on("/off", lightOff);

  // Hanle our wonderful 404 error
  server.onNotFound(handleNotFound);

  // Start our HTTP Web Server
  server.begin();
  Serial.println("HTTP server started");
}

// The Loop lifecyle method for the Arduino.
// As stated in the name, this method endlessly repeats.
void loop() {
  
  // Handle any client requests from the HTTP server
  server.handleClient();

  // Handle any Over the Air update requests
  ArduinoOTA.handle();
}


// Method to shut off all lights
void lightOff() {
  // Set all pixels to 'off'
  for (uint32_t i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();

  server.send(200, "text/plain", "Light turned off!");
}


// Method to turn on all lights
void lightOn() {
  uint32_t red = 10;
  uint32_t green = 100;
  uint32_t blue = 10;

  // Handle if an RGB code is sent as parameters
  if (server.args() == 3) {
      red = (uint32_t) server.arg(0).toInt();
      green = (uint32_t) server.arg(1).toInt();
      blue = (uint32_t) server.arg(2).toInt();
  }

  // Handle arguments if sent from the "Color Sender" test applicaiton I'm using
  // For some reason it has a first parameter it sends that is always one, then RGB.
  // Go figure....
  if (server.args() == 4) {
      red = (uint32_t) server.arg(1).toInt();
      green = (uint32_t) server.arg(2).toInt();
      blue = (uint32_t) server.arg(3).toInt();
  }

  // Set all pixels to be the new color.
  for (uint16_t i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, red, green, blue);
  }

  strip.show();

  server.send(200, "text/plain", "Light turned on!");

  delay(20);
}
