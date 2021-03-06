const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "2.0.0 alpha"

#define TINY_GSM_MODEM_SIM800
#define _TASK_STATUS_REQUEST
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoMatrix.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_NeoPixel.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#define MATRIX_PIN PA7


#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
// Add a reception delay - may be needed for a fast processor at a slow baud rate
// #define TINY_GSM_YIELD() { delay(2); }
// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define SerialMon Serial
#define SerialAT Serial1

//Classes definition
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem);
PubSubClient mqtt(client);
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, MATRIX_PIN,
                            NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

Scheduler ts;

//APN Credentials
const char* apn = "internet.beeline.ru";
const char* gprsUser = "beeline";
const char* gprsPass = "beeline";
// MQTT details
const char* broker = "iotcreative.ru";
const char* mqtt_user = "dave";
const char* mqtt_pass = "lemontree";
const char* topicRegister = "/device/register";
char topicStatus[29] = "/device/";
char topicService[40]= "/device/";
//Light parameters
const byte brc = 10;
const byte delays = 30;
const byte MainBrightness = 40;
const byte NotiBrightness = 20;
const uint32_t PBColour = matrix.Color(255,255,255); //Progress Bar Colour
const uint32_t NBColour = matrix.Color(0, 255, 0); //Notification Background Colour
const uint32_t NSColour = matrix.Color(255, 255, 255); //Notification Strip Colour
const uint32_t NSGColour = matrix.Color(180, 255, 180); //Notification Gradient Colour
//Technical variables
bool NotiOn = false;
uint32_t lastReconnectAttempt = 0;
bool IsSetupComplete = false;
String DeviceImei = "";
char DeviceID[16];
uint32_t ServTime = 10000;
uint32_t ServTimer;
//Error variables
byte NetRT = 0;
byte GprsRT = 0;
byte MqttRT = 0;
//Error parameters
const byte NetThreshold = 1;
const byte GprsThreshold = 10;
const byte MqttThreshold = 5;

//FUNCTIONS DECLARATION
//MQTT Functions
bool mqttConnect();
void mqttRX(char* topic, byte* payload, unsigned int len);
//Notification Functions
bool NotiOnEnable(); 
void NotiCallback();
bool NotiOnDisable();
//Light Functions
void LampAct(uint32_t Colour, byte Brightness);
void Fadein(uint32_t fadecolor, int brightness);
void FadeOut(uint32_t fadecolour, int brightness);
//Service functions
void error(int errcode);
void PublishRSSI();
void SignalTest();
//TASKS
//Task 1 - MQTT
//Task 2 - Notification
//Task 3 - Publish RSSI
//Task tMQTT(TASK_IMMEDIATE, TASK_FOREVER);
Task tNotification(TASK_IMMEDIATE, TASK_FOREVER, &NotiCallback, &ts, &NotiOnEnable, &NotiOnDisable);
Task tRSSI(10000, TASK_FOREVER, &PublishRSSI, &ts);
bool mqttConnect()
{
  while (!mqtt.connected())
  {
    SerialMon.print(F("MQTT?"));
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass))
    {
      Serial.println(F(" OK"));
      //subscribe topic with default QoS 0
      mqtt.publish(topicRegister, DeviceID,16);
      int csq = modem.getSignalQuality();
      int RSSI = -113 +(csq*2);
      char serrssi[4];
      itoa(RSSI, serrssi, 10);
      mqtt.publish(topicService, serrssi);
      mqtt.subscribe(topicStatus,1);
      if (!IsSetupComplete) //Add Boot is Complete Animation
      {
        IsSetupComplete = true;
        for (byte k = MainBrightness; k > 0; k--)
        {
            matrix.fillScreen(PBColour);
            matrix.setBrightness(k);
            matrix.show();
            delay(10);
        }
        matrix.fillScreen(0);
        matrix.show();
      }
    }
    else
    {
      if(MqttRT < MqttThreshold)
      {
          SerialMon.print(F(" NOT OK, status: "));
          SerialMon.print(mqtt.state());
          Serial.println(", try again in 5 seconds");
          MqttRT++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
      else
      {
          error(4);
          
      }
    }
  }
  return mqtt.connected();
}

void mqttRX(char* topic, byte* payload, unsigned int len)
{
  SerialMon.print(F("Message ["));
  SerialMon.print(topic);
  SerialMon.print(F("]: "));
  SerialMon.write(payload, len);
  SerialMon.println();
  DynamicJsonDocument payld(100);
  deserializeJson(payld,payload);
  if (!strncmp((char *)payload, "on", len))
  {
    SerialMon.println(F("LED ON"));
    tNotification.enable();
  }
  else if (!strncmp((char *)payload, "off", len))
  {
    SerialMon.println(F("LED OFF"));
    if(NotiOn)
    {
      tNotification.disable();
    }
  }
}


void setup()
{
  delay(1000);
  SerialMon.begin();
  SerialMon.println(F("BOOT"));
  matrix.begin();
  matrix.setBrightness(MainBrightness);
  matrix.fillRect(0,0,2,16,PBColour);
  matrix.show();
  //TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  SerialAT.begin(115200);
  matrix.fillRect(0,0,4,16,PBColour);
  matrix.show();
  delay(6000);
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("INIT");
  matrix.fillRect(0,0,6,16,PBColour);
  matrix.show();
  modem.restart();
  //modem.init();
  //SerialMon.println(" OK");
  String modemInfo = modem.getModemInfo();
  SerialMon.print(F("Modem Info: "));
  SerialMon.println(modemInfo);

  DeviceImei = modem.getIMEI();
  //SerialMon.println("Lamp IMEI number: "+ DeviceImei);
  DeviceImei.toCharArray(DeviceID, 16);
  SerialMon.print("Lamp ID number: ");
  SerialMon.println(DeviceID);
  strcat(topicService, DeviceID);
  strcat(topicStatus, DeviceID);
  strcat(topicService, "/GSM/RSSI");
  strcat(topicStatus, "/lamp");
  SerialMon.print("TopicStatus: ");
  SerialMon.println(topicStatus);
  SerialMon.println(topicService);
  if(DeviceImei == "")
  {
    SerialMon.println("Error 1: No modem");
    error(1);
  }
  matrix.fillRect(0,0,8,16,PBColour);
  matrix.show();
  if(modem.getSimStatus() != 1)
  {
    //Serial.print("NO SIM");
    error(5);
  }
  SerialMon.print("NET? ");
  if (!modem.waitForNetwork())
  {
    NetRT++;
    SerialMon.println("!OK");
    error(2);
  }
  if (modem.isNetworkConnected())
  {
    NetRT = 0;
    SerialMon.println("OK");

    matrix.fillRect(0,0,10,16,PBColour);
    matrix.show();
  }
  int csq = modem.getSignalQuality();
  int RSSI = -113 +(csq*2);
  SerialMon.print("RSSI? ");
  SerialMon.println(RSSI);

  SerialMon.print("GPRS?");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    GprsRT++;
    SerialMon.println(F(" !OK"));
    delay(10000);
    if(GprsRT > GprsThreshold)
    {
      error(2); 
    }
    return;
  }
  if (modem.isGprsConnected()) 
  {
    SerialMon.println(F(" OK"));
    matrix.fillRect(0,0,12,16,PBColour);
    matrix.show();

  }
  ts.addTask(tNotification);
  ts.addTask(tRSSI);
  
  tRSSI.enable();
  // MQTT Broker setup
  SerialMon.println(F("SETUP"));
  mqtt.setServer(broker, 8883);
  mqtt.setCallback(mqttRX);
  matrix.fillRect(0,0,14,16,PBColour);
  matrix.show();
  ServTimer = millis();
}

void loop()
{
  ts.execute();
  if (!mqtt.connected())
  {
    SerialMon.println(F("=== MQTT DISCONNECT ==="));
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L)
    {
      
      lastReconnectAttempt = t;
      if (mqttConnect())
      {
        lastReconnectAttempt = 0;
        MqttRT = 0;
      }
      
    }
    
    return;

  }
  mqtt.loop();
}
bool NotiOnEnable()
{
    Fadein(NBColour, NotiBrightness);
    return true;
}
bool NotiOnDisable()
{
  FadeOut(NBColour, NotiBrightness);
  return true;
}
void NotiCallback()
{
  
  for (byte i = 0; i <= 17; i++)
  {
    matrix.fillScreen(NBColour);
    matrix.drawLine(i, 0, i, 16, NSColour);
    matrix.drawLine(i + 1, 0, i + 1, 16, NSGColour);
    matrix.drawLine(i - 1, 0, i - 1, 16, NSGColour);
    matrix.show();
    if (i <= 8)
    {
      //delay(delays-(i*delayk));
      delay(delays);
      matrix.setBrightness(NotiBrightness + (i * brc));
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      matrix.setBrightness(100 - (i - 8)*brc);
      delay(delays);
    }
  }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  matrix.setBrightness(brightness);
  matrix.fillScreen(fadecolour);
  matrix.show();
  
  for (byte k = brightness; k > 0; k--)
  {
    matrix.fillScreen(fadecolour);
    matrix.setBrightness(k);
    matrix.show();
    delay(10);
  }
  matrix.setBrightness(brightness);
  matrix.fillScreen(matrix.Color(0, 0, 0));
  matrix.show();
}
void LampAct(uint32_t Colour, byte Brightness)
{
  for (byte j = 0; j < Brightness; j++)
  {
    matrix.setBrightness(j);
    matrix.fillScreen(Colour);
    matrix.show();
  }
  matrix.setBrightness(Brightness);
  matrix.fillScreen(Colour);
  matrix.show();
}
void Fadein(uint32_t fadecolor, int brightness)
{
  for (byte j = 0; j < brightness; j++)
  {
    matrix.setBrightness(j);
    matrix.fillScreen(fadecolor);
    matrix.show();
  }
}
void error(int errcode)
{
    Serial.println("ERROR " + errcode);
    while(1)
    {
      matrix.fillScreen(matrix.Color(255,0,0));
      
      matrix.show();
      delay(2000);
      if(errcode == 1) //Blank IMEI -> No connection with the modem
      {
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(8,0,8,16,matrix.Color(255,0,0));
          matrix.show();
          delay(2000);
      }
      if(errcode == 2) //No NET
      {
        matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(7,0,7,16, matrix.Color(255,0,0));
          matrix.drawLine(9,0,9,16,matrix.Color(0,255,0));
          matrix.show();
          delay(2000);
      }
      if(errcode == 3) //No GPRS
      {
        matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(6,0,6,16, matrix.Color(255,0,0));
          matrix.drawLine(8,0,8,16,matrix.Color(0,255,0));
          matrix.drawLine(10,0,10,16,matrix.Color(0,0,255));
          matrix.show();
          delay(2000);
      }
      if(errcode == 4)  //No MQTT
      {
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(5,0,5,16, matrix.Color(255,0,0));
          matrix.drawLine(7,0,7,16, matrix.Color(0,255,0));
          matrix.drawLine(9,0,9,16,matrix.Color(255,255,255));
          matrix.drawLine(11,0,11,16,matrix.Color(0,0,255));
          matrix.show();
          delay(2000);
      }
      if(errcode == 5) //No SIM
      {
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(4,0,4,16, matrix.Color(255,0,0));
          matrix.drawLine(6,0,6,16, matrix.Color(0,255,0));
          matrix.drawLine(8,0,8,16,matrix.Color(255,255,255));
          matrix.drawLine(10,0,10,16,matrix.Color(0,0,255));
          matrix.drawLine(12,0,12,16, matrix.Color(255,0,0));
          matrix.show();
          delay(2000);
      }
    }
}
void PublishRSSI ()
{
      int csq = modem.getSignalQuality();
      int RSSI = -113 +(csq*2);
      char serrssi[4];
      itoa(RSSI, serrssi, 10);
      mqtt.publish(topicService, serrssi);
}
