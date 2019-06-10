//Sensor input and MQTT

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <OneWire.h>                  //Waterproof temp sensor
#include <DallasTemperature.h>        //Waterproof temp sensor
#include <DHT.h>
#include <DHT_U.h>

//#define wifi_ssid "University of Washington"               // Wifi network SSID
//#define wifi_password ""                                   // Wifi network password
#define wifi_ssid "ourhouse1"               // Wifi network SSID
#define wifi_password "abcdefg1234567"       

#define weatherKey = "5ef236346ba98ca15c415bed6119532a"

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server
#define topic_name "maria/aquarium"        //topic you are subscribing to
#define topic_name2 "maria/switch"

WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client

String NEW_TP = "";

//typedef struct { //we created a new data type definition to hold the new data
//  String tp;     //temp coming from the weather service, this creates a slot to hold the incoming data
//} OutsideTempData;
//
//OutsideTempData conditions; //created a OutsideTempData and variable conditions

char mac[6];
char message[201];
unsigned long currentMillis, timerOne, timerTwo, timerThree;

double switchh;      //switch state from mqtt
char incoming[100]; //an array to hold the incoming message
// waterproof temp sensor is plugged into pin 15
#define ONE_WIRE_BUS 15

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// pin connected to DH22 data line
#define DATA_PIN 2

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

//power switch tail 12
const int relay_pin = 12;

//pH sensor setup
#define SensorPin A0            //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.34            //deviation compensate
#define samplingInterval 20
#define printInterval 800
#define ArrayLenth  40    //times of collection

int pHArray[ArrayLenth];   //Store the average value of the sensor feedback
int pHArrayIndex=0;

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
  Serial.println(WiFi.macAddress());  //macAddress returns a byte array 6 bytes representing the MAC address
}

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("maria/aquarium"); //we are subscribing to 'maria/aquarium' and all subtopics below that topic
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

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  setup_wifi();
  while (!Serial);
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);
  timerOne = timerTwo = timerThree = millis();

  //Start up the library
  sensors.begin();

  // initialize dht22
  dht.begin();

  //initalize power switch
  pinMode(relay_pin, OUTPUT);

}

float tempC, tempF;

void getAquaTemp() {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  tempC = sensors.getTempCByIndex(0);
  tempF = (tempC * 1.8) + 32;         //converts celsius into fahrenheit

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C)
  {
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.print(tempC);
    Serial.print(", ");
    Serial.println(tempF);
  }
  else
  {
    Serial.println("Error: Could not read temperature data");
  }

}

float celsius, fahrenheit;

void getRoomTemp() {
  sensors_event_t event;
  dht.temperature().getEvent(&event);

  celsius = event.temperature;
  fahrenheit = (celsius * 1.8) + 32;

  Serial.print("fahrenheit: ");
  Serial.print(fahrenheit);
  Serial.println("F");
}


void getOutsideTemp() {

  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://api.openweathermap.org/data/2.5/weather?q=seattle&units=imperial&appid=5ef236346ba98ca15c415bed6119532a"); //returns IP as .json object
  int httpCode = theClient.GET();
  
  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }

      //conditions.tp = root["main"]["temp"].as<String>();    //casting these values as Strings because the metData "slots" are Strings
      NEW_TP = root["main"]["temp"].as<String>();    //casting these values as Strings because the metData "slots" are Strings
      //NEW_TP = (char*)root["main"]["temp"];
    }
  }
  else {
    Serial.printf("Something went wrong with connecting to the endpoint in getMet().");
  }
}

static float pHValue,voltage;
void getPH() {
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  
  if(millis()-samplingTime > samplingInterval){
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
      pHValue = 3.5*voltage+Offset;
      samplingTime=millis();
  }
  if(millis() - printTime > printInterval){   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  
    Serial.print("Voltage:");
    Serial.print(voltage,2);
    Serial.print("    pH value: ");
    Serial.println(pHValue,2);
    printTime=millis();
  }
}
double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
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

  double switchh = root["SwitchState"];

  //controlls power switch tail
  if (switchh == 1) {  
    Serial.println("Heater on");
    digitalWrite(relay_pin, HIGH);
  } else {
    Serial.println("Heater off");
    digitalWrite(relay_pin, LOW);
  }

}


void loop() {

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'
  //Here we will deal with a JSON string
  if (millis() - timerTwo > 15000) {
    sprintf(message, "{\"AquaTemp\":\"%s\", \"RoomTemp\":\"%s\", \"OutsideTemp\":\"%s\", \"pHvalue\":\%s\"}", tempF, fahrenheit, NEW_TP.c_str(),pHValue);
    mqtt.publish("maria/aquarium", message);
    timerTwo = millis();
  }

  getAquaTemp();

  getRoomTemp();

  getOutsideTemp();

  getPH();

}


