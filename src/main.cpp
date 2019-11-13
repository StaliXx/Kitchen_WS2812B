#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <PubSubClient.h>

// #define BEDROOM
#define KITCHEN

#ifdef BEDROOM
const char* host = "esp8266-bedroom-ws2812b";
#define COLOR_TOPIC "cmnd/bedroom_ws2812b/COLOR"
#define STATUS_TOPIC "cmnd/bedroom_ws2812b/STATUS"
#define STAT_COLOR_TOPIC "stat/bedroom_ws2812b/COLOR"
#define STAT_STATUS_TOPIC "stat/bedroom_ws2812b/STATUS"
#define PIXEL_COUNT 300
#endif

#ifdef KITCHEN
const char* host = "esp8266-kitchen-ws2812b";
#define COLOR_TOPIC "cmnd/kitchen_ws2812b/COLOR"
#define STATUS_TOPIC "cmnd/kitchen_ws2812b/STATUS"
#define STAT_COLOR_TOPIC "stat/kitchen_ws2812b/COLOR"
#define STAT_STATUS_TOPIC "stat/kitchen_ws2812b/STATUS"
#define PIXEL_COUNT 72
#endif

// const char* mqtt_server = "stalixx.ddns.net";
// #define mqtt_port 12081
const char* mqtt_server = "192.168.43.2";
#define mqtt_port 1883

#define BUTTON_PIN   13  // D7
#define PIXEL_PIN    14  // D5

Button myBtn(BUTTON_PIN,300, true, false);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool oldState = HIGH;
int showType = 0;
long lastReconnectAttempt = 0;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient espClient;
PubSubClient client(espClient);

int status_value = 0;
long color_value = 0xffff64;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
 
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void togglePower (int status_new){
  if (status_new != status_value){
    status_value = status_new;
    if (status_new){
      client.publish(STAT_STATUS_TOPIC,"1");
      client.publish(STAT_COLOR_TOPIC, (char*) String(color_value).c_str());
      Serial.println ("Status = 1");
      Serial.print ("Color = ");
      Serial.println (String(color_value, HEX));
      colorWipe(color_value, 10);
    } else {
      client.publish(STAT_STATUS_TOPIC,"0");
      Serial.println ("Status = 0");
      colorWipe(0x000000, 10);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [" + String(topic) + "] ");
  String value = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    value = value + ((char)payload[i]);
  }
  Serial.println();
  if (String(topic) == COLOR_TOPIC){
    long color_wire = (long) strtol( &value[0], NULL, 16);
    if (color_wire){
      color_value = color_wire;
    }
    Serial.println(color_wire);
    colorWipe(color_wire,10);
  }
  if (String(topic) == STATUS_TOPIC){
    Serial.print("Status_topic = ");
    Serial.print(value);
    Serial.println();
    togglePower(value.toInt());
  }
  // if ((char)payload[0] == '1') {
  //   colorWipe(strip.Color(255, 255, 100), 10);
  // } else if ((char)payload[0] == '2'){
  //   colorWipe(0xFF0000, 10);
  // } else if ((char)payload[0] == '3'){
  //   colorWipe(strip.Color(255, 15, 255), 10);
  // } else if ((char)payload[0] == '4'){
  //   rainbow(20);
  // } else if ((char)payload[0] == '0'){
  //   colorWipe(strip.Color(0, 0, 0), 10);
  // } else {
  //   colorWipe(color_wire,10);
  // }
}

boolean reconnect() {
  if (client.connect(host)) {    
    client.subscribe(COLOR_TOPIC);
    client.subscribe(STATUS_TOPIC);
    Serial.print("Podpisalis");
  Serial.println();
  }
  return client.connected();
}

void setup() {
    Serial.begin(115200);

    myBtn.begin();
    strip.begin();
    // colorWipe(strip.Color(255, 255, 100), 10);
    rainbow(20);
    strip.show(); // Initialize all pixels to 'off'
    
    WiFiManager wifiManager;
    wifiManager.autoConnect(host,"9268168144");
    //if you get here you have connected to the WiFi
    Serial.println("WiFi connected... :)");
    delay(100);

    MDNS.begin(host);
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

    ArduinoOTA.setPort(8266); // Port defaults to 8266
    ArduinoOTA.setHostname(host); // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setPassword("9268168144"); // No authentication by default
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.print("Arduino OTA started. IP address: ");
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    lastReconnectAttempt = 0;
    Serial.print ("MQTT: ");
    Serial.print(mqtt_server);
    Serial.print (":");
    Serial.println(mqtt_port);
}

void loop(void){
  httpServer.handleClient();
  ArduinoOTA.handle();

  #ifndef BEDROOM
  myBtn.read();
  if (myBtn.wasReleased())    // if the button was released, change the LED state
    {
        colorWipe(strip.Color(0, 0, 0), 10);
        client.publish("stat/kitchen_ws2812b/COLOR","000000");
        client.publish("stat/kitchen_ws2812b/STATUS","0");
    }
  if (myBtn.wasPressed())    // if the button was released, change the LED state
    {
        colorWipe(strip.Color(255, 255, 100), 10);
        client.publish("stat/kitchen_ws2812b/COLOR","ffff64");
        client.publish("stat/kitchen_ws2812b/STATUS","1");
    }
  #endif

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }
}
