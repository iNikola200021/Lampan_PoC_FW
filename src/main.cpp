const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "Lampan-EVT2"
#define FW_VERSION      "4.0.0 alpha (WolfMQTT)"
#define ENABLE_MQTT_TLS
#define TINY_GSM_MODEM_SIM800
#define _TASK_STATUS_REQUEST
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoMatrix.h>
#include <TinyGsmClient.h>
#include <TaskScheduler.h>
#include <wolfMQTT.h>
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

#define DEFAULT_MQTT_HOST       "iot.eclipse.org" /* broker.hivemq.com */
#define DEFAULT_CMD_TIMEOUT_MS  30000
#define DEFAULT_CON_TIMEOUT_MS  5000
#define DEFAULT_MQTT_QOS        MQTT_QOS_0
#define DEFAULT_KEEP_ALIVE_SEC  60
#define DEFAULT_CLIENT_ID       "WolfMQTTClient"
#define WOLFMQTT_TOPIC_NAME     "wolfMQTT/example/"
#define DEFAULT_TOPIC_NAME      WOLFMQTT_TOPIC_NAME"testTopic"
#define MAX_BUFFER_SIZE         1024
#define TEST_MESSAGE            "test"
#define TEST_TOPIC_COUNT        2

#ifdef ENABLE_MQTT_TLS
static WOLFSSL_METHOD* mMethod = 0;
static WOLFSSL_CTX* mCtx       = 0;
static WOLFSSL* mSsl           = 0;
static const char* mTlsFile    = NULL;
#endif
static word16 mPort            = 0;
static const char* mHost       = "iot.eclipse.org";
static int mStopRead    = 0;
//Classes definition
TinyGsm modem(SerialAT);
TinyGsmClient modemclient(modem);
MqttClient mqttclient;
MqttNet net;

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
static int mqttclient_tls_verify_cb(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
  char buffer[WOLFSSL_MAX_ERROR_SZ];

  printf("MQTT TLS Verify Callback: PreVerify %d, Error %d (%s)\n", preverify,
         store->error, wolfSSL_ERR_error_string(store->error, buffer));
  printf("  Subject's domain name is %s\n", store->domain);

  if (store->error != 0) {
    /* Allowing to continue */
    /* Should check certificate and return 0 if not okay */
    printf("  Allowing cert anyways");
  }

  return 1;
}

static int mqttclient_tls_cb(MqttClient* cli)
{
  int rc = WOLFSSL_FAILURE;
  
  cli->tls.ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
  if (cli->tls.ctx) {
    wolfSSL_CTX_set_verify(cli->tls.ctx, SSL_VERIFY_PEER, mqttclient_tls_verify_cb);

    /* default to success */
    rc = WOLFSSL_SUCCESS;

    if (mTlsFile) {
      /* Load CA certificate file */
      rc = wolfSSL_CTX_load_verify_locations(cli->tls.ctx, mTlsFile, 0);
    }
  }
  else {
#if 0
    /* Load CA using buffer */
    rc = wolfSSL_CTX_load_verify_buffer(cli->tls.ctx, caCertBuf,
                                        caCertSize, WOLFSSL_FILETYPE_PEM);
#endif
    rc = WOLFSSL_SUCCESS;
  }

  printf("MQTT TLS Setup (%d)\n", rc);

  return rc;
}
static int SimConnect(void *context, const char* host, word16 port, int timeout_ms) 
{
  int ret = 0;

  ret = modemclient.connect(host, port);

  return ret;
}

static int SimRead(void *context, byte* buf, int buf_len, int timeout_ms) 
{
  int recvd = 0;
  /* While data and buffer available */
  while (modemclient.available() > 0 && recvd < buf_len) {
    buf[recvd] = modemclient.read();
    recvd++;
  }

  return recvd;
}

static int SimWrite(void *context, const byte* buf, int buf_len, int timeout_ms) 
{
  int sent = 0;

  sent = modemclient.write(buf, buf_len);

  return sent;
}

static int SimDisconnect(void *context) 
{
  modemclient.stop();

  return 0;
}
#define MAX_PACKET_ID   ((1 << 16) - 1)
static int mPacketIdLast;
static word16 mqttclient_get_packetid(void)
{
  mPacketIdLast = (mPacketIdLast >= MAX_PACKET_ID) ?
                  1 : mPacketIdLast + 1;
  return (word16)mPacketIdLast;
}

#define PRINT_BUFFER_SIZE    80
static int mqttclient_message_cb(MqttClient *client, MqttMessage *msg,
                                 byte msg_new, byte msg_done)
{
  byte buf[PRINT_BUFFER_SIZE + 1];
  word32 len;

  (void)client; /* Supress un-used argument */

  if (msg_new) {
    /* Determine min size to dump */
    len = msg->topic_name_len;
    if (len > PRINT_BUFFER_SIZE) {
      len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->topic_name, len);
    buf[len] = '\0'; /* Make sure its null terminated */

    /* Print incoming message */
    Serial.print("MQTT Message: Topic ");
    Serial.println((char*)buf);
    Serial.print("Qos ");
    Serial.println(msg->qos);
    Serial.print("Len ");
    Serial.println(msg->total_len);
  }

  /* Print message payload */
  len = msg->buffer_len;
  if (len > PRINT_BUFFER_SIZE) {
    len = PRINT_BUFFER_SIZE;
  }
  XMEMCPY(buf, msg->buffer, len);
  buf[len] = '\0'; /* Make sure its null terminated */
  Serial.print("Payload: ");
  Serial.println((char*)buf);

  if (msg_done) {
    Serial.println("MQTT Message: Done");
  }

  /* Return negative to terminate publish processing */
  return MQTT_CODE_SUCCESS;
}

bool mqttConnect()
{
    MqttConnect connect;
    memset(&connect, 0, sizeof(MqttConnect));
    connect.keep_alive_sec = keep_alive_sec;
    connect.clean_session = clean_session;
    connect.client_id = DeviceID;
    connect.username = mqtt_user;
    connect.password = mqtt_pass;
    MqttClient_Connect(&mqttclient, &connect);
      MqttSubscribe subscribe;
      MqttTopic topics[1], *topic;
      MqttPublish publish;

      memset(&publish, 0, sizeof(MqttPublish));
      publish.retain = 0;
      publish.qos = MQTT_QOS_0;
      publish.duplicate = 0;
      publish.topic_name = topicRegister;
      publish.packet_id = mqttclient_get_packetid();
      publish.buffer = (byte*)DeviceID;
      publish.total_len = (word16)strlen(DeviceID);
      MqttClient_Publish(&mqttclient, &publish);

      topics[0].topic_filter = topicStatus;
      topics[0].qos = MQTT_QOS_1;
      memset(&subscribe, 0, sizeof(MqttSubscribe));
      subscribe.packet_id = mqttclient_get_packetid();
      subscribe.topic_count = sizeof(topics)/sizeof(MqttTopic);
      subscribe.topics = topics;
      rc = MqttClient_Subscribe(&mqttclient, &subscribe);
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
  MqttQoS qos = DEFAULT_MQTT_QOS;
  byte clean_session = 1;
  word16 keep_alive_sec = 60;
  int enable_lwt = 0;
  byte tx_buf[MAX_BUFFER_SIZE];
  byte rx_buf[MAX_BUFFER_SIZE];
  MqttMsgCb msg_cb;
  net.connect = SimConnect;
  net.read = SimRead;
  net.write = SimWrite;
  net.disconnect = SimDisconnect;
  net.context = &modemclient;

  MqttClient_Init(&mqttclient, &net, mqttclient_message_cb, tx_buf, MAX_BUFFER_SIZE,
                       rx_buf, MAX_BUFFER_SIZE, DEFAULT_CMD_TIMEOUT_MS);

  int rc = MqttClient_NetConnect(&mqttclient, mHost, mPort,
                             DEFAULT_CON_TIMEOUT_MS, true,
                             mqttclient_tls_cb);
  ts.addTask(tNotification);
  ts.addTask(tRSSI);
  
  tRSSI.enable();
  // MQTT Broker setup
  SerialMon.println(F("SETUP"));
  matrix.fillRect(0,0,14,16,PBColour);
  matrix.show();
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