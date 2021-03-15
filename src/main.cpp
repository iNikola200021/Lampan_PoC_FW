const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "STM32 Maple 0.0.1 alpha (FastLED)"
#define TINY_GSM_MODEM_SIM800
#define _TASK_STATUS_REQUEST
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#define MATRIX_PIN 17


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
TinyGsmClient client(modem);
PubSubClient mqtt(client);

#define mw 16
#define mh 16
#define NUMMATRIX (mw*mh)
CRGB leds[NUMMATRIX];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, 
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
    NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);

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
const uint32_t PBColour = matrix->Color(255,255,255); //Progress Bar Colour
const uint32_t NBColour = matrix->Color(0, 255, 0); //Notification Background Colour
const uint32_t NSColour = matrix->Color(255, 255, 255); //Notification Strip Colour
const uint32_t NSGColour = matrix->Color(180, 255, 180); //Notification Gradient Colour
//Technical variables
bool NotiOn = false;
uint32_t lastReconnectAttempt = 0;
bool IsSetupComplete = false;
String DeviceImei = "";
char DeviceID[16];
uint32_t ServTime = 10;
//uint32_t ServTimer;
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
//Light Functions
void LampAct(uint32_t Colour, byte Brightness);
void Fadein(uint32_t fadecolor, int brightness);
void FadeOut(uint32_t fadecolour, int brightness);
//Notification Functions
bool NotiOnEnable(); 
void NotiCallback();
void NotiOnDisable();

//Service functions
void error(int errcode);
void PublishRSSI();
void SignalTest();
//TASKS
//Task 1 - MQTT
//Task 2 - Notification
//Task 3 - Publish RSSI
//Task tMQTT(TASK_IMMEDIATE, TASK_FOREVER);
Task tNotification(TASK_IMMEDIATE, TASK_FOREVER, &NotiCallback, &ts,false, &NotiOnEnable, &NotiOnDisable);
Task tRSSI(ServTime*TASK_SECOND, TASK_FOREVER, &PublishRSSI, &ts);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts);
bool mqttConnect()
{
  while (!mqtt.connected())
  {
    SerialMon.print(F("MQTT?"));
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass))
    {
      Serial.println(F(" OK"));
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
            matrix->fillScreen(PBColour);
            matrix->setBrightness(k);
            matrix->show();
            delay(10);
        }
        matrix->fillScreen(0);
        matrix->show();
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
  //DynamicJsonDocument payld(100);
  //deserializeJson(payld,payload);
  if (!strncmp((char *)payload, "on", len))
  {
    SerialMon.println(F("LED ON"));
    if(!tNotification.isEnabled())
    {
    tNotification.enable();
    }
  }
  else if (!strncmp((char *)payload, "off", len))
  {
    SerialMon.println(F("LED OFF"));
    tNotification.disable();
  }
}


void setup()
{
  delay(2000);
  SerialMon.begin();
  
  SerialMon.println(F("BOOT"));
  FastLED.addLeds<NEOPIXEL,MATRIX_PIN>(leds, NUMMATRIX);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,3000);
  matrix->begin();
  matrix->setBrightness(MainBrightness);
  matrix->fillRect(0,0,2,16,PBColour);
  matrix->show();
  SerialAT.begin(115200);
  matrix->fillRect(0,0,4,16,PBColour);
  matrix->show();
  delay(6000);
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("INIT");
  SerialMon.print("Firmware ");
  SerialMon.print(FW_NAME);  
  SerialMon.print(", version ");
  SerialMon.print(FW_VERSION);
  SerialMon.print(",build ");  
  SerialMon.println(BUILD);
  matrix->fillRect(0,0,6,16,PBColour);
  matrix->show();
  //modem.restart();
  modem.init();
  String modemInfo = modem.getModemInfo();
  SerialMon.print(F("Modem Info: "));
  SerialMon.println(modemInfo);

  DeviceImei = modem.getIMEI();
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
  matrix->fillRect(0,0,8,16,PBColour);
  matrix->show();
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

    matrix->fillRect(0,0,10,16,PBColour);
    matrix->show();
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
    matrix->fillRect(0,0,12,16,PBColour);
    matrix->show();

  }
  ts.addTask(tNotification);
  ts.addTask(tRSSI);
  
  tRSSI.enable();
  // MQTT Broker setup
  SerialMon.println(F("SETUP"));
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttRX);
  matrix->fillRect(0,0,14,16,PBColour);
  matrix->show();
  mqttConnect();
  //ServTimer = millis();
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
void NotiOnDisable()
{
  FadeOut(NBColour, NotiBrightness);
  //return true;
}
void NotiCallback()
{
  
  for (byte i = 0; i <= 17; i++)
  {
    matrix->fillScreen(NBColour);
    matrix->drawLine(i, 0, i, 16, NSColour);
    matrix->drawLine(i + 1, 0, i + 1, 16, NSGColour);
    matrix->drawLine(i - 1, 0, i - 1, 16, NSGColour);
    matrix->show();
    if (i <= 8)
    {
      //delay(delays-(i*delayk));
      delay(delays);
      matrix->setBrightness(NotiBrightness + (i * brc));
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      matrix->setBrightness(100 - (i - 8)*brc);
      delay(delays);
    }
    yield();
  }
  
}
void error(int errcode)
{
    Serial.println("ERROR " + errcode);
    while(1)
    {
      matrix->fillScreen(matrix->Color(255,0,0));
      
      matrix->show();
      delay(2000);
      switch (errcode)
      {
        case 1: //Blank IMEI -> No connection with the modem
          matrix->fillScreen(matrix->Color(0,0,0));
          matrix->drawLine(8,0,8,16,matrix->Color(255,0,0));
          matrix->show();
          delay(2000);
          break;

        case 2: //No NET
          matrix->fillScreen(matrix->Color(0,0,0));
          matrix->drawLine(7,0,7,16, matrix->Color(255,0,0));
          matrix->drawLine(9,0,9,16,matrix->Color(0,255,0));
          matrix->show();
          delay(2000);
          break;

        case 3: //No GPRS
          matrix->fillScreen(matrix->Color(0,0,0));
          matrix->drawLine(6,0,6,16, matrix->Color(255,0,0));
          matrix->drawLine(8,0,8,16,matrix->Color(0,255,0));
          matrix->drawLine(10,0,10,16,matrix->Color(0,0,255));
          matrix->show();
          delay(2000);
          break;

        case 4: //No MQTT
          matrix->fillScreen(matrix->Color(0,0,0));
          matrix->drawLine(5,0,5,16, matrix->Color(255,0,0));
          matrix->drawLine(7,0,7,16, matrix->Color(0,255,0));
          matrix->drawLine(9,0,9,16,matrix->Color(255,255,255));
          matrix->drawLine(11,0,11,16,matrix->Color(0,0,255));
          matrix->show();
          delay(2000);
          break;
          
        case 5: //No SIM
          matrix->fillScreen(matrix->Color(0,0,0));
          matrix->drawLine(4,0,4,16, matrix->Color(255,0,0));
          matrix->drawLine(6,0,6,16, matrix->Color(0,255,0));
          matrix->drawLine(8,0,8,16,matrix->Color(255,255,255));
          matrix->drawLine(10,0,10,16,matrix->Color(0,0,255));
          matrix->drawLine(12,0,12,16, matrix->Color(255,0,0));
          matrix->show();
          delay(2000);
          break;
      }
    }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  matrix->setBrightness(brightness);
  matrix->fillScreen(fadecolour);
  matrix->show();
  
  for (byte k = brightness; k > 0; k--)
  {
    matrix->fillScreen(fadecolour);
    matrix->setBrightness(k);
    matrix->show();
    delay(10);
  }
  matrix->setBrightness(brightness);
  matrix->fillScreen(matrix->Color(0, 0, 0));
  matrix->show();
}
void LampAct(uint32_t Colour, byte Brightness)
{
  for (byte j = 0; j < Brightness; j++)
  {
    matrix->setBrightness(j);
    matrix->fillScreen(Colour);
    matrix->show();
  }
  matrix->setBrightness(Brightness);
  matrix->fillScreen(Colour);
  matrix->show();
}
void Fadein(uint32_t fadecolor, int brightness)
{
  for (byte j = 0; j < brightness; j++)
  {
    matrix->setBrightness(j);
    matrix->fillScreen(fadecolor);
    matrix->show();
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
void SignalTest ()
{
    int csq = modem.getSignalQuality();
    int RSSI = -113 +(csq*2);
    if (RSSI <=  -110)
    {
        matrix->fillScreen(matrix->Color(139,0,0));
    }
    else if (RSSI >  -110 && RSSI < -100)
    {
        matrix->fillScreen(matrix->Color(220,20,60));
    }
    else if (RSSI >=  -100 && RSSI < -85)
    {
        matrix->fillScreen(matrix->Color(255,165,0));
    }
    else if (RSSI >=  -85 && RSSI < -70)
    {
        matrix->fillScreen(matrix->Color(255,255,0));
    }
    else if (RSSI >=  -70)
    {
        matrix->fillScreen(matrix->Color(0,0,255));
    }
}