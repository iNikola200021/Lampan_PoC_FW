const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan DVT4"
#define FW_VERSION      "v2.2.1 alpha 1"
#define TINY_GSM_MODEM_SIM800
#define ARDUINOJSON_USE_LONG_LONG 1

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoMatrix.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <Adafruit_GFX.h>
#define MATRIX_PIN PA7
#define SerialMon Serial
#define SerialAT Serial1

//Classes definition
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, MATRIX_PIN,
                            NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

Scheduler ts;

//APN Credentials
const char* apn = "mts";
const char* gprsUser = "mts";
const char* gprsPass = "mts";
// MQTT details
const char* broker = "mqtt.iotcreative.ru";
const char* mqtt_user = "dave";
const char* mqtt_pass = "lemontree";
int port = 1884;
char topicState[33]= "$device/";
char topicCmd[32]= "$device/";
char topicEvent[45]= "$device/";
int LDC = 0;
//Pulse anim
const byte PulseDelay = 20;
//Mixed anim
bool ColCh = true;
uint32_t MixedColour = matrix.Color(255, 165, 0); //Mixed Notification Colour
//Light parameters
const uint32_t PBColour = matrix.Color(255,255,255); //Progress Bar Colour
const byte MainBrightness = 40;
byte NotiBrightness = 100;
//Wave anim
float WaveDelay = 2000;
const byte MinBrightness = 20;
uint32_t NSColour = matrix.Color(255, 255, 255); //Notification Strip Colour
uint32_t NSGColour = matrix.Color(180, 255, 180); //Notification Gradient Colour 
 //Main Background Colour
uint16_t NotiColour = matrix.Color(0, 255, 0);
uint64_t sendR = 0;
uint32_t sendG = 0;
int sendB = 0;
bool TestMode = false;
//Technical variables
bool NotiOn = false;
bool IsSetupComplete = false;
char DeviceID[16];
uint32_t StateFreq = 1; //Minutes
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
//Animation: 0 - pulse; 1 - mixed
byte pattern = 0;
void Light(); 
void wave();
void pulse();
void mixed();
//Service functions
void ProgressBar (int pb);
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
Task tState(StateFreq*TASK_MINUTE, TASK_FOREVER, &PublishState, &ts);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts, false, &SignalTestEn, &SignalTestDis);

bool mqttConnect()
{
  char output[48];
  StaticJsonDocument<48> event;
  event["event"] = "disconnect";
  serializeJson(event, output);
  while (!mqtt.connected())
  {
    SerialMon.print(F("MQTT?"));
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass, topicEvent, 1, false, output))
    {
      Serial.println(F(" OK"));
      event["event"] = "connect";
      event["LDC"] = LDC;
      serializeJson(event, output);
      mqtt.publish(topicEvent, output);
      LDC = 1;
      PublishState();
      mqtt.subscribe(topicCmd,1);
      if (!IsSetupComplete) //Add Boot is Complete Animation
      {
        IsSetupComplete = true;
        ProgressBar(16);
        for (byte k = MainBrightness; k > 0; k--)
        {
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
  JSONApply(payload, len);
}
void setup()
{
  SerialMon.begin();
  
  matrix.begin();
  ProgressBar(2);
  matrix.show();
  SerialAT.begin(115200);
  ProgressBar(4);
  delay(6000);

  SerialMon.print("Firmware ");
  SerialMon.print(FW_NAME);  
  SerialMon.print(", version ");
  SerialMon.print(FW_VERSION);
  SerialMon.print(",build ");  
  SerialMon.println(BUILD);
  ProgressBar(6);
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
    strcat(topicCmd, DeviceID);
    strcat(topicState, "/state");
    strcat(topicCmd, "/command");
    strcat(topicEvent, DeviceID);
    strcat(topicEvent, "/event/disconnect");

  ProgressBar(8);
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
    ProgressBar(10);
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
    ProgressBar(12);
  }
  ts.addTask(tNotification);
  ts.addTask(tState);
  // MQTT Broker setup
  SerialMon.println(F("SETUP"));
  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttRX);
  ProgressBar(14);
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
    //Fadein(NotiColour, NotiBrightness);
    NotiOn = true;
    SerialMon.println(F("Notification enabled"));
    return true;
}
void NotiOnDisable()
{
    //FadeOut(NotiColour, NotiBrightness);
    NotiOn = false;
}
void NotiCallback()
{
      switch(pattern)
      {
          case 0: pulse(); break;
          case 1: mixed(); break;
      }
}
//Signal Test Task
void SignalTest ()
{
    int RSSI = -113 +(modem.getSignalQuality()*2);
    if (RSSI <=  -110)
    {
        matrix.fillScreen(matrix.Color(139,0,0));
        matrix.show();
    }
    else if (RSSI >  -110 && RSSI < -100)
    {
        matrix.fillScreen(matrix.Color(220,20,60));
        matrix.show();
    }
    else if (RSSI >=  -100 && RSSI < -85)
    {
        matrix.fillScreen(matrix.Color(255,165,0));
        matrix.show();
    }
    else if (RSSI >=  -85 && RSSI < -70)
    {
        matrix.fillScreen(matrix.Color(255,255,0));
        matrix.show();
    }
    else if (RSSI >=  -70)
    {
        matrix.fillScreen(matrix.Color(0,0,255));
        matrix.show();    
        }
}
bool SignalTestEn()
{
  for(int i = 0; i<3; i++)
  {
    matrix.setBrightness(MainBrightness);
    matrix.fillScreen(matrix.Color(255,165,0));
    matrix.show();
    delay(500);
    matrix.fillScreen(matrix.Color(0,0,0));
    matrix.show();
    delay(500);
  }
  SerialMon.println(F("SignalTest has been started"));
  return true;
}
void SignalTestDis()
{
  for(int i = 0; i<3; i++)
  {
    matrix.setBrightness(MainBrightness);
    matrix.fillScreen(matrix.Color(255,165,0));
    matrix.show();
    delay(500);
    matrix.fillScreen(matrix.Color(0,0,0));
    matrix.show();
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
    if (config["mode"] == "pulse")
    {
      TestMode = false;
      tSignalTest.disable();

          //Colour change
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
          uint32_t NewColour = matrix.Color(r,g,b);
          byte NewBr = a;
          if (NotiColour != NewColour || NotiBrightness != NewBr)
          {
            tNotification.disable();
            NotiColour = matrix.Color(r,g,b);
            sendR = r;
            sendG = g;
            sendB = b;
            NotiBrightness = a;
            PublishState();
            tNotification.enable();
          }
          pattern = 0;
          //Colour change end
    }
    else if (config["mode"] == "mixed")
    {
      TestMode = false;
      tSignalTest.disable();

          //Colour change
          uint64_t colour1 = config["color"];
          uint64_t colour2 = config["colour2"];
          int r1 = colour1 / 1000000000;
          int g1 = (colour1 % 1000000000)/1000000;
          int b1 = (colour1 % 1000000)/1000;
          int a1 = colour1 % 1000;
          int r2 = colour2 / 1000000000;
          int g2 = (colour2 % 1000000000)/1000000;
          int b2 = (colour2 % 1000000)/1000;
          int a2 = colour2 % 1000;
          SerialMon.print(F("Got colour (RGBA): "));
          SerialMon.print(r1);
          SerialMon.print(F(", "));
          SerialMon.print(g1);
          SerialMon.print(F(", "));
          SerialMon.print(b1);
          SerialMon.print(F(", "));
          SerialMon.println(a1);
          SerialMon.print(F("Got colour 2 (RGBA): "));
          SerialMon.print(r2);
          SerialMon.print(F(", "));
          SerialMon.print(g2);
          SerialMon.print(F(", "));
          SerialMon.print(b2);
          SerialMon.print(F(", "));
          SerialMon.println(a2);
          NotiColour = matrix.Color(r1,g1,b1);
          NotiBrightness = a1;
          MixedColour = matrix.Color(r2,g2,b2);
          pattern = 1;
          //Colour change end
    }
    else if (config["mode"] == "test")
    {
      SerialMon.println(F("Config: Test mode"));
      TestMode = true;
      tNotification.disable();
      tSignalTest.enable();
    }
    if(config["mode"]=="off")
    {
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
      matrix.fillScreen(matrix.Color(255,0,0));
      
      matrix.show();
      delay(2000);
      switch (errcode)
      {
        case 1: //Blank IMEI -> No connection with the modem
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(8,0,8,16,matrix.Color(255,0,0));
          matrix.show();
          delay(2000);
          break;

        case 2: //No NET
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(7,0,7,16, matrix.Color(255,0,0));
          matrix.drawLine(9,0,9,16,matrix.Color(0,255,0));
          matrix.show();
          delay(2000);
          break;

        case 3: //No GPRS
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(6,0,6,16, matrix.Color(255,0,0));
          matrix.drawLine(8,0,8,16,matrix.Color(0,255,0));
          matrix.drawLine(10,0,10,16,matrix.Color(0,0,255));
          matrix.show();
          delay(2000);
          break;

        case 4: //No MQTT
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(5,0,5,16, matrix.Color(255,0,0));
          matrix.drawLine(7,0,7,16, matrix.Color(0,255,0));
          matrix.drawLine(9,0,9,16,matrix.Color(255,255,255));
          matrix.drawLine(11,0,11,16,matrix.Color(0,0,255));
          matrix.show();
          delay(2000);
          break;
          
        case 5: //No SIM
          matrix.fillScreen(matrix.Color(0,0,0));
          matrix.drawLine(4,0,4,16, matrix.Color(255,0,0));
          matrix.drawLine(6,0,6,16, matrix.Color(0,255,0));
          matrix.drawLine(8,0,8,16,matrix.Color(255,255,255));
          matrix.drawLine(10,0,10,16,matrix.Color(0,0,255));
          matrix.drawLine(12,0,12,16, matrix.Color(255,0,0));
          matrix.show();
          delay(2000);
          break;
      }
    }
}
void ProgressBar (int pb)
{
  matrix.fillRect(0,0,pb,16,matrix.Color(255,255,255));
  matrix.show();
}
//Animations
void Fadein(uint32_t fadecolor, int brightness)
{
  matrix.setBrightness(0);
  matrix.fillScreen(fadecolor);
  matrix.show();
  for (byte j = 0; j < brightness; j++)
  {
    matrix.setBrightness(j);
    delay(1);
    matrix.fillScreen(fadecolor);
    matrix.show();
  }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  //matrix.setBrightness(brightness);
  for (byte k = brightness; k > 0; k-=2)
  {
    matrix.setBrightness(k);
    delay(1);
  }
  matrix.fillScreen(matrix.Color(0, 0, 0));
  matrix.show();
}
void Light()
{
  if(!NotiOn)
  {
      NotiOn = true;
      matrix.setBrightness(NotiBrightness);
      matrix.fillScreen(NotiColour);
      matrix.show();
  }
  else
  {
    return;
  }
}
void pulse()
{
  for (byte i = 0; i <= NotiBrightness; i++)
  {
    matrix.fillScreen(NotiColour);
    if (i < (NotiBrightness/2))
    {
      matrix.setBrightness(i);
      matrix.show();
      delay(PulseDelay);
    }
    else if (i == (NotiBrightness/2))
    {
      matrix.setBrightness(i);
      matrix.show();
      delay(WaveDelay);
    }
    else
    {
      matrix.setBrightness(NotiBrightness - i);
      matrix.show();
      delay(PulseDelay);
    }
  }
  delay(WaveDelay);
}
void wave()
{
  float brc = (NotiBrightness - MinBrightness) / 8;
  for (byte i = 0; i <= 17; i++)
  {
    matrix.fillScreen(NotiColour);
    matrix.drawLine(i, 0, i, 16, NSColour);
    matrix.drawLine(i + 1, 0, i + 1, 16, NSGColour);
    matrix.drawLine(i - 1, 0, i - 1, 16, NSGColour);
    matrix.show();
    if (i <= 8)
    {
      //delay(delays-(i*delayk));
      matrix.setBrightness(MinBrightness+ (i * brc));
      delay(PulseDelay);
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      matrix.setBrightness(NotiBrightness - (i - 8)*brc);
      delay(PulseDelay);
    }
  }
}
void mixed()
{
  if(ColCh)
  { 
      matrix.fillScreen(NotiColour);
        
  }
  else
  { 
      matrix.fillScreen(MixedColour);
  }
  ColCh = !ColCh;
  for (byte i = 0; i <= NotiBrightness; i++)
  {
    
    if (i < (NotiBrightness/2))
    {
      matrix.setBrightness(i);
      matrix.show();
      delay(PulseDelay);
    }
    else if (i == (NotiBrightness/2))
    {
      matrix.setBrightness(i);
      matrix.show();
      delay(WaveDelay);
    }
    else
    {
      matrix.setBrightness(NotiBrightness - i);
      matrix.show();
      delay(PulseDelay);
    }
  }
  delay(WaveDelay);
}

