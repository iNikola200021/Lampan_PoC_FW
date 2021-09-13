const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME "Lampan"
#define FW_VERSION "v0.1-Native-MQTT"
#define ARDUINOJSON_USE_LONG_LONG 1

#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <TinyGsmClient.h>
#define PIN A7
#define Terminal Serial
#define Modem Serial1
//Classes definition
TinyGsm modemc(Modem);
TinyGsmClient modemClient (modemc);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, PIN,
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
char topicState[32] = "$device/";
char topicCmd[32] = "$device/";
char topicEvent[32] = "$device/";
int ComQOS = 1;
int LDC = 0;
//Pulse anim //Delay time in peaks of pulse. Milliseconds
//Delay time in zero brightness of pulse. Milliseconds
uint16_t FN;
//Mixed anim
bool ColCh = false;
//ProgressBar parameters
const uint32_t PBColour = matrix.Color(255, 255, 225); //Progress Bar Colour
const uint8_t PBBrightness = 40;
//Notification params
uint8_t NotiBrightness = 100;
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

//Light Functions
void FadeOut(uint32_t fadecolour, int brightness);
//Notification Functions
bool PulseOn();
void NotiCallback();
void NotiOnDisable();
//Current mode: 0 - off; 1 - pulse; 2 - mixed; 3 - test;
//Default Animation: 0 - pulse; 1 - wave;
uint8_t CurrentMode = 1;
void pulse(uint32_t Colour);
void mixedPulse();
//Service functions
void ProgressBar(int pb);
void error(int errcode);
void PublishState();
void FactoryReset();
void ApplySettingsEvent();
void AnimationChange();
void getEEPROMData();
uint32_t Colour32(uint8_t r, uint8_t g, uint8_t b);
void MQTTConfig();
void MQTTConnect();
bool isMQTTConnected();
void MQTTPublish(char *topic, int qos, int retained, String &message);
void escapeQuotes(char *foo);
void tMQTTConnCallback();
void tMQTTRXCallback();
//TASKS
//Task 1 - Notification
//Task 2 - Publish State
//Task 3 - Signal Test
Task tNotification(TASK_IMMEDIATE, 201, &NotiCallback, &ts, false, &PulseOn, &NotiOnDisable);
Task tState(1 * TASK_MINUTE, TASK_FOREVER, &PublishState, &ts, false);
Task tMQTTConn(TASK_IMMEDIATE, TASK_FOREVER, &tMQTTConnCallback, &ts, false);
Task tMQTTRX(TASK_IMMEDIATE, TASK_FOREVER, &tMQTTRXCallback, &ts, false);

void setup()
{
  Terminal.begin();
  Modem.begin(115200);
  matrix.begin();
  matrix.clear();
  matrix.show();
  ProgressBar(2);
  delay(6000);
  Terminal.print(F("Firmware "));
  Terminal.print(FW_NAME);
  Terminal.print(F(", version "));
  Terminal.print(FW_VERSION);
  Terminal.print(F(",build "));
  Terminal.println(BUILD);
  ProgressBar(4);
  modemc.restart();
  String DeviceImei = modemc.getIMEI();
  if (DeviceImei == "")
  {
    error(1);
  }
  DeviceImei.toCharArray(DeviceID, 16);
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
  if (modemc.getSimStatus() != 1)
  {
    error(5);
  }
  ProgressBar(8);
  Terminal.print(F("NET? "));
  if (!modemc.waitForNetwork())
  {
    Terminal.println(F("!OK"));
    error(2);
  }
  if (modemc.isNetworkConnected())
  {
    Terminal.println("OK");
    ProgressBar(10);
  }
  Terminal.print(F("GPRS? "));
  if (!modemc.gprsConnect(apn, gprsUser, gprsPass))
  {
    Terminal.println(F("!OK"));
    error(3);
  }
  if (modemc.isGprsConnected())
  {
    Terminal.println(F("OK"));
    ProgressBar(12);
  }
  ProgressBar(14);
  MQTTConfig();
  MQTTConnect();
  ts.addTask(tState);
  tState.enable();
  ts.addTask(tMQTTConn);
  tMQTTConn.enable();
  ts.addTask(tNotification);
  tNotification.enable();
  ts.addTask(tMQTTRX);
  tMQTTRX.enable();
  //MQTTPublish(topicState, 1, false, "{\"event\":\"connect\",\"LDC\":0}");
}
void loop()
{
  //Terminal.println("Loop!");
  //if(Modem.available()){    Terminal.write(Modem.read());}
  //if(    Terminal.available()){Modem.write(    Terminal.read());}
  ts.execute();
  modemc.maintain();
}
void ProgressBar(int pb)
{
  matrix.setBrightness(PBBrightness);
  matrix.fillRect(0, 0, pb, 16, PBColour);
  matrix.show();
}
void MQTTConfig()
{
  modemc.sendAT(GF("+SMCONF="), GF("\"URL"), GF("\","), GF("\""), broker, GF(":"), port, GF("\""));
  modemc.waitResponse(1000);
  modemc.sendAT(GF("+SMCONF="), GF("\"CLIENTID"), GF("\","), GF("\""), DeviceID, GF("\""));
  modemc.waitResponse(1000);
  modemc.sendAT(GF("+SMCONF="), GF("\"USERNAME"), GF("\","), GF("\""), mqtt_user, GF("\""));
  modemc.waitResponse(1000);
  modemc.sendAT(GF("+SMCONF="), GF("\"PASSWORD"), GF("\","), GF("\""), mqtt_pass, GF("\""));
  modemc.waitResponse(1000);
}
void error(int errcode)
{
  Terminal.print(F("Error "));
  Terminal.print(errcode);
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
  int i = 0;
  while (i < 5)
  {
    matrix.fillScreen(matrix.Color(255, 0, 0));
    matrix.show();
    delay(2000);
    switch (errcode)
    {
    case 1: //Blank IMEI -> No connection with the modem
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(8, 0, 8, 16, matrix.Color(255, 0, 0));
      matrix.show();
      delay(2000);
      break;

    case 2: //No NET
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(7, 0, 7, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(9, 0, 9, 16, matrix.Color(0, 255, 0));
      matrix.show();
      delay(2000);
      break;

    case 3: //No GPRS
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(6, 0, 6, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(8, 0, 8, 16, matrix.Color(0, 255, 0));
      matrix.drawLine(10, 0, 10, 16, matrix.Color(0, 0, 255));
      matrix.show();
      delay(2000);
      break;

    case 4: //No MQTT
      matrix.fillScreen(matrix.Color(0, 0, 0));
      matrix.drawLine(5, 0, 5, 16, matrix.Color(255, 0, 0));
      matrix.drawLine(7, 0, 7, 16, matrix.Color(0, 255, 0));
      matrix.drawLine(9, 0, 9, 16, matrix.Color(255, 255, 255));
      matrix.drawLine(11, 0, 11, 16, matrix.Color(0, 0, 255));
      matrix.show();
      delay(2000);
      break;

    case 5: //No SIM
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
void MQTTConnect()
{
  modemc.sendAT(GF("+SMCONN"));
  Terminal.print(F("Pending MQTT Connection..."));
  uint8_t res = modemc.waitResponse(10000);
  if (res == 1)
  {
    Terminal.println(F("connected"));
    if (!IsBooted)
    {
      ProgressBar(16);
      IsBooted = true;
      FadeOut(PBColour, PBBrightness);
    }
    modemc.sendAT(GF("+SMSUB="), GF("\""), topicCmd, GF("\","), ComQOS);
    uint8_t res = modemc.waitResponse(10000);
    Terminal.print(F("Sub Res is: "));
    Terminal.println(res);
    String output;
    StaticJsonDocument<48> event;
    event["event"] = "connect";
    event["LDC"] = LDC;
    serializeJson(event, output);
    LDC++;
    Terminal.println(F("MQTTPublish end"));
    MQTTPublish(topicEvent, 1, 0, output);
  }
}
void MQTTPublish(char *topic, int qos, int retained, String &message)
{
  message.replace("\"", "\\\"");
  int len = message.length() + 1;
  char mes[len];
  message.toCharArray(mes, len);
  modemc.sendAT(GF("+SMPUB="), GF("\""), topic, GF("\","), qos, GF(","), retained, GF(","), GF("\""), mes, GF("\""));
}
void FadeOut(uint32_t fadecolour, int brightness)
{
  matrix.fill(fadecolour, 0, 256);
  for (byte k = brightness; k > 0; k--)
  {
    matrix.setBrightness(k);
    matrix.show();
  }
  matrix.clear();
  matrix.show();
}
void PublishState()
{
  String status;
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
  case 3:
    state["mode"] = "test";
    break;
  }
  state["RSSI"] = -113 + (modemc.getSignalQuality() * 2);
  serializeJson(state, status);
  Terminal.println(F("State!"));
  MQTTPublish(topicState, 1, 0, status);
}
bool isMQTTConnected()
{
  modemc.sendAT(GF("+SMSTATE?"));
  uint8_t res = modemc.waitResponse(10000L, GF(GSM_NL "SMSTATE: 1"));
  if (res == 1)
  {
    return true;
  }
  else
  {
    return false;
  }
}
void tMQTTConnCallback()
{
  if (!isMQTTConnected())
  {
    MQTTConnect();
  }
}
void pulse(uint32_t Colour)
{
  if (tNotification.getRunCounter() < NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(10);
  }
  else if (tNotification.getRunCounter() == NotiBrightness)
  {
    matrix.setBrightness(FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(1500);
  }
  else if (tNotification.getRunCounter() > NotiBrightness)
  {
    matrix.setBrightness((NotiBrightness * 2) - FN);
    matrix.fillScreen(Colour);
    matrix.show();
    tNotification.delay(10);
  }
  FN++;
  if (tNotification.getIterations() == 0)
  {
    if (CurrentMode == 2)
    {
      ColCh = !ColCh;
    }
    tNotification.restartDelayed(1500);
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
void NotiOnDisable()
{
  switch (ColCh)
  {
  case false:
    FadeOut(MainColour, matrix.getBrightness());
    ColCh = !ColCh;
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
bool PulseOn()
{
  FN = 0;
  return true;
}
void tMQTTRXCallback()
{
  if(modemc.stream.available()>0)
  {
    Terminal.println(modemc.stream.readStringUntil('\n'));
  }
}