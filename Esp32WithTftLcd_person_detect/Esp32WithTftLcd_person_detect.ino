#include <ImageNet_person_detect2_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

//#include <person_detect_inferencing.h>
//#include <esp32-cam-cat-dog_3_label_inferencing.h>
//#include <esp32-cam-cat-dog_frank_inferencing.h>  // replace with your deployed Edge Impulse library

#define CAMERA_MODEL_AI_THINKER

#include "img_converters.h"
#include "image_util.h"
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include "SPI.h"
#include <TFT_eSPI.h>              // Hardware-specific library
//#include <TJpg_Decoder.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define FRANK_RED TFT_BLUE
#define FRANK_GREEN TFT_GREEN
#define FRANK_CYAN TFT_YELLOW
#define FRANK_BLUE TFT_ORANGE
#define FRANK_YELLOW TFT_CYAN

// Control macro
#define BTN_CONTROL       false   // true: use button to capture and classify; false: loop to capture and classify.
#define ID_COUNT_PER_BTN  1     // identify count when press button, BTN_CONTROL need set to true; set to 1 as default
#define AUTO_BATCH_3X3    true    // 
#define FAST_CLASSIFY     0
//#define ON_LINE           true

#define LOOP_DELAY_TIME   6000  // delay time for continuous
#define REDUCE_POLLING_TIME   true   // if receive MQTT subscribe...skip delay for next loop
#define LOOP_COUNT        100   // test count for per MQTT broker and topic option

// ------ 以下修改成你自己的WiFi帳號密碼 ------
const char* ssid = "iespmqtt";
const char* password = "12345678";

// ------ 以下修改成你MQTT設定 ------
#define MQTT_BROKER_LOCAL   "192.168.90.90"
#define MQTT_BROKER_MQTTGO  "mqttgo.io"
#define MQTT_BROKER_ECLIPSE "mqtt.eclipseprojects.io"

#define MQTT_PORT           1883

#define CLOUD_TOPIC         "frank/Clould_to_Edge"
// option 1: only send picture, do not classify 
#define EDGE_TOPIC_OP1_PIC  "frank/Edge_to_Cloud/Pic"
// option 2: do classify, send result message only
#define EDGE_TOPIC_OP2_CR   "frank/Edge_to_Cloud/ClassifyResult"
// option 3: do classify, only send person picture
#define EDGE_TOPIC_OP3_PP   "frank/Edge_to_Cloud/PicPerson"

char* mqtt_broker[3]        = {MQTT_BROKER_LOCAL, MQTT_BROKER_MQTTGO, MQTT_BROKER_ECLIPSE};
char* mqtt_EdgeTopic[3]     = {EDGE_TOPIC_OP1_PIC, EDGE_TOPIC_OP2_CR, EDGE_TOPIC_OP3_PP};
uint8_t BrokerIndex         = 0;
uint8_t TopicOptionIndex    = 0;
bool PersonDetect           = false;
bool ReportReceived         = false;

uint32_t StartTimeLoop;
uint32_t StartTimeMQTT;
uint32_t MqttSendTime;
uint32_t ClassifyTime;
uint32_t TotalTimeAll = 0;  // need clear after change broker or topic
uint32_t TotalTimeMQTT = 0; // need clear after change broker or topic
uint32_t CycleCount = 0;    // need clear after change broker or topic
uint32_t MqttCount = 0;     // need clear after change broker or topic

#define TFT_SCLK 14 // SCL
#define TFT_MOSI 13 // SDA
#define TFT_RST  12 // RES (RESET)
#define TFT_DC    2 // Data Command control pin
#define TFT_CS   15 // Chip select control pin
                    // BL (back light) and VCC -> 3V3

#define BTN       4 // button (shared with flash led)

#define SHOW_WIDTH  96
#define SHOW_HEIGHT 96
#define RGB565_SIZE SHOW_WIDTH*SHOW_HEIGHT*2

// after rotate 270 degrees
#define TFT_LCD_WIDTH   160
#define TFT_LCD_HEIGHT  128

typedef struct {
	uint32_t timing;
	float score[EI_CLASSIFIER_MAX_OBJECT_DETECTION_COUNT];
	uint8_t label[EI_CLASSIFIER_MAX_OBJECT_DETECTION_COUNT];
  uint8_t finallabel;
} MULTI_ID_RESULT;

/* Function definitions ------------------------------------------------------- */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;
void identify();
String classify();

//dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

//uint8_t bmp96x96header[54] = {0x42, 0x4D, 0x36, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// set BMP height to negative to convert image  offset 0x16 [60 00 00 00] - > [A0 FF FF FF]
uint8_t bmp96x96header[54] = {0x42, 0x4D, 0x36, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0xA0, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t RGB888BufferSize;
uint32_t RGB565BufferSize;
uint32_t BmpBufferSize;
uint8_t *BmpBuffer = NULL;  // for MQTT send BMP data to Cloud, never free it.
uint8_t *RGB888Buffer = NULL;  // for classify
uint8_t *RGB565Buffer = NULL;  // for TFT_eSPI tft.pushImage, never free it.
camera_fb_t * gfb = NULL;   // global fb, easy to use

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

int interruptPin = BTN;
bool triggerClassify = false;

TFT_eSPI tft = TFT_eSPI();         // Invoke custom library

// Interrupt Service Routine (IRS) callback function, declare as IRAM_ATTR means put it in RAM (increase meet rate)
// Note: don't know why... isr need locate above setup()
void IRAM_ATTR isr_Callback() {  
  // Only run simply code to avoid system crash
  triggerClassify = !triggerClassify;
  //Serial.printf("triggerClassify: %d \n", triggerClassify);
}

//啟動WIFI連線
void setup_wifi() {
  Serial.printf("\nConnecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi Connected.  IP Address: ");
  Serial.println(WiFi.localIP());
}

//重新連線MQTT Server
void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.printf("Attempting MQTT connection...broker: %s", mqtt_broker[BrokerIndex]);
    // Create a random client ID
    String clientId = "ESP32CAM_Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.printf("connected (%s)\n", clientId.c_str());
      // re-subscribe
      mqttClient.subscribe(CLOUD_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 1 seconds");
      // Wait 1 seconds before retrying
      delay(1000);
    }
  }
}

//Report loop date, separate here for called by option3 + non-person
void ReportLoop (bool skipMQTT) {
  uint32_t EndTime=0, TimeIntervalMQTT=0, TimeIntervalAll=0, AvgTimeMQTT;

  if (MqttCount == 0) {
    AvgTimeMQTT = 0;
  } else {
    AvgTimeMQTT = TotalTimeMQTT/MqttCount;
  }
  EndTime = millis();
  if (skipMQTT != true) {
    TimeIntervalMQTT = EndTime - StartTimeMQTT;
    TotalTimeMQTT += TimeIntervalMQTT;
  }
  TimeIntervalAll = EndTime - StartTimeLoop;
  TotalTimeAll += TimeIntervalAll;
  Serial.printf ("    <loop %03d> spend time [loop]: %d ms, avg time: %d ms; [MQTT]: %d ms, avg time: %d ms. [MQTT consume rate] %.4f\n", 
                    CycleCount, TimeIntervalAll, TotalTimeAll/CycleCount, TimeIntervalMQTT, AvgTimeMQTT, (float)TotalTimeMQTT/TotalTimeAll );
  Serial.printf("    #CSV: %d,%d,%d,%d,%d,%d,%d,%d,%d,%.4f\n\n",
                  BrokerIndex,TopicOptionIndex,CycleCount,TimeIntervalAll,ClassifyTime,TimeIntervalMQTT,MqttSendTime,TotalTimeAll/CycleCount,AvgTimeMQTT,(float)TotalTimeMQTT/TotalTimeAll);
  ReportReceived = true;
  CycleCount++;
}

//MQTT callback for subscrib CLOUD_TOPIC:"frank/Clould_to_Edge"
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength) {
  MqttCount++;
  Serial.print("    Received from Cloud : [");
  Serial.print(topic);
  Serial.print("] msg: <<");
  for (int i = 0; i < msgLength; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println(">>");
  ReportLoop(false);
}

//MQTT傳遞照片，每N秒傳一次？未到N秒：option1 - wait for it.  option2 - skip it. ??
void MQTT_picture() {
  uint32_t EndTime;
  char* logIsPublished;

  StartTimeMQTT = millis();
  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client Connection LOST %d!\n", __LINE__);
    mqtt_reconnect();
  }

  if (! mqttClient.connected())
    logIsPublished = "  No MQTT Connection, Photo NOT Published !";
  else {
    int imgSize = BmpBufferSize;
    //int ps = (80 > MQTT_MAX_PACKET_SIZE) ? MQTT_MAX_PACKET_SIZE : 80;
    int ps = MQTT_MAX_PACKET_SIZE;
    int SendSize = 0;
    // start to publish the picture
    mqttClient.beginPublish(mqtt_EdgeTopic[TopicOptionIndex], imgSize, false);

    /*
    // send 96x96 bmp file header (56 bytes)
    mqttClient.write(bmp96x96header, sizeof(bmp96x96header));

    // send RGB888 raw data
    imgSize = 96*96*3;
    */
    for (int i = 0; i < imgSize; i += ps) {
      SendSize = (imgSize - i < ps) ? (imgSize - i) : ps;
      mqttClient.write(BmpBuffer + i, SendSize);
    }

    boolean isPublished = mqttClient.endPublish();
    if (isPublished)
      logIsPublished = "MQTT_picture() Publishing OK !";
    else
      logIsPublished = "MQTT_picture() Publishing Failed !";
    
  }
  Serial.print(logIsPublished);

  EndTime = millis();
  MqttSendTime = EndTime - StartTimeMQTT;
  Serial.printf("   Spend time: %d ms\n", MqttSendTime);
}

void MQTT_picture_JPG() {
  uint32_t EndTime;
  char* logIsPublished;

  StartTimeMQTT = millis();
  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client Connection LOST %d!\n", __LINE__);
    mqtt_reconnect();
  }

  if (! mqttClient.connected())
    logIsPublished = "  No MQTT Connection, Photo NOT Published !";
  else {
    int imgSize = gfb->len;
    int ps = MQTT_MAX_PACKET_SIZE;
    int SendSize = 0;
    // start to publish the picture
    mqttClient.beginPublish(mqtt_EdgeTopic[TopicOptionIndex], imgSize, false);

    for (int i = 0; i < imgSize; i += ps) {
      SendSize = (imgSize - i < ps) ? (imgSize - i) : ps;
      mqttClient.write((uint8_t *)(gfb->buf) + i, SendSize);
    }

    boolean isPublished = mqttClient.endPublish();
    if (isPublished)
      logIsPublished = "MQTT_picture_JPG() Publishing OK !";
    else
      logIsPublished = "MQTT_picture_JPG() Publishing Failed !";
    
  }
  Serial.print(logIsPublished);

  EndTime = millis();
  MqttSendTime = EndTime - StartTimeMQTT;
  Serial.printf("   Spend time: %d ms\n", MqttSendTime);
}

// setup
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);

#if BTN_CONTROL == true  
  // button
  pinMode(BTN, INPUT);
#endif

  // Initialise the TFT
  tft.init();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setRotation(3);//1:landscape 3:inv. landscape

  // cam config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //config.pixel_format = PIXFORMAT_RGB565; // PIXFORMAT_RGB888 (x) // PIXFORMAT_JPEG
  // Note: only support: PIXFORMAT_GRAYSCALE / PIXFORMAT_GRAYSCALE / PIXFORMAT_RGB565 / PIXFORMAT_JPEG
  //config.frame_size =  FRAMESIZE_96X96; //FRAMESIZE_QQVGA (160x120)  //FRAMESIZE_240X240 //FRAMESIZE_VGA(640x480)
  config.frame_size =  FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
//  s->set_hmirror(s, 1);//水平翻轉:(0或1)
//  s->set_vflip(s, 1);//垂直翻轉:(0或1)
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, 0); // lower the saturation
  }

  //RGB888BufferSize = 240*240*3;
  RGB888BufferSize = 640*480*3;
  RGB565BufferSize = 96*96*2;
  BmpBufferSize = 96*96*3 + 54;
  // for MQTT send BMP data to Cloud, never free it.
  BmpBuffer = (uint8_t *) malloc(54+RGB888BufferSize);
  RGB888Buffer = BmpBuffer + 54;
  // for TFT_eSPI tft.pushImage, never free it.
  RGB565Buffer = (uint8_t *) malloc(96*96*2);

  // fill BMP header to BmpBuffer
  memcpy (BmpBuffer, bmp96x96header, sizeof(bmp96x96header));

  // set interrupt service routine for button (GPIO 4), trigger: LOW/HIGH/CHANGE/RISING/FALLING, FALLING: when release button 
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr_Callback, FALLING);  

  Serial.println("Camera Ready!...(standby, press button to start)");
  //tft_drawtext(4, 4, "Standby", 1, TFT_BLUE);

  //啟動WIFI連線
  setup_wifi();
  mqttClient.setServer(mqtt_broker[BrokerIndex], MQTT_PORT);
  mqttClient.setCallback(mqtt_callback);

  // ########### setup 3(broker) x 3(option topic) from here ###########
  BrokerIndex      = 0;
  TopicOptionIndex = 0;
  TotalTimeAll = 0;  // need clear after change broker or topic
  TotalTimeMQTT = 0; // need clear after change broker or topic
  CycleCount = 1;    // need clear after change broker or topic
  MqttCount = 0;     // need clear after change broker or topic

  Serial.printf("Wait for %d seconds to start tasks\n", 10);
  for (int i=1; i<=10; i++) {
    Serial.printf("%d - ", i);
    delay(1000);
  }
  Serial.println("\n#CSV: BrokerIndex,TopicOptionIndex,loop,loop time,classify time,MQTT time,MQTT send time,loop avg. time,MQTT avg. time,MQTT consume rate");
  Serial.println("\n");
}

// main loop
void loop() {
  //uint32_t StartTime, EndTime;

  // clear camera buffer?
  gfb = esp_camera_fb_get();
  esp_camera_fb_return(gfb);

  //camera_fb_t *fb = NULL;
  gfb = esp_camera_fb_get();
  if (!gfb) {
    Serial.println("Camera capture failed");
    return;
  }

  //Serial.println("Start show screen.");
  //StartTime = micros(); //millis();
  if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, RGB888Buffer) == false) {
    Serial.println ("\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! capture(fb) error !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n");
    esp_camera_fb_return(gfb);
    return;
  }  

  //showScreen(fb, TFT_YELLOW);
  tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)RGB565Buffer);

  //EndTime = micros(); //millis();
  //Serial.printf("Show screen. spend time: %d ms\n", EndTime - StartTime);
  //Serial.printf("Show screen. spend time: %d.%d ms\n", (EndTime - StartTime)/1000, (EndTime - StartTime)%1000);

// Note (2024/5/23): in this phase, I don't need to run this part, so comment it and do not debug/modify for  multi_identify()
/*
#if (ID_COUNT_PER_BTN == 1 || BTN_CONTROL == false)
  identify(fb);
  esp_camera_fb_return(fb);
#else
  esp_camera_fb_return(fb);  // clear first is requirement!
  multi_identify();
#endif
*/

/*
  // continue classify and send pic to cloud, delay LOOP_DELAY_TIME
  identify();
  MQTT_picture();
  delay(1000);
  MQTT_picture_JPG();
  delay(1000);
*/

  // ########### process 3(broker) x 3(option topic) from here ###########
  //CycleCount++;  // move to report method, make sure not miss time counting
  ReportReceived = false;

  // block when all task done.
  while (BrokerIndex >= 2 && TopicOptionIndex >= 2 && CycleCount > LOOP_COUNT) {
    Serial.printf("## 3 broker x 3 topic option are all done.\nReset ESP32 CAM for next test!\n\n");
    delay(10000);
  }

  // switch task here
  if (CycleCount > LOOP_COUNT) {
    if (TopicOptionIndex >= 2) {
      BrokerIndex++;
      TopicOptionIndex = 0;
      // disconnect MQTT broker!
      mqttClient.disconnect();
      delay (10);
    } else {
      TopicOptionIndex++;
    }

    TotalTimeAll = 0;  // need clear after change broker or topic
    TotalTimeMQTT = 0; // need clear after change broker or topic
    CycleCount = 1;    // need clear after change broker or topic
    MqttCount = 0;     // need clear after change broker or topic

    mqttClient.setServer(mqtt_broker[BrokerIndex], MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
  }

  if (!mqttClient.connected()) {
    mqtt_reconnect();
  }

  MqttSendTime = 0;
  ClassifyTime = 0;
  StartTimeLoop = millis();
  //showScreen(fb, TFT_YELLOW);

  // loop # start
  Serial.printf("=[Loop #%03d for Broker: %s, Topic: %s, (topic option:%d)]=\n", CycleCount, mqtt_broker[BrokerIndex], mqtt_EdgeTopic[TopicOptionIndex], TopicOptionIndex + 1);
  // move to up //if (!capture(fb)) {
  //  Serial.println ("\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! capture(fb) error !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n");
  //  return;
  //}

  switch (TopicOptionIndex) {
    case 0:   // option 1: only send picture, do not classify
      MQTT_picture_JPG();
      break;
    case 1:   // option 2: do classify, send result message only
      identify();
      StartTimeMQTT = millis();
      if (PersonDetect == true) {
        mqttClient.publish(mqtt_EdgeTopic[TopicOptionIndex], "PersonDetect");
      } else {
        mqttClient.publish(mqtt_EdgeTopic[TopicOptionIndex], "Not Person");
      }
      MqttSendTime = millis() - StartTimeMQTT;
      break;
    case 2:   // option 3: do classify, only send person picture
      identify();
      // workaround here, force 50% person
      //PersonDetect = (CycleCount % 2 == 1);
      if (PersonDetect == true) {
        MQTT_picture_JPG();
      } else {
        // don't run MQTT, run ReportLoop directly
        ReportLoop (true); // true: skipMQTT  
      }
      break;    
  }

  // MUST free gfb here due to MQTT_picture_JPG need read raw data from gfb
  esp_camera_fb_return(gfb);

  // delay for next loop & receive mqtt ... 
  Serial.printf("Finish loop %d, wait for %d ms to next loop.\n", CycleCount, LOOP_DELAY_TIME);
  long tt,now;
  tt = millis();
  now = tt;
  while (now - tt < LOOP_DELAY_TIME) {
    mqttClient.loop();  // spend around 43 us.
    delay(10);  // delay 10 ms to prevent do too many time of mqttClient.loop()
    now = millis();
    if (REDUCE_POLLING_TIME == true && ReportReceived == true) {
      delay(10); // skip wait for next loop since receive/finish MQTT report.
      break;
    }
  } // while for delay

} // loop

/*
void showScreen(camera_fb_t *fb, uint16_t color) {
  //int StartTime, EndTime;
  //tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)fb->buf);
  tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)RGB565Buffer);
}
*/

void identify(/* camera_fb_t *fb */) {
  uint32_t StartTime, EndTime;
  
  // capture a image and classify it
#if BTN_CONTROL == true
  if (triggerClassify) {
#endif    
//    Serial.println("Start classify.");
    StartTime = millis();
    String result = classify(/*fb*/);
    EndTime = millis();
//    Serial.printf("End classify. spend time: %d ms\n", EndTime - StartTime);

    // display result
    //Serial.printf("Result: %s\n", result);
    ClassifyTime = EndTime - StartTime;
    Serial.printf("End classify. Result: <<###############    "); Serial.print(result); Serial.printf("    ###############>>,  spend time: %d ms\n", ClassifyTime);
    //tft_drawtext(4, 128 - 8, result, 1, TFT_GREEN /*ST77XX_GREEN*/);

#if BTN_CONTROL == true  
    // wait for next press button to continue show screen
    while (triggerClassify){
      delay (200);
      //Serial.printf("while triggerClassify: %d \n", triggerClassify);
    }
    tft.fillScreen(TFT_BLACK);
    //delay(1000);
  }
#else //#if BTN_CONTROL == true  
  //delay for next loop.
#if FAST_CLASSIFY != 1
/*
  uint16_t delayTime = LOOP_DELAY_TIME;
  Serial.printf("Finish loop %d, wait for %d ms to next loop.\n", CycleCount, delayTime);
  long tt,now;
  tt = millis();
  now = tt;
  while (now - tt < delayTime) {
    mqttClient.loop();  // spend around 43 us.
    delay(10);  // delay 10 ms to prevent do too many time of mqttClient.loop()
    now = millis();
  }
*/
  //delay(delayTime);
#endif //#if FAST_CLASSIFY != 1
//  tft.fillScreen(TFT_BLACK);
//  Serial.println("Finish classify.\n");
//  tft.fillRoundRect (0, 128-16, 160, 16, 0, TFT_BLACK);  // clear string
#endif //#if BTN_CONTROL == true  
}

//
// In this function, independently run esp_camera_fb_get() and esp_camera_fb_return()
//
/*
void multi_identify() {
  uint32_t Index;
  camera_fb_t *fb = NULL;
  signal_t signal;
  EI_IMPULSE_ERROR res;
  MULTI_ID_RESULT MultiIdResult[ID_COUNT_PER_BTN] = {0};
  MULTI_ID_RESULT TotalResult;
  int LabelIndex;
  float TmpSocre;
  uint8_t tmp;

  //ei_printf("size of MultiIdResult: %d, size of TotalResult: %d\n", sizeof(MultiIdResult), sizeof(TotalResult));
  memset (&TotalResult, 0, sizeof(TotalResult));

  if (!triggerClassify) {
    return;
  }

// capture a image and classify it
  Serial.printf("Start classify %d times. label count: %d\n", ID_COUNT_PER_BTN, EI_CLASSIFIER_LABEL_COUNT);
  tft_drawtext(4, 4, "Start classify " + String(ID_COUNT_PER_BTN) + " times.", 1, FRANK_BLUE);
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  for (Index = 0; Index < ID_COUNT_PER_BTN; Index++) {
    StartTimeLoop = millis();

    // loop for subscribe first...
    mqttClient.loop();

    //String result = classify(fb);
    fb = esp_camera_fb_get();
    if (!capture(fb)) {
      Serial.printf("capture fail, abort multi_identify.\n");
      esp_camera_fb_return(fb);
      return;
    }
    showScreen(fb, TFT_YELLOW);
    signal.get_data = &raw_feature_get_data;

    // edge impulse classify
    res = run_classifier(&signal, &result, false);  // false for debug

    // --- Free memory ---
  //  dl_matrix3du_free(resized_matrix);
    if (res != 0) {
      Serial.printf("error occur in identify index: %d\n", Index);
      continue;
    }

    // collect data
    MultiIdResult[Index].timing = result.timing.classification;
    TotalResult.timing += result.timing.classification;
    TmpSocre = 0.0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      MultiIdResult[Index].score[ix] = result.classification[ix].value;
      TotalResult.score[ix] += result.classification[ix].value;
      // record the most possible label
      if (result.classification[ix].value > TmpSocre) {
        TmpSocre = result.classification[ix].value;
        LabelIndex = ix;
      }
    } // for (size_t ix = 0;
    MultiIdResult[Index].label[LabelIndex] = 1;
    MultiIdResult[Index].finallabel = LabelIndex;
    TotalResult.label[LabelIndex] += 1;

    esp_camera_fb_return(fb);
    // clean text in the bottom of tft
    //tft.drawRect (0, 128-16, 160, 16, TFT_BLACK),
    tft.fillRoundRect (0, 128-16, 160, 16, 0, TFT_BLACK);
    tft_drawtext(4, 128 - 16, String(result.classification[0].label) + ":" + String(TotalResult.label[0]) + ","
                             + String(result.classification[1].label) + ":" + String(TotalResult.label[1])
#if EI_CLASSIFIER_LABEL_COUNT > 2
                       + "," + String(result.classification[2].label) + ":" + String(TotalResult.label[2])
#endif
                             , 1, FRANK_BLUE);
    if (Index % 10 == 0) {Serial.printf("  [%02d] :", Index);}
    //Serial.print("*");
    Serial.printf("%d", LabelIndex);

    // send pic via MQTT
    MQTT_picture();
  } // for (Index = 0;

  tmp = 0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (TotalResult.label[ix] > tmp) {
      tmp = TotalResult.label[ix];
      TotalResult.finallabel = ix;
    }
  }
  

  // print total score and result
  Serial.printf("\nEnd classify.\nTotal:\n\tavg. identify spend time: %d ms\n", TotalResult.timing/ID_COUNT_PER_BTN);
#if EI_CLASSIFIER_LABEL_COUNT == 2
  Serial.printf("\tidentify as: %s (%s: %d, %s: %d)\n", result.classification[TotalResult.finallabel].label, 
                          result.classification[0].label, TotalResult.label[0],
                          result.classification[1].label, TotalResult.label[1]);
  Serial.printf("\taccuracy: %.2f\n", (float)TotalResult.label[TotalResult.finallabel]/ID_COUNT_PER_BTN);
  Serial.printf("\ttotal avg. score: %s: %.2f, %s: %.2f\n",
                          result.classification[0].label, TotalResult.score[0]/ID_COUNT_PER_BTN,
                          result.classification[1].label, TotalResult.score[1]/ID_COUNT_PER_BTN);
#else //#if EI_CLASSIFIER_LABEL_COUNT == 2
  Serial.printf("\tidentify as: %s (%s: %d, %s: %d, %s: %d)\n", result.classification[TotalResult.finallabel].label, 
                          result.classification[0].label, TotalResult.label[0],
                          result.classification[1].label, TotalResult.label[1],
                          result.classification[2].label, TotalResult.label[2]);
  Serial.printf("\taccuracy: %.2f\n", (float)TotalResult.label[TotalResult.finallabel]/ID_COUNT_PER_BTN);
  Serial.printf("\ttotal avg. score: %s: %.2f, %s: %.2f, %s: %.2f\n",
                          result.classification[0].label, TotalResult.score[0]/ID_COUNT_PER_BTN,
                          result.classification[1].label, TotalResult.score[1]/ID_COUNT_PER_BTN,
                          result.classification[2].label, TotalResult.score[2]/ID_COUNT_PER_BTN);
#endif //#if EI_CLASSIFIER_LABEL_COUNT == 2

  // wait for next press button to continue show screen
  while (triggerClassify){
    delay (200);
  }
  tft.fillScreen(TFT_BLACK);
} // void multi_identify() {
*/

// classify labels
String classify(/* camera_fb_t * fb */) {
  int StartTime, EndTime;
  int index;
  uint16_t ResultColor = TFT_GREEN;

  // capture image from camera
  //if (!capture(fb)) return "Error";
//  tft_drawtext(4, 4, "Classifying...", 1, TFT_CYAN /*ST77XX_CYAN*/);

//  Serial.println("  Getting image...");
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  // edge impulse classify
//  Serial.println("  Run classifier...");
  // Feed signal to the classifier
  //StartTime = millis();
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
  //EndTime = millis();
  //Serial.printf("  run_classifier() spend time: %d ms\n", EndTime - StartTime);

  // --- Free memory ---
//  dl_matrix3du_free(resized_matrix);

  // --- Returned error variable "res" while data object.array in "result" ---
//  ei_printf("run_classifier returned: %d\n", res);
  if (res != 0) return "Error";

  // --- print the predictions ---
//  ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
//            result.timing.dsp, result.timing.classification, result.timing.anomaly);
//  int index;
  float score = 0.0;
  ei_printf("Classify score:");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    // record the most possible label
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
    ei_printf("  # %s: %f;", result.classification[ix].label, result.classification[ix].value);
//    tft_drawtext(4, 12 + 8 * ix, String(result.classification[ix].label) + " " + String(result.classification[ix].value * 100) + "%", 1, TFT_ORANGE /*ST77XX_ORANGE*/);
  }
  ei_printf("\n");

  // show score
  tft.fillRoundRect (0, 128-16, 160, 16, 0, TFT_BLACK);
  tft_drawtext(4, 128 - 16, String(result.classification[0].label, 3) + ":" + String(result.classification[0].value, 4) + ", "
                            + String(result.classification[1].label, 3) + ":" + String(result.classification[1].value, 4)
#if EI_CLASSIFIER_LABEL_COUNT > 2
                      + ", " + String(result.classification[2].label, 3) + ":" + String(result.classification[2].value, 4)
#endif
                            , 1, TFT_GOLD);


  switch (index) {
    case 0:
      ResultColor = FRANK_GREEN;
      tft.drawRect(0, 0, 158, 127, TFT_BLACK);   
      break;
    case 1:
      ResultColor = FRANK_YELLOW;
      tft.drawRect(0, 0, 158, 127, FRANK_RED); 
      break;
    case 2:
      ResultColor = FRANK_CYAN;
      tft.drawRect(0, 0, 158, 127, FRANK_GREEN);
      break;
  }

  // show result
  tft_drawtext(4, 128 - 8, String(result.classification[index].label), 1, ResultColor);


#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("    anomaly score: %f\r\n", result.anomaly);
#endif

  // index 1 is person
  if (index == 1) {
    PersonDetect = true;
  } else {
    PersonDetect = false;
  }

  // --- return the most possible label ---
  return String(result.classification[index].label);
}

// 2024/5/26 discard this function due to change to jpg format
// capture image from cam
bool capture(camera_fb_t * fb) {

  // --- Convert frame to RGB888  ---
  //Serial.println("Converting to RGB888...");
  // Allocate rgb888_matrix buffer
//  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  //Serial.println("fmt2rgb888...");  
//  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);  // 不明原因圖片反轉！ -> BMP檔本來就反轉…

  // 這邊轉化有問題…圖片都變1/4 x 4，還有亂碼、上一張的內容…改回上面的fmt2rgb888 (變正常)
  //Serial.println("rgb565_to_888...");  
  //Serial.printf("fb->len %d !\n", fb->len);  // len=18432=96x96x2
  /*
  for (uint16_t i=0; i < fb->len; i+=2) {
    rgb565_to_888( *((uint16_t *)(fb->buf + i)), (rgb888_matrix->item + (i>>1)*3) );  //仍然反轉？ MQTT問題？ -> BMP檔本來就反轉…
  }
  */

//  resized_matrix = rgb888_matrix; // due to capture 96x96, no need to resize.

// --- Free memory ---
//  dl_matrix3du_free(rgb888_matrix);  // don't free rgb888_matrix due to it assign to resized_matrix and resized_matrix will be free in up function.

/* because ESP32 not support PIXFORMAT_RGB888 ... so discard this region
  // 1. copy fb to RGB888Buffer
  if (RGB888BufferSize != fb->len) {
    Serial.printf("!!! RGB888BufferSize:%d != fb->len:%d !!!\n", RGB888BufferSize, fb->len);
  }
  memcpy(RGB888Buffer, fb->buf, fb->len);

  // 2. transfer RGB888 to RGB565Buffer
  for (uint16_t i=0; i < RGB888BufferSize; i+=3) {
    rgb888_to_565( ((uint16_t *)RGB565Buffer + i/3), RGB888Buffer[i], RGB888Buffer[i+1], RGB888Buffer[i+2] );
  }
*/

  // 1. copy fb to RGB565Buffer
  if (RGB565BufferSize != fb->len) {
    Serial.printf("!!! RGB565BufferSize:%d != fb->len:%d !!!\n", RGB565BufferSize, fb->len);
  }
  memcpy(RGB565Buffer, fb->buf, fb->len);

  // 2. transfer RGB888 to RGB888Buffer
  for (uint16_t i=0; i < RGB565BufferSize; i+=2) {
    rgb565_to_888( *((uint16_t *)(RGB565Buffer + i)), (RGB888Buffer + (i>>1)*3) );  //仍然反轉？ MQTT問題？ -> BMP檔本來就反轉…
  }

  return true;
}

/**
 * @brief      Capture, rescale and crop image
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  out_buf       pointer to store output image, NULL may be used
 *                           if ei_camera_frame_buffer is to be used for capture and resize/cropping.
 *
 * @retval     false if not initialised, image captured, rescaled or cropped failed
 *
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    /*
    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }
    */

   bool converted = fmt2rgb888(gfb->buf, gfb->len, PIXFORMAT_JPEG, out_buf);

   //esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != gfb->width)
        || (img_height != gfb->height)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        gfb->width,
        gfb->height,
        out_buf,
        img_width,
        img_height);
    }

  // 2. transfer RGB888 to RGB565Buffer
  for (uint16_t i=0; i < img_width*img_height*3; i+=3) {
    rgb888_to_565( ((uint16_t *)RGB565Buffer + i/3), RGB888Buffer[i], RGB888Buffer[i+1], RGB888Buffer[i+2] );
  }

    return true;
}

int raw_feature_get_data(size_t offset, size_t out_len, float *signal_ptr) {

  size_t pixel_ix = offset * 3;
  size_t bytes_left = out_len;
  size_t out_ptr_ix = 0;

  // read byte for byte
  while (bytes_left != 0) {
    // grab the values and convert to r/g/b
    uint8_t r, g, b;
    /*
    r = resized_matrix->item[pixel_ix];
    g = resized_matrix->item[pixel_ix + 1];
    b = resized_matrix->item[pixel_ix + 2];
    */
    r = RGB888Buffer[pixel_ix];
    g = RGB888Buffer[pixel_ix + 1];
    b = RGB888Buffer[pixel_ix + 2];

    // then convert to out_ptr format
    float pixel_f = (r << 16) + (g << 8) + b;
    signal_ptr[out_ptr_ix] = pixel_f;

    // and go to the next pixel
    out_ptr_ix++;
    pixel_ix += 3;
    bytes_left--;
  }

  return 0;
}

// draw test on TFT
void tft_drawtext(int16_t x, int16_t y, String text, uint8_t font_size, uint16_t color) {
  tft.setCursor (x, y, font_size);
  tft.setTextColor(color);
  tft.print(strcpy(new char[text.length() + 1], text.c_str()));
}
/*
#define FRANK_RED TFT_BLUE
#define FRANK_GREEN TFT_GREEN
#define FRANK_CYAN TFT_YELLOW
#define FRANK_BLUE TFT_ORANGE
#define FRANK_YELLOW TFT_CYAN
*/