const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "2.1.0 alpha"

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
#include <SSLClient.h>
#include <certificates.h>
#define MATRIX_PIN PA7

const char my_cert[] = "-----BEGIN CERTIFICATE-----\n"
"MIIF2DCCA8CgAwIBAgIQTKr5yttjb+Af907YWwOGnTANBgkqhkiG9w0BAQwFADCB\n"
"hTELMAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4G\n"
"A1UEBxMHU2FsZm9yZDEaMBgGA1UEChMRQ09NT0RPIENBIExpbWl0ZWQxKzApBgNV\n"
"BAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMTE5\n"
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBhTELMAkGA1UEBhMCR0IxGzAZBgNVBAgT\n"
"EkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEaMBgGA1UEChMR\n"
"Q09NT0RPIENBIExpbWl0ZWQxKzApBgNVBAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNh\n"
"dGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCR\n"
"6FSS0gpWsawNJN3Fz0RndJkrN6N9I3AAcbxT38T6KhKPS38QVr2fcHK3YX/JSw8X\n"
"pz3jsARh7v8Rl8f0hj4K+j5c+ZPmNHrZFGvnnLOFoIJ6dq9xkNfs/Q36nGz637CC\n"
"9BR++b7Epi9Pf5l/tfxnQ3K9DADWietrLNPtj5gcFKt+5eNu/Nio5JIk2kNrYrhV\n"
"/erBvGy2i/MOjZrkm2xpmfh4SDBF1a3hDTxFYPwyllEnvGfDyi62a+pGx8cgoLEf\n"
"Zd5ICLqkTqnyg0Y3hOvozIFIQ2dOciqbXL1MGyiKXCJ7tKuY2e7gUYPDCUZObT6Z\n"
"+pUX2nwzV0E8jVHtC7ZcryxjGt9XyD+86V3Em69FmeKjWiS0uqlWPc9vqv9JWL7w\n"
"qP/0uK3pN/u6uPQLOvnoQ0IeidiEyxPx2bvhiWC4jChWrBQdnArncevPDt09qZah\n"
"SL0896+1DSJMwBGB7FY79tOi4lu3sgQiUpWAk2nojkxl8ZEDLXB0AuqLZxUpaVIC\n"
"u9ffUGpVRr+goyhhf3DQw6KqLCGqR84onAZFdr+CGCe01a60y1Dma/RMhnEw6abf\n"
"Fobg2P9A3fvQQoh/ozM6LlweQRGBY84YcWsr7KaKtzFcOmpH4MN5WdYgGq/yapiq\n"
"crxXStJLnbsQ/LBMQeXtHT1eKJ2czL+zUdqnR+WEUwIDAQABo0IwQDAdBgNVHQ4E\n"
"FgQUu69+Aj36pvE8hI6t7jiY7NkyMtQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB\n"
"/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAArx1UaEt65Ru2yyTUEUAJNMnMvl\n"
"wFTPoCWOAvn9sKIN9SCYPBMtrFaisNZ+EZLpLrqeLppysb0ZRGxhNaKatBYSaVqM\n"
"4dc+pBroLwP0rmEdEBsqpIt6xf4FpuHA1sj+nq6PK7o9mfjYcwlYRm6mnPTXJ9OV\n"
"2jeDchzTc+CiR5kDOF3VSXkAKRzH7JsgHAckaVd4sjn8OoSgtZx8jb8uk2Intzna\n"
"FxiuvTwJaP+EmzzV1gsD41eeFPfR60/IvYcjt7ZJQ3mFXLrrkguhxuhoqEwWsRqZ\n"
"CuhTLJK7oQkYdQxlqHvLI7cawiiFwxv/0Cti76R7CZGYZ4wUAc1oBmpjIXUDgIiK\n"
"boHGhfKppC3n9KUkEEeDys30jXlYsQab5xoq2Z0B15R97QNKyvDb6KkBPvVWmcke\n"
"jkk9u+UJueBPSZI9FoJAzMxZxuY67RIuaTxslbH9qh17f4a+Hg4yRvv7E491f0yL\n"
"S0Zj/gA0QHDBw7mh3aZw4gSzQbzpgJHqZJx64SIDqZxubw5lT2yHh17zbqD5daWb\n"
"QOhTsiedSrnAdyGN/4fy3ryM7xfft0kL0fJuMAsaDk527RH89elWsn2/x20Kk4yl\n"
"0MC2Hb46TpSi125sC8KKfPog88Tk5c0NqMuRkrF8hey1FGlmDoLnzc7ILaZRfyHB\n"
"NVOFBkpdn627G190\n"
"-----END CERTIFICATE-----\n";
const char my_key[] = "-----BEGIN EC PRIVATE KEY-----\n"
"-----END EC PRIVATE KEY-----\n";
SSLClientParameters mTLS = SSLClientParameters::fromPEM(my_cert, sizeof my_cert, my_key, sizeof my_key);

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
TinyGsmClient GSMclient(modem);
SSLClient client(GSMclient, TAs, (size_t)TAs_NUM, PA10);
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
const uint32_t brc = 10;
const byte delays = 30;
const byte MainBrightness = 100;
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
  matrix.begin();
  matrix.setBrightness(MainBrightness);
  matrix.fillRect(0,0,2,16,PBColour);
  matrix.show();
  SerialAT.begin(115200);
  matrix.fillRect(0,0,4,16,PBColour);
  matrix.show();
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
  matrix.fillRect(0,0,6,16,PBColour);
  matrix.show();
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
  client.setMutualAuthParams(mTLS);
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttRX);
  matrix.fillRect(0,0,14,16,PBColour);
  matrix.show();
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
    yield();
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
        matrix.fillScreen(matrix.Color(139,0,0));
    }
    else if (RSSI >  -110 && RSSI < -100)
    {
        matrix.fillScreen(matrix.Color(220,20,60));
    }
    else if (RSSI >=  -100 && RSSI < -85)
    {
        matrix.fillScreen(matrix.Color(255,165,0));
    }
    else if (RSSI >=  -85 && RSSI < -70)
    {
        matrix.fillScreen(matrix.Color(255,255,0));
    }
    else if (RSSI >=  -70)
    {
        matrix.fillScreen(matrix.Color(0,0,255));
    }
}