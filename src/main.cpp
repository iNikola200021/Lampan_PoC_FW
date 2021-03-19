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
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
// Add a reception delay - may be needed for a fast processor at a slow baud rate
// #define TINY_GSM_YIELD() { delay(2); }
// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define SerialMon Serial
#define SerialAT Serial1
const char my_cert[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDtTCCAp0CFFC34Ac6H4/t3DXcRgfS0CTeF1OHMA0GCSqGSIb3DQEBCwUAMIGI\n"
"MQswCQYDVQQGEwJSVTEPMA0GA1UECAwGTW9zY293MQ8wDQYDVQQHDAZNb3Njb3cx\n"
"FTATBgNVBAoMDElvVCBDcmVhdGl2ZTELMAkGA1UECwwCSFExDTALBgNVBAMMBG1x\n"
"dHQxJDAiBgkqhkiG9w0BCQEWFWFydGh1cnNtYWluQGdtYWlsLmNvbTAeFw0yMTAz\n"
"MTcwOTAyNTdaFw0yMjAzMTIwOTAyNTdaMIGkMQswCQYDVQQGEwJSVTEPMA0GA1UE\n"
"CAwGTW9zY293MQ8wDQYDVQQHDAZNb3Njb3cxHDAaBgNVBAoME0lvVCBDcmVhdGl2\n"
"ZSBDbGllbnQxDzANBgNVBAsMBkNsaWVudDEUMBIGA1UEAwwLbXF0dC1jbGllbnQx\n"
"LjAsBgkqhkiG9w0BCQEWH21xdHQtY2xpZW50QG1xdHQuaW90Y3JlYXRpdmUucnUw\n"
"ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQD3H6kpoHDJhEEaOZLt/Nle\n"
"pSZxcpENKCZO8J3SO3x14qFy4ZNJE1Xk6bVt488EgwVp9Re0nDenDHrghiUlYQMe\n"
"mvqKQsiJL4MLtlfVRElH6mkxEi6J9IcyLTFbHaj9bzjVUFWAFk6Y6C5hHTBF0LoJ\n"
"aUx/iWIcTHzpaTv2N9wE1Vc6CW9CjLPx6MV9BLg2XcZFGAyRpqnwSmsMVV7X5j0U\n"
"pCacRHn91Da148E2iOP9NsYuCl6ndFDB2M1SPi6t/iM8Hy7jGCRwCNhh+Rmxqovh\n"
"0/M+t/f/aOO9oxtseqr3EHIhrYbbkJx1H7DcEElEGJOW/Coe+g/t4lxohWqp4OyB\n"
"AgMBAAEwDQYJKoZIhvcNAQELBQADggEBAGJOJOKUnM5yt888jd9/WejrewZkxZS+\n"
"qLADJSpURReqySP2SkUrogfYSQ7VuIhAhcF1hADjR1pSmDcRhUVbUEShY7edkvb/\n"
"2EfmgBm0tKu7+1Jf04e0Q7QZEMvDmgZxmRvq4fU07r0JPObYiJYnOvi8Fj6uEjbx\n"
"pO9TlBbJoJFl9zWNjL85Gm8IIcvM5LVzyjlthlOv2vd/FpwFldlDHkBS357pKRvQ\n"
"5BHEnYuZ/XccHntnEeFeRK6T2YwZMsiPDYFe43237fNvayYTkdlm7/iiTvBWC4XL\n"
"q5JMjoNCjiV5ZLCNYwuGjUku5L/gwvAgGNpTrbLegMDJbsHy/n9jFrY=\n"
"-----END CERTIFICATE-----";
const char my_key[] = 
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEpQIBAAKCAQEA9x+pKaBwyYRBGjmS7fzZXqUmcXKRDSgmTvCd0jt8deKhcuGT\n"
"SRNV5Om1bePPBIMFafUXtJw3pwx64IYlJWEDHpr6ikLIiS+DC7ZX1URJR+ppMRIu\n"
"ifSHMi0xWx2o/W841VBVgBZOmOguYR0wRdC6CWlMf4liHEx86Wk79jfcBNVXOglv\n"
"Qoyz8ejFfQS4Nl3GRRgMkaap8EprDFVe1+Y9FKQmnER5/dQ2tePBNojj/TbGLgpe\n"
"p3RQwdjNUj4urf4jPB8u4xgkcAjYYfkZsaqL4dPzPrf3/2jjvaMbbHqq9xByIa2G\n"
"25CcdR+w3BBJRBiTlvwqHvoP7eJcaIVqqeDsgQIDAQABAoIBAQD0MpQd704kxAz1\n"
"FjmL9NTzwqy3ZGfToX4heYHq8h9t+lJhzGiXyISTXSKMgZXbNoGOD8SqE8imyR1P\n"
"ax+fzTqg7rJvF8lETu/nDfvDYeQsLYZfGEFmz30dZy0jha6tKU/28phapzBpxt21\n"
"13sRMk3z6NrQGHkbz7WLjRLUnqRLHHkZoNNQb3cqkAEs8i3sf4efgu2LMZqU1xCr\n"
"ccMBXi9A5SsMsnjED9KmzEcELA58MLCCTDK4iPgdCJ4pK2GX5m3H0BAmnbU7acOS\n"
"Su28DgxF23nX3BSgy20ASx165WUPAMTwqWLIaJdpAvFr8AI+QKa1zIYf3juXsAyw\n"
"OfEjWaGxAoGBAP+PwaPEB7q3Ke01ZefllAL4AfBd/MLmoTRoEjFYMMuYquzlNAk9\n"
"OR2m+JlV6nYcWgkSPONmQik34iL9LPZK29eADDJs/Vz35sKpCyVdRLEobN0UFMFZ\n"
"18K/FashpDLpUGiRECh62F+Nt5eKTo7iVi5eMptzjaEIBjTk3qYHvHTFAoGBAPeM\n"
"Msz+zY1ZH6OSGfAvl9D5cFRFSBcm4AIDbdk6dp+vw5dm0f49Z1CUYUKbFdrDPueX\n"
"bQY6Ljn/2M6/hfUG7iQqCgttvJZ4rb2F3NuLYYZJ4bm8e9Djovs+jRFli3H59EfH\n"
"h0kUcvdgUcpZz65xWrZgBBTrc+oV3ttRtPAqo+yNAoGBANdCnVZ17hTF51jYm4ej\n"
"BvMhN3DhmeAJJ0vfRUOvBxrW81IrFZOasBtb4xY1GhJ2exgc9pCMN7pESgQywMaL\n"
"83UndB6dUyQV45efj7XfQ9b9OAVXvsvGA0kUbwoAgjAn+gS5+6Q9JTgHRNcuNlGA\n"
"MBtKWNI7mNZEzgWLqkarSYAJAoGALIDR6TSts4mgPIHZk+qaeoE9hojmCvN8NiBz\n"
"sSfnh5HLsf5UJkbXug7S5WyII0oSlwQMH6B6CwdJ7PFVndVfHrVIFQHnc43Cfl4A\n"
"dJ+8Kq5BnGicQdOKy2xVZBlDVnw/RGBEnAjHGl4AlkZLJetPCZZ9ZV1m+2BvLjKk\n"
"OsEPs0kCgYEAqV3tI72mf46fI90CeaqLgLUsmGaJRNEoGZE27lN3U2cDMAwRNOZ8\n"
"mPh+uUVXRpMBEdaKSf+b6dCgwbVgrBI/L/x0GWRa2iTD6oeNlIf9AzhhrWmuWLw4\n"
"3coYSmuC6S6ebyL7KOTzgoL4+rMxKT5Zg4loHfcooWsallDSrVIz0sU=\n"
"-----END RSA PRIVATE KEY-----";
SSLClientParameters mTLS = SSLClientParameters::fromPEM(my_cert, sizeof my_cert, my_key, sizeof my_key);
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
const char* broker = "mqtt.iotcreative.ru";
int port = 8883;
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
  mqtt.setServer(broker, port);
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