const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan DVT4"
#define FW_VERSION      "v2.2.2 Master (Chase-added)"
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
const char* apn = "internet.beeline.ru";
const char* gprsUser = "beeline";
const char* gprsPass = "beeline";
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
const byte PulseFD = 10; //Frequency Delay 
const byte AnimFreq = 2;
//Mixed anim
bool ColCh = true;
uint32_t MixedColour = matrix.Color(255, 165, 0); //Mixed Notification Colour
//Light parameters
const uint32_t PBColour = matrix.Color(255,255,255); //Progress Bar Colour
const byte MainBrightness = 40;
byte NotiBrightness = 100;
//Chase anim
const uint8_t ChaseFD = 70;
//Wave anim
float AnimDelay = 300;
const byte MinBrightness = 20;
uint32_t NSColour = matrix.Color(255, 255, 255); //Notification Strip Colour
uint32_t NSGColour = matrix.Color(180, 255, 180); //Notification Gradient Colour 
 //Main Background Colour
uint16_t NotiColour = matrix.Color(0, 255, 0);
char CurrentColour[13];
char CurrentMixedColour[13];
bool TestMode = false;
//Technical variables
bool NotiOn = false;
bool IsSetupComplete = false;
char DeviceID[16];
uint32_t StateFreq = 1; //Minutes
bool AutoReset = true;
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
//Current mode: 0 - pulse; 1 - mixed; 2 - test; 3 - off;
//Default Animation: 0 - pulse; 1 - wave; 2 - chase (EXPERIMENTAL - OVERHEATING)
byte CurrentMode = 0;
byte DefaultAnimation = 0;
void Light(); 
void wave(uint32_t Colour);
void pulse(uint32_t Colour);
void mixedPulse();
void mixedWave();
void chase(uint32_t Colour1, uint32_t Colour2);
void chaseOff(uint32_t Colour1, uint32_t Colour2);
void chaseOn(uint32_t Colour1, uint32_t Colour2);
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
Task tNotification(AnimFreq*TASK_SECOND, TASK_FOREVER, &NotiCallback, &ts,false, &NotiOnEnable, &NotiOnDisable);
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
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass, topicEvent, 1, true, output))
    {
      Serial.println(F(" OK"));
      event["event"] = "connect";
      event["LDC"] = LDC;
      serializeJson(event, output);
      mqtt.publish(topicEvent, output, true);
      SerialMon.println(F("Published connected: "));
      SerialMon.println(F(output));
      LDC++;
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
    strcat(topicEvent, "/event");

  ProgressBar(8);
  if(modem.getSimStatus() != 1)
  {
    error(5);
  }
  int RSSI = modem.getSignalQuality();
  SerialMon.println(RSSI);
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
    if(DefaultAnimation == 2 && CurrentMode == 0)
    {
       chaseOn(NotiColour, matrix.Color(255,255,255));
    }
    else if(DefaultAnimation == 2 && CurrentMode == 1)
    {
       chaseOn(NotiColour, MixedColour);
    }
    NotiOn = false;
    NotiOn = true;
    SerialMon.println(F("Notification enabled"));
    return true;
}
void NotiOnDisable()
{
    //FadeOut(NotiColour, NotiBrightness);
    if(DefaultAnimation == 2 && CurrentMode == 0)
    {
       chaseOff(NotiColour, matrix.Color(255,255,255));
    }
    else if(DefaultAnimation == 2 && CurrentMode == 1)
    {
       chaseOff(NotiColour, MixedColour);
    }
    NotiOn = false;
}
void NotiCallback()
{
  
      switch(CurrentMode)
      {
          case 0: 
            switch(DefaultAnimation)
            {
              case 0: pulse(NotiColour); break;
              case 1: wave(NotiColour); break;
              case 2: chase(NotiColour, matrix.Color(255,255,255)); break;
            }
          break;
          case 1: 
            switch(DefaultAnimation)
            {
              case 0: mixedPulse(); break;
              case 1: mixedWave(); break;
              case 2: chase(NotiColour, MixedColour); break;
            } break;
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
        matrix.fillScreen(matrix.Color(0,255,0));
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
      char status[256];
      StaticJsonDocument<256> state;
      if(NotiOn)
      {
          switch(CurrentMode)
          {
            case 0: state["mode"] = "pulse"; break;
            case 1: state["mode"] = "mixed";  state["color2"] = CurrentMixedColour; break;
          }
      }
      else if(TestMode)
      {
        state["mode"] = "test";
      }
      else
      {
          state["mode"] = "off";
      }
      state["color"] = CurrentColour;
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
    if ((config["mode"] == "pulse") || (config["mode"] == "mixed"))
    {
      TestMode = false;
      tSignalTest.disable();
      
          //Colour change
          String CC = config["color"];
          char NewColour1[13];
          CC.toCharArray(NewColour1,13);
          uint64_t colour = config["color"];
          int r = colour / 1000000000;
          int g = (colour % 1000000000)/1000000;
          int b = (colour % 1000000)/1000;
          int a = colour % 1000;
          if (NewColour1 != CurrentColour)
          {
              tNotification.disable();
              NotiColour = matrix.Color(r,g,b);
              CC.toCharArray(CurrentColour,13);
              NotiBrightness = a;
              if(config["mode"] == "pulse")
              {
                  CurrentMode = 0;
                  tNotification.enable();
              }
          }
          if (config["mode"] == "mixed")
          {
            SerialMon.println(F("Mixed mode"));
            String CC2 = config["color2"];
            char NewColour2[13];
            CC2.toCharArray(NewColour2,13);
            uint64_t colour2 = config["color2"];
            int r2 = colour2 / 1000000000;
            int g2 = (colour2 % 1000000000)/1000000;
            int b2 = (colour2 % 1000000)/1000;
            if (NewColour2 != CurrentMixedColour)
            {
              tNotification.disable();
              CC2.toCharArray(CurrentMixedColour,13);
              MixedColour = matrix.Color(r2,g2,b2);
              tNotification.enable();
            }
            if(CurrentMode != 1)
            {
                CurrentMode = 1;
                tNotification.enable();
            }
            
          }
          //Colour change end
    }
    else if (config["mode"] == "test")
    {
      SerialMon.println(F("Test mode"));
      TestMode = true;
      tNotification.disable();
      tSignalTest.enable();
    }
     else if(config["mode"]=="write-defaults")
    {
        SerialMon.println(F("Write-defaults mode"));
        if(config["process"]=="Notification")
        { 
            if(config["-a"]=="pulse")
            {
                DefaultAnimation = 0;
            }
            else if(config["-a"]=="wave")
            {
                DefaultAnimation = 1;
            }
            else if(config["-a"]=="chase")
            {
                DefaultAnimation = 2;
            }
        }
        if(config["process"]=="State")
        {
            StateFreq = config["-t"];
        }
        if(config["process"]=="System")
        {
            if(config["AutoReset"]=="true")
            {
                AutoReset = true;
            }
            else if(config["AutoReset"]=="false")
            { 
                AutoReset = false;
            }
        }
        
    }
    if(config["mode"]=="off")
    {
        SerialMon.println(F("Notification off"));
        TestMode = false;
        tSignalTest.disable();
        tNotification.disable();
    }
    PublishState();
}
void error(int errcode)
{
    Serial.println("Error " + errcode);
    delay(1000);
    int i = 0;
    while(i < 5)
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
      if (AutoReset)
      {
        i++;
      }
    }
    NVIC_SystemReset();
}
void ProgressBar (int pb)
{
  matrix.setBrightness(40);
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
    matrix.fillScreen(fadecolor);
    matrix.setBrightness(j);
    matrix.show();
    delay(PulseFD);
  }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  //matrix.setBrightness(brightness);
  for (byte k = brightness; k > 0; k--)
  {
    matrix.fillScreen(fadecolour);
    matrix.setBrightness(k);
    matrix.show();
    delay(PulseFD);
  }
  matrix.fillScreen(matrix.Color(0, 0, 0));
  matrix.show();
}
void Light()
{
    if(tNotification.isFirstIteration())
    {
      matrix.setBrightness(NotiBrightness);
      matrix.fillScreen(NotiColour);
      matrix.show();
    }
    else
    {
      return;
    }
}
void pulse(uint32_t Colour)
{
    Fadein(Colour, NotiBrightness);
    delay(AnimDelay);
    FadeOut(Colour, NotiBrightness);
}
void wave(uint32_t Colour)
{
  float brc = NotiBrightness / 8;
  for (byte i = 0; i <= 17; i++)
  {
    matrix.fillScreen(Colour);
    matrix.drawLine(i, 0, i, 16, NSColour);
    matrix.drawLine(i + 1, 0, i + 1, 16, NSGColour);
    matrix.drawLine(i - 1, 0, i - 1, 16, NSGColour);
    matrix.show();
    if (i <= 8)
    {
      //delay(delays-(i*delayk));
      matrix.setBrightness(i+(i * brc));
      delay(40);
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      
      matrix.setBrightness(NotiBrightness - ((i - 8)*brc));
      delay(40);
    }
  }
  matrix.setBrightness(0);
  matrix.show();
}
void mixedPulse()
{
  switch (ColCh)
  {
     case true: pulse(NotiColour); break;
     case false: pulse(MixedColour); break;
  }
  ColCh = !ColCh;
}
void mixedWave()
{
  switch (ColCh)
  {
     case true: wave(NotiColour); break;
     case false: wave(MixedColour); break;
  }
  ColCh = !ColCh;
}
void chase(uint32_t Colour1, uint32_t Colour2)
{
      matrix.setBrightness(NotiBrightness);
      
      for(int i = 0; i < 16; i++)
      {
      matrix.fillScreen(Colour2);

      matrix.drawLine(i-24,0,i-16,16,Colour1);
      matrix.drawLine(i-23,0,i-15,15,Colour1);
      matrix.drawLine(i-22,0,i-14,15,Colour1);
      matrix.drawLine(i-21,0,i-13,15,Colour1);

      matrix.drawLine(i-16 ,0,i-8,15,Colour1);
      matrix.drawLine(i-15,0,i-7,15,Colour1);
      matrix.drawLine(i-14,0,i-6,15,Colour1);
      matrix.drawLine(i-13,0,i-5,15,Colour1);

      matrix.drawLine(i-8,0,i,15,Colour1);
      matrix.drawLine(i-7,0,i+1,15,Colour1);
      matrix.drawLine(i-6,0,i+2,15,Colour1);
      matrix.drawLine(i-5,0,i+3,15,Colour1);


      matrix.drawLine(i ,0,i+8,15,Colour1);
      matrix.drawLine(i+ 1,0,i+9,15,Colour1);
      matrix.drawLine(i+ 2,0,i+10,15,Colour1);
      matrix.drawLine(i + 3,0,i+11,15,Colour1);

 

      matrix.drawLine(i+8,0,i+16,15,Colour1);
      matrix.drawLine(i+9,0,i+17,15,Colour1);
      matrix.drawLine(i+10,0,i+18,15,Colour1);
      matrix.drawLine(i+11,0,i+19,15,Colour1);


      matrix.show();
      delay(ChaseFD);
      }
      SerialMon.println("Chase!");
}
void chaseOff(uint32_t Colour1, uint32_t Colour2)
{
      int CurrentBr = NotiBrightness;
      matrix.setBrightness(NotiBrightness);
      int brc = NotiBrightness / 16;
      for(int i = 0; i < 16; i++)
      {
      matrix.fillScreen(Colour2);

      matrix.drawLine(i-24,0,i-16,16,Colour1);
      matrix.drawLine(i-23,0,i-15,15,Colour1);
      matrix.drawLine(i-22,0,i-14,15,Colour1);
      matrix.drawLine(i-21,0,i-13,15,Colour1);

      matrix.drawLine(i-16 ,0,i-8,15,Colour1);
      matrix.drawLine(i-15,0,i-7,15,Colour1);
      matrix.drawLine(i-14,0,i-6,15,Colour1);
      matrix.drawLine(i-13,0,i-5,15,Colour1);

      matrix.drawLine(i-8,0,i,15,Colour1);
      matrix.drawLine(i-7,0,i+1,15,Colour1);
      matrix.drawLine(i-6,0,i+2,15,Colour1);
      matrix.drawLine(i-5,0,i+3,15,Colour1);


      matrix.drawLine(i ,0,i+8,15,Colour1);
      matrix.drawLine(i+ 1,0,i+9,15,Colour1);
      matrix.drawLine(i+ 2,0,i+10,15,Colour1);
      matrix.drawLine(i + 3,0,i+11,15,Colour1);

 

      matrix.drawLine(i+8,0,i+16,15,Colour1);
      matrix.drawLine(i+9,0,i+17,15,Colour1);
      matrix.drawLine(i+10,0,i+18,15,Colour1);
      matrix.drawLine(i+11,0,i+19,15,Colour1);
      matrix.setBrightness(CurrentBr);
      matrix.show();
      delay(ChaseFD);
      CurrentBr=CurrentBr-brc;
      }
      matrix.setBrightness(0);
      matrix.show();
      SerialMon.println("Chase! off");
}
void chaseOn(uint32_t Colour1, uint32_t Colour2)
{
      matrix.setBrightness(0);
      matrix.show();
      int brc = NotiBrightness / 16;
      int CurrentBr = 0;
      SerialMon.println("Chase! on");
      for(int i = 0; i < 16; i++)
      {
      matrix.fillScreen(Colour2);

      matrix.drawLine(i-24,0,i-16,16,Colour1);
      matrix.drawLine(i-23,0,i-15,15,Colour1);
      matrix.drawLine(i-22,0,i-14,15,Colour1);
      matrix.drawLine(i-21,0,i-13,15,Colour1);

      matrix.drawLine(i-16 ,0,i-8,15,Colour1);
      matrix.drawLine(i-15,0,i-7,15,Colour1);
      matrix.drawLine(i-14,0,i-6,15,Colour1);
      matrix.drawLine(i-13,0,i-5,15,Colour1);

      matrix.drawLine(i-8,0,i,15,Colour1);
      matrix.drawLine(i-7,0,i+1,15,Colour1);
      matrix.drawLine(i-6,0,i+2,15,Colour1);
      matrix.drawLine(i-5,0,i+3,15,Colour1);


      matrix.drawLine(i ,0,i+8,15,Colour1);
      matrix.drawLine(i+ 1,0,i+9,15,Colour1);
      matrix.drawLine(i+ 2,0,i+10,15,Colour1);
      matrix.drawLine(i + 3,0,i+11,15,Colour1);

 

      matrix.drawLine(i+8,0,i+16,15,Colour1);
      matrix.drawLine(i+9,0,i+17,15,Colour1);
      matrix.drawLine(i+10,0,i+18,15,Colour1);
      matrix.drawLine(i+11,0,i+19,15,Colour1);
      matrix.setBrightness(CurrentBr);
      matrix.show();
      delay(ChaseFD);
      CurrentBr=CurrentBr+brc;
      }
}
