const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "3.0.0 alpha"

#define TINY_GSM_MODEM_SIM800
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoMatrix.h>
#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

#define MATRIX_PIN PA7
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define SerialMon Serial
#define SerialAT Serial1

//Classes definition
TinyGsm modem(SerialAT);
TinyGsmClient ModemClient(modem);
IPStack ipstack(ModemClient);
MQTT::Client<IPStack, Countdown, 50, 1> client = MQTT::Client<IPStack, Countdown, 50, 1>(ipstack);
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
char broker[] = "iotcreative.ru";
int port = 1883;
char* mqtt_user = "dave";
char* mqtt_pass = "lemontree";
const char* topicRegister = "/device/register";
char topicStatus[29] = "/device/";
char topicService[40]= "/device/";
//Light parameters
const byte brc = 10;
const byte delays = 30;
const byte MainBrightness = 40;
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
void messageArrived(MQTT::MessageData& md);
void connect();
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
//Task 1 - Notification
//Task 2 - Publish RSSI
//Task 3 - Signal Test
Task tNotification(TASK_IMMEDIATE, TASK_FOREVER, &NotiCallback, &ts,false, &NotiOnEnable, &NotiOnDisable);
Task tRSSI(ServTime*TASK_SECOND, TASK_FOREVER, &PublishRSSI, &ts);
Task tSignalTest(TASK_IMMEDIATE, TASK_FOREVER, &SignalTest, &ts);

void setup()
{
  SerialMon.begin();
  delay(3000);
  SerialMon.println(F("BOOT"));
  matrix.begin();
  matrix.setBrightness(MainBrightness);
  matrix.fillRect(0,0,2,16,PBColour);
  matrix.show();

  SerialAT.begin(115200);
  matrix.fillRect(0,0,4,16,PBColour);
  matrix.show();

  delay(3000);
  SerialMon.println("INIT");
  SerialMon.print("Firmware ");
  SerialMon.print(FW_NAME);  
  SerialMon.print(", version ");
  SerialMon.print(FW_VERSION);
  SerialMon.print(",build ");  
  SerialMon.println(BUILD);
  matrix.fillRect(0,0,6,16,PBColour);
  matrix.show();

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
  }
  matrix.fillRect(0,0,10,16,PBColour);
  matrix.show();

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
  }
  matrix.fillRect(0,0,12,16,PBColour);
  matrix.show();
  ts.addTask(tNotification);
  ts.addTask(tRSSI);
  tRSSI.enable();
  SerialMon.println(F("SETUP"));
  matrix.fillRect(0,0,14,16,PBColour);
  connect();
}

void loop()
{
    ts.execute();
    if (!client.isConnected())
    {
    connect();
    }
}
void connect()
{

  Serial.print("Connecting to ");
  Serial.print(broker);
  Serial.print(":");
  Serial.println(port);
 
  int rc = ipstack.connect(broker, port);
  if (rc != 1)
  {
    Serial.print("rc from TCP connect is ");
    Serial.println(rc);
  }
  Serial.println("MQTT connecting");
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
  data.MQTTVersion = 3;
  data.clientID.cstring = DeviceID;
  data.username.cstring = mqtt_user;
  data.password.cstring = mqtt_pass;
  rc = client.connect(data);
  if (rc != 0)
  {
    Serial.print("rc from MQTT connect is ");
    Serial.println(rc);
  }
  Serial.println("MQTT connected");
  MQTT::Message RegisterMsg;
  RegisterMsg.qos = MQTT::QOS0;
  RegisterMsg.retained = false;
  RegisterMsg.dup = false;
  RegisterMsg.payload = DeviceID;
  RegisterMsg.payloadlen = strlen(DeviceID)+1;
  rc = client.publish(topicRegister, RegisterMsg);
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
  rc = client.subscribe(topicStatus, MQTT::QOS1, messageArrived);   
  if (rc != 0)
  {
    Serial.print("rc from MQTT subscribe is ");
    Serial.println(rc);
  }
  Serial.println("MQTT subscribed");
  PublishRSSI();
}
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;
  
  Serial.print("Message arrived: qos");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  Serial.println((char*)message.payload);

  if (!strncmp((char *)message.payload, "on", message.payloadlen))
  {
    SerialMon.println(F("LED ON"));
    if(!tNotification.isEnabled())
    {
    tNotification.enable();
    }
  }
  else if (!strncmp((char *)message.payload, "off", message.payloadlen))
  {
    SerialMon.println(F("LED OFF"));
    tNotification.disable();
  }
}
bool NotiOnEnable()
{
    Fadein(NBColour, NotiBrightness);
    return true;
}
void NotiOnDisable()
{
  FadeOut(NBColour, NotiBrightness);
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
      MQTT::Message RSSIMsg;
      RSSIMsg.qos = MQTT::QOS0;
      RSSIMsg.retained = false;
      RSSIMsg.dup = false;
      RSSIMsg.payload = serrssi;
      RSSIMsg.payloadlen = strlen(serrssi)+1;
      client.publish(topicService, RSSIMsg);
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