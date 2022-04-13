const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan DVT4"
#define FW_VERSION      "v1.0.0-release"
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
#define MATRIX_PIN A7
#define SerialMon Serial
#define SerialAT Serial1

//Classes definition
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, MATRIX_PIN,
                            NEO_MATRIX_BOTTOM    + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

Scheduler ts;

//APN Credentials
const char* apn = "internet.mts.ru";
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
uint16_t PulseFD = 10; //Delay time between frames. Milliseconds
uint16_t PeakFreeze = 1000; //Delay time in peaks of pulse. Milliseconds
uint16_t ZeroFreeze = 1000; //Delay time in zero brightness of pulse. Milliseconds
uint16_t FN = 0;
//Mixed anim
bool ColCh = false;
//ProgressBar parameters
const uint32_t PBColour = matrix.Color(255,255,255); //Progress Bar Colour
const byte PBBrightness = 40;
//Notification params
byte NotiBrightness = 100;
//Wave anim
float AnimDelay = 300;
const byte MinBrightness = 20;
uint32_t NSColour = matrix.Color(255, 255, 255); //Notification Strip Colour
uint32_t NSGColour = matrix.Color(180, 255, 180); //Notification Gradient Colour 
//Colours
uint16_t MainColour = matrix.Color(0, 255, 0);
uint32_t MixedColour = matrix.Color(255, 165, 0); //Mixed Notification Colour
char CurrentColour[13];
char CurrentMixedColour[13];
bool TestMode = false;
//Technical variables
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
//FUNCTIONS DECLARATION
//MQTT Functions
bool mqttConnect();
void mqttRX(char* topic, byte* payload, unsigned int len);
//Light Functions
void Fadein(uint32_t fadecolor, int brightness);
void FadeOut(uint32_t fadecolour, int brightness);
//Notification Functions
bool PulseOn(); 
void NotiCallback();
void NotiOnDisable();
//Current mode: 0 - off; 1 - pulse; 2 - mixed; 3 - test;
//Default Animation: 0 - pulse; 1 - wave;
byte CurrentMode = 0;
byte DefaultAnimation = 0;
void wave(uint32_t Colour);
void pulse(uint32_t Colour);
void mixedPulse();
void mixedWave();
//Service functions
void ProgressBar (int pb);
void error(int errcode);
void PublishState();
void SignalTest();
bool SignalTestEn();
void SignalTestDis();
void FactoryReset();

//TASKS
//Task 1 - Notification
//Task 2 - Publish State
//Task 3 - Signal Test
Task tNotification(TASK_IMMEDIATE, 0, &NotiCallback, &ts,false, &PulseOn, &NotiOnDisable);
Task tState(StateFreq*TASK_MINUTE, TASK_FOREVER, &PublishState, &ts, true);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts, false, &SignalTestEn, &SignalTestDis);
//Task tMQTT(TASK_IMMEDIATE, TASK_FOREVER, &MqttCheck, &ts);

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
        tState.enable();
        FadeOut(PBColour, PBBrightness);
        SerialMon.println(F("Boot complete"));
      }
    }
    else
    {
      if(MqttRT < MqttThreshold)
      {
          SerialMon.print(F(" NOT OK, status: "));
          SerialMon.print(mqtt.state());
          MqttRT++;
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
          if(CurrentMode == 3)
          {
              tSignalTest.disable();
          }
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
              MainColour = matrix.Color(r,g,b);
              CC.toCharArray(CurrentColour,13);
              NotiBrightness = a;
              tNotification.setIterations((NotiBrightness*2)+1);
              if(config["mode"] == "pulse")
              {
                  CurrentMode = 1;
                  ColCh = false;
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
            if(CurrentMode != 2)
            {
                CurrentMode = 2;
                SerialMon.print(CurrentMode);
                tNotification.enable();
            }
            
          }
          //Colour change end
    }
    else if (config["mode"] == "test")
    {
      CurrentMode = 3;
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
        if(config["process"]=="Pulse")
        {
            if(config["-FD"] != "") // FrameDelay
            {
              PulseFD = config["-FD"];
            }
            if(config["-PF"] != "") // Peak Freeze
            {
              PeakFreeze = config["-PF"];
            }
            if(config["-ZF"] != "") //Zero Freeze
            {
              ZeroFreeze = config["-ZF"]
            }
        }
    }
    if(config["mode"]=="off")
    {
        CurrentMode = 0;
        tSignalTest.disable();
        tNotification.disable();
    }
    PublishState();
}
void setup()
{
  SerialMon.begin();
  matrix.begin();
  ProgressBar(2);
  SerialAT.begin(115200);
  delay(6000);
  SerialMon.print("Firmware ");
  SerialMon.print(FW_NAME);  
  SerialMon.print(", version ");
  SerialMon.print(FW_VERSION);
  SerialMon.print(",build ");  
  SerialMon.println(BUILD);
  ProgressBar(4);
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
  ProgressBar(6);
  if(modem.getSimStatus() != 1)
  {
    error(5);
  }
  ProgressBar(8);
  SerialMon.print("NET? ");
  if (!modem.waitForNetwork())
  {
    SerialMon.println(F("!OK"));
    error(2);
  }
  if (modem.isNetworkConnected())
  {
    SerialMon.println("OK");
    ProgressBar(10);
  }
  SerialMon.print("GPRS? ");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    SerialMon.println(F("!OK"));
    error(2); 
  }
  if (modem.isGprsConnected()) 
  {
    SerialMon.println(F("OK"));
    ProgressBar(12);
  }
  ts.addTask(tNotification);
  ts.addTask(tState);
  ts.addTask(tSignalTest);
  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttRX);
  ProgressBar(14);
  mqttConnect();
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
void NotiOnDisable()
{
    switch (ColCh)
    {
       case false: FadeOut(MainColour, matrix.getBrightness()); ColCh = !ColCh; break;
       case true: FadeOut(MixedColour, matrix.getBrightness()); ColCh = !ColCh; break;
    }
}
void NotiCallback()
{
      switch(CurrentMode)
      {
          case 1: 
            switch(DefaultAnimation)
            {
              case 0: pulse(MainColour); break;
              case 1: wave(MainColour); break;
            }
          break;
          case 2: 
            switch(DefaultAnimation)
            {
              case 0: mixedPulse(); break;
              case 1: mixedWave(); break;
            } 
            break;
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
    matrix.setBrightness(PBBrightness);
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
    matrix.setBrightness(PBBrightness);
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
      if(tNotification.isEnabled())
      {
          switch(CurrentMode)
          {
            case 0: state["mode"] = "off"; break;
            case 1: state["mode"] = "pulse"; state["color"] = CurrentColour; break;
            case 2: state["mode"] = "mixed";  state["color"] = CurrentColour; state["color2"] = CurrentMixedColour; break;
            case 3: state["mode"] = "test"; break;
          }
      }
      state["RSSI"] = -113 +(modem.getSignalQuality()*2);
      serializeJson(state, status);
      mqtt.publish(topicState, status);
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
  matrix.setBrightness(PBBrightness);
  matrix.fillRect(0,0,pb,16,PBColour);
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
  for (byte k = brightness; k > 0; k--)
  {
    matrix.fillScreen(fadecolour);
    matrix.setBrightness(k);
    matrix.show();
  }
  matrix.fillScreen(matrix.Color(0, 0, 0));
  matrix.show();
}
void pulse(uint32_t Colour)
{
  if(tNotification.getRunCounter() < NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PulseFD);
  }
  else if(tNotification.getRunCounter() == NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PeakFreeze);
  }
  else if(tNotification.getRunCounter() > NotiBrightness)
  {
    matrix.setBrightness((NotiBrightness*2)-FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PulseFD);
  }
  FN++;
  if(tNotification.getIterations() == 0)
  {
    if(CurrentMode == 2)
    {
        ColCh = !ColCh;
    }
    tNotification.restartDelayed(PeakFreeze);
  }
    //FadeOut(Colour, NotiBrightness);
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
      tNotification.delay(40);
    }
    else
    {
      // delay(delays-(8*delayk)+(i*delayk));
      
      matrix.setBrightness(NotiBrightness - ((i - 8)*brc));
      tNotification.delay(40);
    }
  }
  matrix.setBrightness(0);
  matrix.show();
}
void mixedPulse()
{
  switch (ColCh)
  {
     case false: pulse(MainColour); break;
     case true: pulse(MixedColour); break;
  }
}
void mixedWave()
{
  switch (ColCh)
  {
     case false: wave(MainColour); break;
     case true: wave(MixedColour); break;
  }
}
bool PulseOn()
{
    FN = 0;
    return true;
}
