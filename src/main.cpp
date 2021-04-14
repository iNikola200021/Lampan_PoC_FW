const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "STM32 Maple 0.0.1 (FastLED)"
#define TINY_GSM_MODEM_SIM800
#define ARDUINOJSON_USE_LONG_LONG 1
#define _TASK_STATUS_REQUEST
#include <Arduino.h>
#include <cstdlib>
#include <Wire.h>
#include <SPI.h>
#include <FastLED.h>
#include <FastLED_Neomatrix.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <Adafruit_GFX.h>
#define MATRIX_PIN BOARD_SPI1_MOSI_PIN
#define BTNPIN 17

#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define ARDUINOJSON_USE_LONG_LONG  1

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
const char* apn = "mts";
const char* gprsUser = "mts";
const char* gprsPass = "mts";
// MQTT details
const char* broker = "mqtt.iotcreative.ru";
const char* mqtt_user = "dave";
const char* mqtt_pass = "lemontree";
const char* topicRegister = "/device/register";
int port = 1884;
//char topicStatus[29] = "/device/";
char topicState[33]= "$device/";
char topicCmd[32]= "$device/";
//Pulse anim
const byte PulsePeak = 100;
const byte PulseDelay = 10;
const byte PulseMin = 0;
//Light parameters
byte MainBrightness = 40;
byte NotiBrightness = 20;
float WaveDelay = 2000;
const uint32_t PBColour = matrix->Color(255,255,255); //Progress Bar Colour
uint32_t NSColour = matrix->Color(255, 255, 255); //Notification Strip Colour
uint32_t NSGColour = matrix->Color(180, 255, 180); //Notification Gradient Colour 
 //Main Background Colour
uint16_t NotiColour = matrix->Color(0, 255, 0);
uint64_t sendR = 0;
uint32_t sendG = 0;
int sendB = 0;
bool TestMode = false;
//Technical variables
bool NotiOn = false;
bool IsSetupComplete = false;
char DeviceID[16];
uint32_t StateFreq = 60;
//Error variables
byte NetRT = 0;
byte GprsRT = 0;
byte MqttRT = 0;
//Error parameters
const byte NetThreshold = 1;
const byte GprsThreshold = 10;
const byte MqttThreshold = 5;
uint32_t lastReconnectAttempt = 0;
//FUNCTIONS DECLARATION
//MQTT Functions
bool mqttConnect();
void mqttRX(char* topic, byte* payload, unsigned int len);
//Light Functions
void Fadein(uint32_t fadecolor, int brightness);
void FadeOut(uint32_t fadecolour, int brightness);
//Notification Functions
bool NotiOnEnable(); 
void NotiCallback();
void NotiOnDisable();
//Animation 
byte pattern = 2;
void Light(); //0 -light, 1 - wave, 2 - ...
void wave();
void pulse();
//Service functions
void error(int errcode);
void PublishState();
void SignalTest();
bool SignalTestEn();
void SignalTestDis();
void JSONApply(byte* payload, unsigned int len);
void button();
//TASKS
//Task 1 - Notification
//Task 2 - Publish State
//Task 3 - Signal Test
Task tNotification(WaveDelay*TASK_MILLISECOND, TASK_FOREVER, &NotiCallback, &ts,false, &NotiOnEnable, &NotiOnDisable);
Task tState(StateFreq*TASK_SECOND, TASK_FOREVER, &PublishState, &ts);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts, false, &SignalTestEn, &SignalTestDis);

bool mqttConnect()
{
  while (!mqtt.connected())
  {
    SerialMon.print(F("MQTT?"));
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass))
    {
      Serial.println(F(" OK"));
      mqtt.publish(topicRegister, DeviceID,16);
      PublishState();
      //mqtt.publish(topicService, serrssi);
      //mqtt.subscribe(topicStatus,1);
      mqtt.subscribe(topicCmd,1);
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
  SerialMon.print(F(" status: "));
  SerialMon.print(mqtt.state());
  SerialMon.print(F(", "));
  SerialMon.println(mqtt.connected());
  return mqtt.connected();
}
void mqttRX(char* topic, byte* payload, unsigned int len)
{
  SerialMon.print(F("Message ["));
  SerialMon.print(topic);
  SerialMon.print(F("]: "));
  SerialMon.write(payload, len);
  SerialMon.println();
  JSONApply(payload, len);
}
void setup()
{
  SerialMon.begin();
  
  FastLED.addLeds<NEOPIXEL,MATRIX_PIN>(leds, NUMMATRIX);
  matrix->begin();
  FastLED.setMaxPowerInMilliWatts(13000);
  matrix->setBrightness(MainBrightness);
  matrix->fillRect(0,0,2,16,PBColour);
  matrix->show();
  SerialAT.begin(115200);
  matrix->fillRect(0,0,4,16,PBColour);
  matrix->show();
  delay(6000);

  SerialMon.print("Firmware ");
  SerialMon.print(FW_NAME);  
  SerialMon.print(", version ");
  SerialMon.print(FW_VERSION);
  SerialMon.print(",build ");  
  SerialMon.println(BUILD);
  matrix->fillRect(0,0,6,16,PBColour);
  matrix->show();
  modem.init();
  String DeviceImei = modem.getIMEI();
  if(DeviceImei == "")
  {
    error(1);
  }
  DeviceImei.toCharArray(DeviceID, 16);
  SerialMon.print("Lamp ID number: ");
  SerialMon.println(DeviceID);
  //Topics formation
    strcat(topicState, DeviceID);
    //strcat(topicStatus, DeviceID);
    strcat(topicCmd, DeviceID);
    strcat(topicState, "/state");
    //strcat(topicStatus, "/lamp");
    strcat(topicCmd, "/command");

  matrix->fillRect(0,0,8,16,PBColour);
  matrix->show();
  if(modem.getSimStatus() != 1)
  {
    error(5);
  }
  SerialMon.print("NET? ");
  if (!modem.waitForNetwork())
  {
    error(2);
  }
  if (modem.isNetworkConnected())
  {
    NetRT = 0;
    SerialMon.println("OK");
    matrix->fillRect(0,0,10,16,PBColour);
    matrix->show();
  }
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
  ts.addTask(tState);
  
  // MQTT Broker setup
  SerialMon.println(F("SETUP"));
  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttRX);
  pinMode(BTNPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BTNPIN), &button, RISING);
  matrix->fillRect(0,0,14,16,PBColour);
  matrix->show();
  mqttConnect();
  tState.enable();
  //ServTimer = millis();
}
void loop()
{
  ts.execute();
  if (!mqtt.connected())
  {
    mqttConnect();
    return;

  }
  
  mqtt.loop();
}
//Notification Task
bool NotiOnEnable()
{
    Fadein(NotiColour, NotiBrightness);
    return true;
}
void NotiOnDisable()
{
    FadeOut(NotiColour, NotiBrightness);
}
void NotiCallback()
{
  for (byte i = 0; i <= PulsePeak; i++)
  {
    if (i < (PulsePeak/2))
    {
      FastLED.setBrightness(i);
      delay(PulseDelay);
      
    }
    else if (i == (PulsePeak/2))
    {
      FastLED.setBrightness(i);
      delay(WaveDelay);
    }
    else
    {
      FastLED.setBrightness(PulsePeak - i);
      delay(PulseDelay);
    }
  }
}
//Signal Test Task
void SignalTest ()
{
    int RSSI = -113 +(modem.getSignalQuality()*2);
    if (RSSI <=  -110)
    {
        matrix->fillScreen(matrix->Color(139,0,0));
        matrix->show();
    }
    else if (RSSI >  -110 && RSSI < -100)
    {
        matrix->fillScreen(matrix->Color(220,20,60));
        matrix->show();
    }
    else if (RSSI >=  -100 && RSSI < -85)
    {
        matrix->fillScreen(matrix->Color(255,165,0));
        matrix->show();
    }
    else if (RSSI >=  -85 && RSSI < -70)
    {
        matrix->fillScreen(matrix->Color(255,255,0));
        matrix->show();
    }
    else if (RSSI >=  -70)
    {
        matrix->fillScreen(matrix->Color(0,0,255));
        matrix->show();    
        }
}
bool SignalTestEn()
{
  for(int i = 0; i<3; i++)
  {
    matrix->setBrightness(MainBrightness);
    matrix->fillScreen(matrix->Color(255,165,0));
    matrix->show();
    delay(500);
    matrix->fillScreen(matrix->Color(0,0,0));
    matrix->show();
    delay(500);
  }
  SerialMon.println(F("SignalTest has been started"));
  return true;
}
void SignalTestDis()
{
  for(int i = 0; i<5; i++)
  {
    matrix->setBrightness(MainBrightness);
    matrix->fillScreen(matrix->Color(255,165,0));
    matrix->show();
    delay(500);
    matrix->fillScreen(matrix->Color(0,0,0));
    matrix->show();
    delay(500);
  }
  SerialMon.println(F("SignalTest has been disabled"));
}
//Service 
void PublishState ()
{
      char status[128];
      StaticJsonDocument<128> state;
      if(NotiOn)
      {
          state["mode"] = "light";
      }
      else
      {
          state["mode"] = "off";
      }
      uint64_t colour = (sendR*1000000000)+(sendG*1000000)+(sendB*1000)+NotiBrightness;
      state["color"] = colour;
      SerialMon.println(colour);
      state["RSSI"] = -113 +(modem.getSignalQuality()*2);
      serializeJson(state, status);
      mqtt.publish(topicState, status);
      SerialMon.print("Published state: ");
      SerialMon.print(status);
      SerialMon.println("!");
}
void JSONApply(byte *payload, unsigned int len)
{
    StaticJsonDocument<100> config;
    char payld[len];
    for(unsigned int i=0; i < len; i++)
    {
       payld[i] = (char)payload[i];
    }
    DeserializationError error = deserializeJson(config, payld);
    if (error)
    {
       SerialMon.println(F("Not JSON or deserialize error"));
       return;
    }
    if (config["mode"] == "light")
    {
      SerialMon.println(F("Light mode"));
      TestMode = false;
      tSignalTest.disable();

          //Colour change
          pattern = config["pattern"];
          uint64_t colour = config["color"];
          int r = colour / 1000000000;
          int g = (colour % 1000000000)/1000000;
          int b = (colour % 1000000)/1000;
          int a = colour % 1000;
          SerialMon.print(F("Got colour (RGBA): "));
          SerialMon.print(r);
          SerialMon.print(F(", "));
          SerialMon.print(g);
          SerialMon.print(F(", "));
          SerialMon.print(b);
          SerialMon.print(F(", "));
          SerialMon.println(a);
          uint32_t NewColour = matrix->Color(r,g,b);
          byte NewBr = a;
          if (NotiColour != NewColour || NotiBrightness != NewBr)
          {
            tNotification.disable();
            NotiColour = matrix->Color(r,g,b);
            sendR = r;
            sendG = g;
            sendB = b;
            NotiBrightness = a;
            PublishState();
            NotiOn = false;
            tNotification.enable();
          }
          
          //Colour change end

    }
    else if (config["mode"] == "test")
    {
      SerialMon.println(F("Config: Test mode"));
      TestMode = true;
      NotiOn = false;
      tNotification.disable();
      tSignalTest.enable();
    }
    if(config["mode"]=="off")
    {
        NotiOn = false;
        SerialMon.println(F("LED OFF"));
        tNotification.disable();
        PublishState();
    }
}
void error(int errcode)
{
    Serial.println("ERROR " + errcode);
    delay(500);
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
void button()
{
  SerialMon.println("button!");
  NotiOn = false;
  //tNotification.disable();
}
//Animations
void Fadein(uint32_t fadecolor, int brightness)
{
  FastLED.setBrightness(0);
  matrix->fillScreen(fadecolor);
  matrix->show();
  for (byte j = 0; j < brightness; j++)
  {
    FastLED.setBrightness(j);
    FastLED.delay(1);
    //matrix->fillScreen(fadecolor);
    //matrix->show();
  }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  //FastLED.setBrightness(brightness);
  for (byte k = brightness; k > 0; k-=2)
  {
    FastLED.setBrightness(k);
    FastLED.delay(1);
  }
  matrix->fillScreen(matrix->Color(0, 0, 0));
  matrix->show();
}
void Light()
{
  if(!NotiOn)
  {
      NotiOn = true;
      matrix->setBrightness(NotiBrightness);
      matrix->fillScreen(NotiColour);
      matrix->show();
  }
  else
  {
    return;
  }
}
void pulse()
{
  float brc = (PulsePeak - PulseMin) / 8;
  for (byte i = 0; i < 16; i++)
  {
    if (i <= 8)
    {
      FastLED.setBrightness(PulseMin + (i * brc));
      FastLED.delay(PulseDelay);
      
    }
    else
    {
      FastLED.setBrightness(PulsePeak - (i - 8)*brc);
      FastLED.delay(PulseDelay);
      //FastLED.show();
    }
  }
}
void wave()
{
  float brc = (NotiBrightness - PulseMin) / 8;
  for (byte i = 0; i <= 17; i++)
  {
    matrix->fillScreen(NotiColour);
    matrix->drawLine(i, 0, i, 16, NSColour);
    matrix->drawLine(i + 1, 0, i + 1, 16, NSGColour);
    matrix->drawLine(i - 1, 0, i - 1, 16, NSGColour);
    matrix->show();
    if (i <= 8)
    {
      //delay(delays-(i*delayk));
      FastLED.setBrightness(PulseMin + (i * brc));
      FastLED.delay(PulseDelay);
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      FastLED.setBrightness(NotiBrightness - (i - 8)*brc);
      FastLED.delay(PulseDelay);
    }
  }
}


