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
#include "Adafruit_FONA.h"
#define PIN A7
#define Terminal Serial
#define fonaSerial Serial1
//Classes definition
Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();
//TinyGsmClient modemClient (modemc);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, PIN,
                                               NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT +
                                                   NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                                               NEO_GRB + NEO_KHZ800);

Scheduler ts;

  /*#include <StreamDebugger.h>
  StreamDebugger debugger(Modem, Terminal);
  TinyGsm modemc(debugger);*/

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
void tMQTTConnCallback();
void tMQTTRXCallback();
//TASKS
//Task 1 - Notification
//Task 2 - Publish State
//Task 3 - Signal Test
Task tNotification(TASK_IMMEDIATE, 201, &NotiCallback, &ts, false, &PulseOn, &NotiOnDisable);
Task tState(1 * TASK_MINUTE, TASK_FOREVER, &PublishState, &ts, false);
Task tMQTTConn(2*TASK_SECOND, TASK_FOREVER, &tMQTTConnCallback, &ts, false);
Task tMQTTRX(TASK_IMMEDIATE, TASK_FOREVER, &tMQTTRXCallback, &ts, false);
void moduleSetup();
bool netStatus();

void setup()
{
  Terminal.begin();
  //Modem.begin(115200);
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
  moduleSetup();
  fona.setFunctionality(1);
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
  
  ProgressBar(8);
  Terminal.print(F("NET? "));
  if (!netStatus())
  {
    Terminal.println(F("!OK"));
    error(2);
  }
  Terminal.println("OK");
  ProgressBar(10);
  Terminal.print(F("GPRS? "));
  fona.setNetworkSettings(F(apn), F(gprsUser), F(gprsPass));
  while (!fona.enableGPRS(true)) 
  {
    Terminal.println(F("Failed to enable connection, retrying..."));
    delay(2000); // Retry every 2s
  }
  Terminal.println(F("Enabled data!"));
  ProgressBar(12);
  ProgressBar(14);
  ts.addTask(tState);
  tState.enable();
  ts.addTask(tNotification);
  tNotification.enable();
  //MQTTPublish(topicState, 1, false, "{\"event\":\"connect\",\"LDC\":0}");
}
void loop()
{
  if (! fona.MQTT_connectionStatus()) 
  {
    // Set up MQTT parameters (see MQTT app note for explanation of parameter values)
    fona.MQTT_setParameter("URL", broker, port);
    // Set up MQTT username and password if necessary
    fona.MQTT_setParameter("USERNAME", mqtt_user);
    fona.MQTT_setParameter("PASSWORD", mqtt_pass);
    fona.MQTT_setParameter("CLIENTID", DeviceID);
//    fona.MQTTsetParameter("KEEPTIME", 30); // Time to connect to server, 60s by default
    
    Terminal.println(F("Connecting to MQTT broker..."));
    if (! fona.MQTT_connect(true)) {
      Terminal.println(F("Failed to connect to broker!"));
    }
  }
  else {
    Terminal.println(F("Already connected to MQTT server!"));
  }
  
  if (!fona.MQTT_publish(topicEvent, DeviceID, strlen(DeviceID), 1, 0)) Terminal.println(F("Failed to publish!"));
  //Terminal.println("Loop!");
  //if(Modem.available()){    Terminal.write(Modem.read());}
  //if(    Terminal.available()){Modem.write(    Terminal.read());}
  ts.execute();
}
void ProgressBar(int pb)
{
  matrix.setBrightness(PBBrightness);
  matrix.fillRect(0, 0, pb, 16, PBColour);
  matrix.show();
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
  //state["RSSI"] = -113 + (modemc.getSignalQuality() * 2);
  serializeJson(state, status);
  Terminal.println(F("State!"));
  //MQTTPublish(topicState, 1, 0, status);
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

void moduleSetup() {
  // SIM7000 takes about 3s to turn on and SIM7500 takes about 15s
  // Press Arduino reset button if the module is still turning on and the board doesn't find it.
  // When the module is on it should communicate right after pressing reset

  // Software serial:
  fonaSerial.begin(115200); // Default SIM7000 shield baud rate


  // Hardware serial:
  /*
  fonaSerial->begin(115200); // Default SIM7000 baud rate
  if (! fona.begin(*fonaSerial)) {
    DEBUG_PRINTLN(F("Couldn't find SIM7000"));
  }
  */
  if (!fona.begin(fonaSerial)) 
  {
    Terminal.println(F("Couldn't find FONA"));
    error(1); // Don't proceed if it couldn't find the device
  }
  // The commented block of code below is an alternative that will find the module at 115200
  // Then switch it to 9600 without having to wait for the module to turn on and manually
  // press the reset button in order to establish communication. However, once the baud is set
  // this method will be much slower.
  /*
  fonaSerial->begin(115200); // Default LTE shield baud rate
  fona.begin(*fonaSerial); // Don't use if statement because an OK reply could be sent incorrectly at 115200 baud
  Terminal.println(F("Configuring to 9600 baud"));
  fona.setBaudrate(9600); // Set to 9600 baud
  fonaSerial->begin(9600);
  if (!fona.begin(*fonaSerial)) {
    Terminal.println(F("Couldn't find modem"));
    while(1); // Don't proceed if it couldn't find the device
  }
  */
uint8_t type;
  type = fona.type();
  Terminal.println(F("FONA is OK"));
  Terminal.print(F("Found "));
  switch (type) {
    case SIM800L:
      Terminal.println(F("SIM800L")); break;
    case SIM800H:
      Terminal.println(F("SIM800H")); break;
    case SIM808_V1:
      Terminal.println(F("SIM808 (v1)")); break;
    case SIM808_V2:
      Terminal.println(F("SIM808 (v2)")); break;
    case SIM5320A:
      Terminal.println(F("SIM5320A (American)")); break;
    case SIM5320E:
      Terminal.println(F("SIM5320E (European)")); break;
    case SIM7000:
      Terminal.println(F("SIM7000")); break;
    case SIM7070:
      Terminal.println(F("SIM7070")); break;
    case SIM7500:
      Terminal.println(F("SIM7500")); break;
    case SIM7600:
      Terminal.println(F("SIM7600")); break;
    case SIM800_MQTT:
      Terminal.println(F("SIM800 MQTT")); break;
    default:
      Terminal.println(F("???")); break;
  }
  
  // Print module IMEI number.
  uint8_t imeiLen = fona.getIMEI(DeviceID);
  if (imeiLen > 0) {
    Terminal.print("Module IMEI: "); Terminal.println(DeviceID);
  }
}

// Read the module's power supply voltage

bool netStatus() {
  int n = fona.getNetworkStatus();
  
  Terminal.print(F("Network status ")); Terminal.print(n); Terminal.print(F(": "));
  if (n == 0) Terminal.println(F("Not registered"));
  if (n == 1) Terminal.println(F("Registered (home)"));
  if (n == 2) Terminal.println(F("Not registered (searching)"));
  if (n == 3) Terminal.println(F("Denied"));
  if (n == 4) Terminal.println(F("Unknown"));
  if (n == 5) Terminal.println(F("Registered roaming"));

  if (!(n == 1 || n == 5)) return false;
  else return true;
}