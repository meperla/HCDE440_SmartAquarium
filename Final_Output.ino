//Outputs and display

#include <ESP8266WiFi.h>
#include "Wire.h"          
#include <PubSubClient.h>  
#include <ArduinoJson.h>  
#include <Servo.h>
//included for the 128x64 i2c OLED screen
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, & Wire, OLED_RESET);

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

Servo myservo;

const int switchPin = 2;
int switchVal = HIGH;

//#define wifi_ssid "University of Washington"               // Wifi network SSID
//#define wifi_password ""                                   // Wifi network password
#define wifi_ssid "ourhouse1"           
#define wifi_password "abcdefg1234567"


#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server
#define topic_name "maria/aquarium"        //topic you are subscribing to
#define topic_name2 "maria/switch"

WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client

char mac[6];
char message[201];
unsigned long currentMillis, timerOne, timerTwo, timerThree;

double aTemp;   //aquarium temp
double rTemp;   //room temp
double oTemp;   //outside temp
double pH;      //ph value

char incoming[100]; //an array to hold the incoming message

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
} 

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("maria/aquarium"); //we are subscribing to 'fromJon/words' and all subtopics below that topic
      mqtt.subscribe("maria/switch");
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //DJB
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }

  double aTemp=root["AquaTemp"];
  double rTemp=root["RoomTemp"];
  double oTemp=root["OutsideTemp"];
  double pH=root["pHvalue"];  
}

void setup() {
  Serial.begin(115200);

  setup_wifi();
  
  while(!Serial);
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);
  timerOne = timerTwo = timerThree = millis();

  myservo.attach(13);

  pinMode(switchPin, INPUT);

  //setup for the OLED screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initailize with the i2c addre 0x3C
  display.clearDisplay();                    //Clears any existing images
  display.setTextSize(2);                    //Set text size
  display.setTextColor(WHITE);               //Set text color to WHITE
  display.setCursor(0,0);                    //Puts cursor back on top left corner
  display.println("Starting up...");         //Test and write up
  display.display();                         //Displaying the display

  drawTitles();
}

void loop() {
  switchVal = digitalRead(switchPin);
  
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active' 
    if (millis() - timerTwo > 15000) {
    sprintf(message, "{\"SwitchState\":\"%s\"}", switchVal);
    mqtt.publish("maria/switch", message);
    timerTwo = millis();
  }

  displayInfo();
  servoMotorDisplay();
  
}

void drawTitles() {
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Aquarium");

  display.display();

}

void displayInfo() {
 
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 19);
  //Room Temp Display
  display.print("Room Temp: ");
  display.print(rTemp);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.println("F");
  //Outside Temp Display
  display.print("Outside Temp: ");
  display.println(oTemp);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.println("F");
  //pH level display
  display.print("pH level: ");
  display.println(pH);
  
  display.display();
}

void servoMotorDisplay() {
  int temp = aTemp;
  temp = map(temp, 0, 100, 0, 180);

  if(temp < 0){
    temp = 0;
  }else if(temp > 180){
    temp = 180;
  }
   
  myservo.write(temp);
  
}

