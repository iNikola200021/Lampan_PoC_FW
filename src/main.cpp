const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan DVT4"
#define FW_VERSION      "v1.2-EEPROM"
#define TINY_GSM_MODEM_SIM800
#define ARDUINOJSON_USE_LONG_LONG 1
#define USING_FLASH_SECTOR_NUMBER           (REGISTERED_NUMBER_FLASH_SECTORS - 2)
#include <FlashStorage_STM32F1.h>
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
#define Terminal Serial
#define SerialModem Serial1

const int WRITTEN_SIGNATURE = 0x49544352;
const int FSYS_SIGNATURE = 0x4C534E53;
const int NVRAMAddr = sizeof(WRITTEN_SIGNATURE) + 16;
const int FsysAddr = sizeof(WRITTEN_SIGNATURE);
//Classes definition
TinyGsm modem(SerialModem);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, MATRIX_PIN,
                            NEO_MATRIX_BOTTOM    + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

Scheduler ts;

// EEPROM ADRESSING
// |           |               Fsys Block               |                         NVRAM                        |
// |     0     |FsysAddr                                |NVRAMAddr                                             | 
// |0 |1 |2 |3 |0|1|2|3|4|5|6|7|8|9|10| 11  |12|13|14|15|   0   |   1  |   2   | 3  | 4 | 5 | 6 | 7 |    8     |
// | SIGNATURE |      SerialNumber    |UseSN| FsysSign  |AutoRst|SysStF|DftAnim|PuFD|PulsePF|PulseZF|ColourCorr|

struct Fsys
{
    char SerialNumber [11];
    bool UseSN; 
    int FsysSign;
};
struct Settings
{
    bool AutoReset; 
    uint8_t SysStateFreq;
    uint8_t DefaultAnim;
    uint8_t PulseFrameDelay; 
    uint16_t PulsePeakFreeze; 
    uint16_t PulseZeroFreeze; 
    uint8_t ColourCorr;
};
Settings CurrentSettings;
Fsys FsysBlock;

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
//Pulse anim //Delay time in peaks of pulse. Milliseconds
//Delay time in zero brightness of pulse. Milliseconds
uint16_t FN;
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
uint32_t MainColour = matrix.Color(0, 255, 0);
uint32_t MixedColour = matrix.Color(255, 165, 0); //Mixed Notification Colour
char CurrentColour[13];
char CurrentMixedColour[13];
bool TestMode = false;
//Technical variables
bool IsBooted = false;
char DeviceID[16];
//Error variables
byte MqttRT = 0;
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
void wave(uint32_t Colour);
void pulse(uint32_t Colour, Settings *set);
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
void ApplySettingsEvent();
void AnimationChange();
void getEEPROMData();
uint32_t Colour32(uint8_t r, uint8_t g, uint8_t b);
//TASKS
//Task 1 - Notification
//Task 2 - Publish State
//Task 3 - Signal Test
Task tNotification(TASK_IMMEDIATE, 0, &NotiCallback, &ts,false, &PulseOn, &NotiOnDisable);
Task tState(TASK_IMMEDIATE, TASK_FOREVER, &PublishState, &ts, false);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts, false, &SignalTestEn, &SignalTestDis);


bool mqttConnect()
{
  char output[48];
  StaticJsonDocument<48> event;
  event["event"] = "disconnect";
  serializeJson(event, output);
  while (!mqtt.connected())
  {
    Terminal.print(F("MQTT?"));
    if (mqtt.connect(DeviceID, mqtt_user, mqtt_pass, topicEvent, 1, true, output))
    {
      Terminal.println(F(" OK"));
      event["event"] = "connect";
      event["LDC"] = LDC;
      serializeJson(event, output);
      mqtt.publish(topicEvent, output, true);
      Terminal.print(F("Published connected: "));
      Terminal.println(output);
      LDC++;
      PublishState();
      mqtt.subscribe(topicCmd,1);
      if (!IsBooted) //Add Boot is Complete Animation
      {
        IsBooted = true;
        ProgressBar(16);
        tState.enable();
        FadeOut(PBColour, PBBrightness);
        Terminal.println(F("Boot complete"));
      }
    }
    else
    {
      if(MqttRT < MqttThreshold)
      {
          Terminal.print(F(" !OK, status: "));
          Terminal.println(mqtt.state());
          MqttRT++;
      }
      if(MqttRT == MqttThreshold)
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
       Terminal.println(F("Not JSON or deserialize error"));
       return;
    }
    if ((config["mode"] == "pulse") || (config["mode"] == "mixed"))
    {
          if(CurrentMode == 3)
          {
              tSignalTest.disable();
          }
          //Colour change
          const char* NewColour1 = config["color"];
          uint64_t colour = config["color"];
          int r = colour / 1000000000;
          int g = (colour % 1000000000)/1000000;
          int b = (colour % 1000000)/1000;
          int a = colour % 1000;
          if (NewColour1 != CurrentColour)
          {
              tNotification.disable();
              if(CurrentSettings.ColourCorr == 1 || CurrentSettings.ColourCorr == 2)
              {
                MainColour = Colour32(r,g,b);
              }
              else
              {
                MainColour = matrix.Color(r,g,b);
              }
              strncpy(CurrentColour, NewColour1, strlen(NewColour1));
              NotiBrightness = a;
              AnimationChange();
              if(config["mode"] == "pulse")
              {
                  CurrentMode = 1;
                  ColCh = false;
                  tNotification.enable();
              }
          }
          if (config["mode"] == "mixed")
          {
            const char* NewColour2 = config["color2"];
            uint64_t colour2 = config["color2"];
            int r2 = colour2 / 1000000000;
            int g2 = (colour2 % 1000000000)/1000000;
            int b2 = (colour2 % 1000000)/1000;
            if (NewColour2 != CurrentMixedColour)
            {
              tNotification.disable();
              strncpy(CurrentMixedColour, NewColour2, strlen(NewColour2));
              if(CurrentSettings.ColourCorr == 1 || CurrentSettings.ColourCorr == 2)
              {
                MixedColour = Colour32(r2,g2,b2);
              }
              else
              {
                MixedColour = matrix.Color(r2,g2,b2);
              }
              tNotification.enable();
            }
            if(CurrentMode != 2)
            {
                CurrentMode = 2;
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
        bool SettingsChanged = false;
        if(config.containsKey("AutoReset") && CurrentSettings.AutoReset != config["AutoReset"])
        {
            CurrentSettings.AutoReset = (bool)config["AutoReset"];
            EEPROM.update(NVRAMAddr, CurrentSettings.AutoReset);
            SettingsChanged = true;
        }
        if(config.containsKey("StateFrequency") && CurrentSettings.SysStateFreq != config["StateFrequency"])
        {
            CurrentSettings.SysStateFreq = (uint8_t)config["StateFrequency"];
            tState.setInterval(CurrentSettings.SysStateFreq*TASK_MINUTE);
            EEPROM.update(NVRAMAddr + 1, CurrentSettings.SysStateFreq);
            SettingsChanged = true;
        }
        if(config.containsKey("DefaultAnimation"))
        {
            tNotification.disable();
            if(config["DefaultAnimation"] == "pulse")
            {
               CurrentSettings.DefaultAnim = 0;
               EEPROM.update(NVRAMAddr + 2, CurrentSettings.DefaultAnim);
               SettingsChanged = true;
            }
            else if(config["DefaultAnimation"] == "wave")
            {
               CurrentSettings.DefaultAnim = 1;
               EEPROM.update(NVRAMAddr + 2, CurrentSettings.DefaultAnim);
               SettingsChanged = true;
            }
            AnimationChange();
            tNotification.enable();
        }
        if(config.containsKey("PulseFD") && CurrentSettings.PulseFrameDelay != config["PulseFD"]) // FrameDelay
        {
          uint8_t FD = config["PulseFD"];
          Terminal.println(FD);
          CurrentSettings.PulseFrameDelay = FD;
          EEPROM.update(NVRAMAddr + 3, CurrentSettings.PulseFrameDelay);
          SettingsChanged = true;
        }
        if(config.containsKey("PulsePF") && CurrentSettings.PulsePeakFreeze != config["PulsePF"]) // Peak Freeze
        {
          CurrentSettings.PulsePeakFreeze = (uint16_t)config["PulsePF"];
          uint8_t HiB = highByte(CurrentSettings.PulsePeakFreeze);
          uint8_t LoB = lowByte(CurrentSettings.PulsePeakFreeze);
          EEPROM.update(NVRAMAddr + 4, HiB);
          EEPROM.update(NVRAMAddr + 5, LoB);
          SettingsChanged = true;
        }
        if(config.containsKey("PulseZF") && CurrentSettings.PulseZeroFreeze != config["PulseZF"]) //Zero Freeze
        {
          CurrentSettings.PulseZeroFreeze = (uint16_t)config["PulseZF"];
          uint8_t HiB = highByte(CurrentSettings.PulseZeroFreeze);
          uint8_t LoB = lowByte(CurrentSettings.PulseZeroFreeze);
          EEPROM.update(NVRAMAddr + 6, HiB);
          EEPROM.update(NVRAMAddr + 7, LoB);
          SettingsChanged = true;
        }
        if(config.containsKey("ColourCorrection"))
        {
            uint64_t colour = MainColour;
            int r = colour / 1000000000;
            int g = (colour % 1000000000)/1000000;
            int b = (colour % 1000000)/1000;
            int a = colour % 1000;
            uint64_t colour2 = MixedColour;
            int r2 = colour2 / 1000000000;
            int g2 = (colour2 % 1000000000)/1000000;
            int b2 = (colour2 % 1000000)/1000;
            tNotification.disable();
            if(config["ColourCorrection"] == "matrix")
            {
               CurrentSettings.ColourCorr = 0;
               MainColour = matrix.Color(r,g,b);
               MixedColour = matrix.Color(r2,g2,b2);
               EEPROM.update(NVRAMAddr + 8, CurrentSettings.ColourCorr);
               SettingsChanged = true;
            }
            else if(config["ColourCorrection"] == "no_correction")
            {
               CurrentSettings.ColourCorr = 1;
               MainColour = Colour32(r,g,b);
               MixedColour = Colour32(r2,g2,b2);
               EEPROM.update(NVRAMAddr + 8, CurrentSettings.ColourCorr);
               SettingsChanged = true;
            }
            else if(config["ColourCorrection"] == "gamma")
            {
               CurrentSettings.ColourCorr = 2;
               MainColour = Colour32(r,g,b);
               MixedColour = Colour32(r2,g2,b2);
               EEPROM.update(NVRAMAddr + 8, CurrentSettings.ColourCorr);
               SettingsChanged = true;
            }
            tNotification.enable();
        }
        if(config.containsKey("SetSerialNumber") && FsysBlock.FsysSign != FSYS_SIGNATURE && config["Signature"] == FSYS_SIGNATURE)
        {
          const char* SN = config["SetSerialNumber"];
          Terminal.print(F("Serial Number Set: "));
          for (uint8_t i = 0; i < 11; i++)
          {
              EEPROM.update(FsysAddr + i, SN[i]);
              Terminal.print(SN[i]);
          }
          Terminal.println();
          EEPROM.put(FsysAddr + 12, FSYS_SIGNATURE);
          EEPROM.commit();
          NVIC_SystemReset();
        }
        if(config.containsKey("UseSN") && FsysBlock.FsysSign == FSYS_SIGNATURE && config["Signature"] == FSYS_SIGNATURE)
        {
          FsysBlock.UseSN = (bool)config["UseSN"];
          EEPROM.update(FsysAddr + 11, FsysBlock.UseSN);
          EEPROM.commit();
          ApplySettingsEvent();
          NVIC_SystemReset();
        }
        if(config.containsKey("FactoryReset"))
        {
          FactoryReset();
        }
        if (SettingsChanged)
        {
            EEPROM.commit();
            ApplySettingsEvent();
            Terminal.println("Settings Changed");
        }
    }
    else if(config["mode"]=="off")
    {
        CurrentMode = 0;
        tSignalTest.disable();
        tNotification.disable();
    }
    if(config["mode"] != "write-defaults")
    {
        PublishState();
    }
}
void setup()
{
  Terminal.begin();
  matrix.begin();
  ProgressBar(2);
  SerialModem.begin(115200);
  delay(6000);
  Terminal.print(F("Firmware "));
  Terminal.print(FW_NAME);  
  Terminal.print(F(", version "));
  Terminal.print(FW_VERSION);
  Terminal.print(F(",build "));  
  Terminal.println(BUILD);
  EEPROM.init();
  int sign;
  EEPROM.get(0, sign);
  if (sign != WRITTEN_SIGNATURE)
  {
    EEPROM.put(0, WRITTEN_SIGNATURE);
    for (uint8_t i = 0; i < 16; i++)
    {
        EEPROM.update(FsysAddr + i, NULL);
    }
    FactoryReset();
  }
  EEPROM.get(FsysAddr, FsysBlock);
  EEPROM.get(NVRAMAddr, CurrentSettings);
  tState.setInterval(CurrentSettings.SysStateFreq*TASK_MINUTE);
  getEEPROMData();
  if (FsysBlock.FsysSign == FSYS_SIGNATURE)
  {
      Terminal.print(F("Device SN: "));
      for (uint8_t i = 0; i < 11; i++)
      {
          FsysBlock.SerialNumber[i] = EEPROM.read(FsysAddr + i);
          Terminal.print(FsysBlock.SerialNumber[i]);
      }
      Terminal.println();
      Terminal.print("Use SN as ID: ");
      Terminal.println(FsysBlock.UseSN);
  }
  else
  {
      Terminal.println(F("Serial Number is not set"));
  }
  ProgressBar(4);
  modem.init();
  String DeviceImei = modem.getIMEI();
  if(DeviceImei == "")
  {
    error(1);
  }
  DeviceImei.toCharArray(DeviceID, 16);
  if (FsysBlock.UseSN && FsysBlock.FsysSign == FSYS_SIGNATURE)
  {
      for (uint8_t i = 0; i < 17; i++)
      {
        if(i < 11)
        {
          DeviceID[i] = FsysBlock.SerialNumber[i];
        }
        if (i >= 11)
        {
          DeviceID[i] = NULL;
        }
      }
  }
  Terminal.print(F("Device ID: "));
  Terminal.println(DeviceID);
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
  Terminal.print(F("NET? "));
  if (!modem.waitForNetwork())
  {
    Terminal.println(F("!OK"));
    error(2);
  }
  if (modem.isNetworkConnected())
  {
    Terminal.println("OK");
    ProgressBar(10);
  }
  Terminal.print(F("GPRS? "));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    Terminal.println(F("!OK"));
    error(2); 
  }
  if (modem.isGprsConnected()) 
  {
    Terminal.println(F("OK"));
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
            switch(CurrentSettings.DefaultAnim)
            {
              case 0: pulse(MainColour, &CurrentSettings); break;
              case 1: wave(MainColour); break;
            }
          break;
          case 2: 
            switch(CurrentSettings.DefaultAnim)
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
}
//Service 
void PublishState ()
{
      char status[256];
      StaticJsonDocument<256> state;
          switch(CurrentMode)
          {
            case 0: state["mode"] = "off"; break;
            case 1: state["mode"] = "pulse"; state["color"] = CurrentColour; break;
            case 2: state["mode"] = "mixed";  state["color"] = CurrentColour; state["color2"] = CurrentMixedColour; break;
            case 3: state["mode"] = "test"; break;
          }
      state["RSSI"] = -113 +(modem.getSignalQuality()*2);
      serializeJson(state, status);
      mqtt.publish(topicState, status);
}
void error(int errcode)
{
    Terminal.print(F("Error "));
    Terminal.print(errcode);
    switch (errcode)
    {
      case 1: Terminal.println(F(" - No connection with the modem")); break;
      case 2: Terminal.println(F(" - No connection with the GSM network")); break;
      case 3: Terminal.println(F(" - No connection with the internet")); break;
      case 4: Terminal.println(F(" - No connection with the server")); break;
      case 5: Terminal.println(F(" - SIM-Card is not inserted")); break;
    }
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
      if (CurrentSettings.AutoReset)
      {
        i++;
      }
    }
    if (CurrentSettings.AutoReset)
    {
      NVIC_SystemReset();
    }
    
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
  if(CurrentSettings.ColourCorr == 1)
  {
    matrix.fill(fadecolor, 0, 256);
  }
  else if(&CurrentSettings.ColourCorr == reinterpret_cast<uint8_t*>(2))
  {
    matrix.fill(matrix.gamma32(fadecolor), 0, 256);
  }
  else if(CurrentSettings.ColourCorr == 0)
  {
    matrix.fillScreen(fadecolor);
  }
  matrix.setBrightness(0);
  matrix.fillScreen(fadecolor);
  matrix.show();
  for (byte j = 0; j < brightness; j++)
  {
    matrix.setBrightness(j);
    matrix.show();
    delay(CurrentSettings.PulseFrameDelay);
  }
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  if(CurrentSettings.ColourCorr == 1)
  {
    matrix.fill(fadecolour, 0, 256);
  }
  else if(CurrentSettings.ColourCorr == 2)
  {
    matrix.fill(matrix.gamma32(fadecolour), 0, 256);
  }
  else if(CurrentSettings.ColourCorr == 0)
  {
    matrix.fillScreen(fadecolour);
  }
  for (byte k = brightness; k > 0; k--)
  {
    matrix.setBrightness(k);
    matrix.show();
  }
  matrix.fillScreen(matrix.Color(0, 0, 0));
  matrix.show();
}
void pulse(uint32_t Colour,struct Settings *set)
{
  if(set->ColourCorr == 1)
  {
    matrix.fill(Colour, 0, 256);
  }
  else if(set->ColourCorr == 2)
  {
    matrix.fill(matrix.gamma32(Colour), 0, 256);
  }
  else if(set->ColourCorr == 0)
  {
    matrix.fillScreen(Colour);
  }
  if(tNotification.getRunCounter() < NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.show();
    tNotification.delay(set->PulseFrameDelay);
  }
  else if(tNotification.getRunCounter() == NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.show();
    tNotification.delay(set->PulsePeakFreeze);
  }
  else if(tNotification.getRunCounter() > NotiBrightness)
  {
    matrix.setBrightness((NotiBrightness*2)-FN);
    matrix.show();
    tNotification.delay(set->PulseFrameDelay);
  }
  FN++;
  if(tNotification.getIterations() == 0)
  {
    if(CurrentMode == 2)
    {
        ColCh = !ColCh;
    }
    tNotification.restartDelayed(set->PulseZeroFreeze);
  }
}
void wave(uint32_t Colour)
{
    float brc = NotiBrightness / 8;
    if(CurrentSettings.ColourCorr == 1)
    {
       matrix.fill(Colour, 0, 256);
    }
    else if(CurrentSettings.ColourCorr == 2)
    {
      matrix.fill(matrix.gamma32(Colour), 0, 256);
    }
    else if(CurrentSettings.ColourCorr == 0)
    {
      matrix.fillScreen(Colour);
    }
    matrix.drawLine(FN, 0, FN, 16, NSGColour);
    matrix.drawLine(FN-1, 0, FN -1, 16, NSColour);
    matrix.drawLine(FN - 2, 0, FN - 2, 16, NSGColour);
    if (FN <= 9)
    {
      matrix.setBrightness(FN+(FN * brc));
      matrix.show();
      tNotification.delay(40);
    }
    else
    {
      matrix.setBrightness(NotiBrightness - ((FN - 8)*brc));
      matrix.show();
      tNotification.delay(40);
    }
  FN++;
  if(tNotification.getIterations() == 0)
  {
    matrix.setBrightness(0);
    matrix.show();
    if(CurrentMode == 2)
    {
        ColCh = !ColCh;
    }
    tNotification.restartDelayed(CurrentSettings.PulsePeakFreeze);
  }
}
void mixedPulse()
{
  switch (ColCh)
  {
     case false: pulse(MainColour, &CurrentSettings); break;
     case true: pulse(MixedColour, &CurrentSettings); break;
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
void FactoryReset()
{
   Terminal.println("Factory reset");

    Settings DefaultSettings;
    DefaultSettings.AutoReset = true;
    DefaultSettings.SysStateFreq = 1;
    DefaultSettings.DefaultAnim = 0;
    DefaultSettings.PulseFrameDelay = 3;
    DefaultSettings.PulsePeakFreeze = 1500;
    DefaultSettings.PulseZeroFreeze = 1500;
    DefaultSettings.ColourCorr = 0;
    
   matrix.setBrightness(40);
   matrix.fillScreen(matrix.Color(0,0,0));
   matrix.show();
   matrix.fillRect(0,0,4,16,matrix.Color(255,255,255));
   delay(500);
   matrix.show();
   matrix.fillRect(4,0,4,16,matrix.Color(50,168,168));
   delay(500);
   matrix.show();
   matrix.fillRect(8,0,4,16,matrix.Color(255,255,255));
   delay(500);
   matrix.show();
   matrix.fillRect(12,0,4,16,matrix.Color(50,168,168));
   delay(500);
   matrix.show();
 
   EEPROM.put(NVRAMAddr, DefaultSettings);
   EEPROM.commit();

   delay(2000);
   matrix.fillRect(0,0,4,16,matrix.Color(0,0,0));
   delay(500);
   matrix.show();
   matrix.fillRect(4,0,4,16,matrix.Color(0,0,0));
   delay(500);
   matrix.show();
   matrix.fillRect(8,0,4,16,matrix.Color(0,0,0));
   delay(500);
   matrix.show();
   matrix.fillRect(12,0,4,16,matrix.Color(0,0,0));
   delay(500);
   matrix.show();
   delay(500);
   NVIC_SystemReset();
}
void ApplySettingsEvent()
{
  char eventum[192];
  StaticJsonDocument<192> event;
  event["write-defaults"] = "success";
  JsonArray Current_Settings = event.createNestedArray("current-settings");
  Current_Settings.add(CurrentSettings.AutoReset);
  Current_Settings.add(CurrentSettings.SysStateFreq);
  Current_Settings.add(CurrentSettings.DefaultAnim);
  Current_Settings.add(CurrentSettings.PulseFrameDelay);
  Current_Settings.add(CurrentSettings.PulsePeakFreeze);
  Current_Settings.add(CurrentSettings.PulseZeroFreeze);
  Current_Settings.add(CurrentSettings.ColourCorr);
  serializeJson(event, eventum);
  mqtt.publish(topicEvent, eventum);
}
void AnimationChange()
{
    Terminal.println("Animation Change!");
    switch (CurrentSettings.DefaultAnim)
    {
        case 0: tNotification.setIterations((NotiBrightness*2)+1); break;
        case 1: tNotification.setIterations(18);break;
    }
}
void getEEPROMData()
{
  Terminal.println("EEPROM data: ");
  Terminal.print("Signature: ");
  for(int i = 0; i < 4; i++)
  {
      Terminal.print(char(EEPROM.read(i)));
  }
  Terminal.print(" (0x");
  for(int i = 0; i < 4; i++)
  {
      Terminal.print(EEPROM.read(i), HEX);
  }
  Terminal.println(")");
  Terminal.print("Fsys: ");
  for(int i = FsysAddr; i < FsysAddr+11; i++)
  {
      Terminal.print((char)EEPROM.read(i));
  }
  for(int i = FsysAddr+11; i < FsysAddr+12; i++)
  {
      Terminal.print(EEPROM.read(i));
  }
  for(int i = FsysAddr+12; i < FsysAddr+16; i++)
  {
      Terminal.print(char(EEPROM.read(i)));
  }
  Terminal.println();
  Terminal.print("NVRAM: ");
  for(int i = NVRAMAddr; i < NVRAMAddr+8; i++)
  {
      Terminal.print(EEPROM.read(i));
  }
  Terminal.println();
}
 uint32_t Colour32(uint8_t r, uint8_t g, uint8_t b) 
 {
    return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}