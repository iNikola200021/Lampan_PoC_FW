const char BUILD[] = __DATE__ " master";
#define FW_NAME "Lampan"
#define FW_VERSION "v1.0"
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
#define Terminal Serial
#define Modem Serial1

//Classes definition
TinyGsm modem(Modem);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, MATRIX_PIN,
                                               NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT +
                                                   NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                                               NEO_GRB + NEO_KHZ800);

Scheduler ts;

//APN Credentials
const char *apn = "internet.mts.ru";
const char *gprsUser = "mts";
const char *gprsPass = "mts";
// MQTT details
const char *broker = "mqtt.iotcreative.ru";
const char *mqtt_user = "dave";
const char *mqtt_pass = "lemontree";
int port = 1884;
char topicState[33] = "$device/";
char topicCmd[32] = "$device/";
char topicEvent[45] = "$device/";
int LDC = 0;
//Pulse anim
uint16_t PulseFD = 10;      //Delay time between frames. Milliseconds
uint16_t PeakFreeze = 1000; //Delay time in peaks of pulse. Milliseconds
uint16_t ZeroFreeze = 1000; //Delay time in zero brightness of pulse. Milliseconds
uint16_t FN = 0;
//Mixed anim
bool ColCh = false;
//ProgressBar parameters
const uint32_t PBColour = matrix.Color(255, 255, 255); //Progress Bar Colour
const uint8_t PBBrightness = 40;
//Notification params
uint8_t NotiBrightness = 100;
//Colours
uint16_t MainColour = matrix.Color(0, 255, 0);
uint32_t MixedColour = matrix.Color(255, 165, 0); //Mixed Notification Colour
char CurrentColour[13];
char CurrentMixedColour[13];
//Technical variables
bool IsSetupComplete = false;
char DeviceID[16];
uint32_t StateFreq = 1; //Minutes
//Error variables
uint8_t MQTTRT = 0;
//Error parameters
const uint8_t MQTTThreshold = 5;
//FUNCTIONS DECLARATION
//MQTT Functions
bool mqttConnect();
void mqttRX(char *topic, byte *payload, unsigned int len);
//Light Functions
void FadeOut(uint32_t fadecolour, int brightness);
//Notification Functions
bool PulseOn();
void NotiCallback();
void NotiOnDisable();
//Current mode: 0 - off; 1 - pulse; 2 - mixed;
uint8_t CurrentMode = 0;
void pulse(uint32_t Colour);
void mixedPulse();
void mixedWave();
//Service functions
void ProgressBar(int pb);
void error(int errcode);
void PublishState();

//TASKS
//Task 1 - Notification
//Task 2 - Publish State
Task tNotification(TASK_IMMEDIATE, 0, &NotiCallback, &ts, false, &PulseOn, &NotiOnDisable);
Task tState(StateFreq *TASK_MINUTE, TASK_FOREVER, &PublishState, &ts, true);

void setup()
{
  Terminal.begin();
  Modem.begin(115200);
  matrix.begin();
  matrix.clear();
  matrix.show();
  ProgressBar(2);
  delay(6000);
  Terminal.print(FW_NAME);
  Terminal.print(" FW ");
  Terminal.print(FW_VERSION);
  Terminal.print(" build ");
  Terminal.println(BUILD);
  ProgressBar(4);
  modem.restart();
  String DeviceImei = modem.getIMEI();
  if (DeviceImei == "")
  {
    error(1);
  }
  DeviceImei.toCharArray(DeviceID, 16);
  Terminal.print("Lamp ID number: ");
  Terminal.println(DeviceID);
  //Topics formation
  strcat(topicState, DeviceID);
  strcat(topicCmd, DeviceID);
  strcat(topicState, "/state");
  strcat(topicCmd, "/command");
  strcat(topicEvent, DeviceID);
  strcat(topicEvent, "/event");
  ProgressBar(6);
  if (modem.getSimStatus() != 1)
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
    Terminal.println(F("OK"));
    ProgressBar(10);
  }
  Terminal.print(F("GPRS? "));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    Terminal.println(F("!OK"));
    error(3);
  }
  if (modem.isGprsConnected())
  {
    Terminal.println(F("OK"));
    ProgressBar(12);
  }
  ts.addTask(tNotification);
  ts.addTask(tState);
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
  case false:
    FadeOut(MainColour, matrix.getBrightness()); //Get current brightness and fade
    ColCh = !ColCh;                              //Change ColourChange flag to start from another colour
    break;
  case true:
    FadeOut(MixedColour, matrix.getBrightness());
    ColCh = !ColCh;
    break;
  }
}
void NotiCallback()
{
  switch (CurrentMode)
  {
  case 1:
    pulse(MainColour);
    break;
  case 2:
    mixedPulse();
    break;
  }
}
//Service
void PublishState()
{
  char status[256];
  StaticJsonDocument<256> state;
  switch (CurrentMode)
  {
  case 0:
    state["mode"] = "off";
    break;
  case 1:
    state["mode"] = "pulse";
    state["color"] = CurrentColour;
    break;
  case 2:
    state["mode"] = "mixed";
    state["color"] = CurrentColour;
    state["color2"] = CurrentMixedColour;
    break;
  }
  state["RSSI"] = -113 + (modem.getSignalQuality() * 2);
  serializeJson(state, status);
  mqtt.publish(topicState, status);
}
void error(int errcode)
{
  Terminal.println("Error " + errcode);
  switch (errcode)
  {
  case 1:
    Terminal.println(F(" - No connection with the modem"));
    break;
  case 2:
    Terminal.println(F(" - No connection with the GSM network"));
    break;
  case 3:
    Terminal.println(F(" - No connection with the internet"));
    break;
  case 4:
    Terminal.println(F(" - No connection with the server"));
    break;
  case 5:
    Terminal.println(F(" - SIM-Card is not inserted"));
    break;
  }
  delay(1000);
  int i = 0;
  while (i < 5)
  {
    matrix.fillScreen(matrix.Color(255, 0, 0));
    matrix.show();
    delay(2000);
    switch (errcode)
    {
    case 1:
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(8, 0, 8, 16, matrix.Color(255, 0, 0));
      matrix.show();
      delay(2000);
      break;

    case 2:
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(7, 0, 7, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(9, 0, 9, 16, matrix.Color(0, 255, 0));
      matrix.show();
      delay(2000);
      break;

    case 3:
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(6, 0, 6, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(8, 0, 8, 16, matrix.Color(0, 255, 0));
      matrix.drawLine(10, 0, 10, 16, matrix.Color(0, 0, 255));
      matrix.show();
      delay(2000);
      break;

    case 4:
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(5, 0, 5, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(7, 0, 7, 16, matrix.Color(0, 255, 0));
      matrix.drawLine(9, 0, 9, 16, matrix.Color(255, 255, 255));
      matrix.drawLine(11, 0, 11, 16, matrix.Color(0, 0, 255));
      matrix.show();
      delay(2000);
      break;

    case 5:
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(4, 0, 4, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(6, 0, 6, 16, matrix.Color(0, 255, 0));
      matrix.drawLine(8, 0, 8, 16, matrix.Color(255, 255, 255));
      matrix.drawLine(10, 0, 10, 16, matrix.Color(0, 0, 255));
      matrix.drawLine(12, 0, 12, 16, matrix.Color(255, 0, 0));
      matrix.show();
      delay(2000);
      break;
    }
    i++;
  }
  NVIC_SystemReset();
}
void ProgressBar(int pb)
{
  matrix.setBrightness(PBBrightness);
  matrix.fillRect(0, 0, pb, 16, PBColour);
  matrix.show();
}
//Animations
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
  if (tNotification.getRunCounter() < NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PulseFD);
  }
  else if (tNotification.getRunCounter() == NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PeakFreeze);
  }
  else if (tNotification.getRunCounter() > NotiBrightness)
  {
    matrix.setBrightness((NotiBrightness * 2) - FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(PulseFD);
  }
  FN++;
  if (tNotification.getIterations() == 0)
  {
    if (CurrentMode == 2)
    {
      ColCh = !ColCh;
    }
    tNotification.restartDelayed(PeakFreeze);
  }
}
void mixedPulse()
{
  switch (ColCh)
  {
  case false:
    pulse(MainColour);
    break;
  case true:
    pulse(MixedColour);
    break;
  }
}
bool PulseOn()
{
  FN = 0;
  return true;
}
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
      mqtt.subscribe(topicCmd, 1);
      if (!IsSetupComplete) //Add Boot is Complete Animation
      {
        IsSetupComplete = true;
        ProgressBar(16);
        FadeOut(PBColour, PBBrightness);
        tState.enable();
        Terminal.println(F("Boot is complete"));
      }
    }
    else
    {
      if (MQTTRT < MQTTThreshold)
      {
        Terminal.print(F(" NOT OK, status: "));
        Terminal.println(mqtt.state());
        MQTTRT++;
      }
      else
      {
        error(4);
      }
    }
  }
  return mqtt.connected();
}
void mqttRX(char *topic, byte *payload, unsigned int len)
{
  Terminal.print(F("Got message!"));
  StaticJsonDocument<100> config;
  char payld[len];
  for (unsigned int i = 0; i < len; i++)
  {
    payld[i] = (char)payload[i];
  }
  DeserializationError error = deserializeJson(config, payld);
  if ((config["mode"] == "pulse") || (config["mode"] == "mixed"))
  {
    //Colour change
    const char *NewColour1 = config["color"];
    uint64_t colour = config["color"];
    int r = colour / 1000000000;
    int g = (colour % 1000000000) / 1000000;
    int b = (colour % 1000000) / 1000;
    int a = colour % 1000;
    if (NewColour1 != CurrentColour)
    {
      tNotification.disable();
      MainColour = matrix.Color(r, g, b);
      strncpy(CurrentColour, NewColour1, strlen(NewColour1));
      NotiBrightness = a;
      tNotification.setIterations((NotiBrightness * 2) + 1);
      if (config["mode"] == "pulse")
      {
        CurrentMode = 1;
        ColCh = false;
        tNotification.enable();
      }
    }
    if (config["mode"] == "mixed")
    {
      const char *NewColour2 = config["color2"];
      uint64_t colour2 = config["color2"];
      int r2 = colour2 / 1000000000;
      int g2 = (colour2 % 1000000000) / 1000000;
      int b2 = (colour2 % 1000000) / 1000;
      if (NewColour2 != CurrentMixedColour)
      {
        tNotification.disable();
        strncpy(CurrentMixedColour, NewColour2, strlen(NewColour2));
        MixedColour = matrix.Color(r2, g2, b2);
        tNotification.enable();
      }
      if (CurrentMode != 2)
      {
        CurrentMode = 2;
        tNotification.enable();
      }
    }
    //Colour change end
  }
  else if (config["mode"] == "write-defaults")
  {
    Terminal.println(F("Write-defaults mode"));
    if (config["process"] == "State")
    {
      StateFreq = config["-t"];
    }
    if (config["process"] == "Pulse")
    {
      if (config["-FD"] != "") // FrameDelay
      {
        PulseFD = config["-FD"];
      }
      if (config["-PF"] != "") // Peak Freeze
      {
        PeakFreeze = config["-PF"];
      }
      if (config["-ZF"] != "") //Zero Freeze
      {
        ZeroFreeze = config["-ZF"];
      }
    }
  }
  if (config["mode"] == "off")
  {
    CurrentMode = 0;
    tNotification.disable();
  }
  PublishState();
}