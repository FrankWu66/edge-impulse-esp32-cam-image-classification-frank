#include <ImageNet_person_detect2_inferencing.h>

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

// ------ 以下修改成你自己的WiFi帳號密碼 ------
const char* ssid = "iespmqtt";
const char* password = "12345678";

// ------ 以下修改成你MQTT設定 ------
//const char* mqtt_server = "mqtt.eclipseprojects.io";/
//const char* mqtt_server = "mqttgo.io";
const char* mqtt_server = "192.168.90.90";
const unsigned int mqtt_port = 1883;
#define CloudTopic    "frank/Clould_to_Edge"
#define EdgeTopic     "frank/Edge_to_Cloud"
uint32_t StartTimeMQTT;
uint32_t TotalTimeMQTT = 0;
uint32_t CycleMQTT = 0;

#define TFT_SCLK 14 // SCL
#define TFT_MOSI 13 // SDA
#define TFT_RST  12 // RES (RESET)
#define TFT_DC    2 // Data Command control pin
#define TFT_CS   15 // Chip select control pin
                    // BL (back light) and VCC -> 3V3

#define BTN       4 // button (shared with flash led)

#define BTN_CONTROL       false   // true: use button to capture and classify; false: loop to capture and classify.
#define ID_COUNT_PER_BTN  1     // identify count when press button, BTN_CONTROL need set to true; set to 1 as default
#define FAST_CLASSIFY     0
#define NOT_FAST_DELAY_TIME  3000  // delay time for continuous

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

dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

uint8_t bmp96x96header[54] = {0x42, 0x4D, 0x36, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x74, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

char clientId[50];
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength);
WiFiClient wifiClient;
PubSubClient mqttClient(mqtt_server, mqtt_port, mqtt_callback, wifiClient);

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
 
//MQTT callback for subscrib CloudTopic:"frank/Clould_to_Edge"
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength) {
  uint32_t EndTime, time_interval;

  CycleMQTT++;
  Serial.printf ("received from Cloud: [%s]\n", payload);
  EndTime = millis();
  Serial.printf("MQTT_picture() spend time: %d ms\n", EndTime - StartTimeMQTT);
  time_interval = EndTime - StartTimeMQTT;
  TotalTimeMQTT += time_interval;
  Serial.printf ("spend time (total): %d ms, count:%03d, avg time: %d ms\n\n", time_interval, CycleMQTT, TotalTimeMQTT/CycleMQTT);
}

//重新連線MQTT Server
boolean mqtt_nonblock_reconnect() {
  boolean doConn = false;
  if (! mqttClient.connected()) {
    Serial.printf("MQTT Client [%s] Connection LOST line 125 !\n", clientId);
    boolean isConn = mqttClient.connect(clientId);
    //boolean isConn = mqttClient.connect(clientId, MQTT_USER, MQTT_PASSWORD);
    Serial.printf("MQTT Client [%s] Connect %s !\n", clientId, (isConn ? "Successful" : "Failed"));
    // subscribe
    mqttClient.subscribe(CloudTopic);
  } else {
    Serial.printf("MQTT Client [%s] Connection OK 132 !\n", clientId);
  }
  return doConn;
}

//MQTT傳遞照片，每N秒傳一次？未到N秒：option1 - wait for it.  option2 - skip it. ??
void MQTT_picture() {
  uint32_t EndTime;
  char* logIsPublished;

  // loop for subscribe first...
  mqttClient.loop();

  StartTimeMQTT = millis();
  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client [%s] Connection LOST !\n", clientId);
    mqtt_nonblock_reconnect();
  }

  if (! mqttClient.connected())
    logIsPublished = "  No MQTT Connection, Photo NOT Published !";
  else {
    int imgSize = 56+(96*96*3);
    int ps = MQTT_MAX_PACKET_SIZE;
    // start to publish the picture
    mqttClient.beginPublish(EdgeTopic, imgSize, false);

    // send 96x96 bmp file header (56 bytes)
    mqttClient.write(bmp96x96header, sizeof(bmp96x96header));

    // send RGB888 raw data
    imgSize = 96*96*3;
    for (int i = 0; i < imgSize; i += ps) {
      int s = (imgSize - i < s) ? (imgSize - i) : ps;
      mqttClient.write((uint8_t *)(resized_matrix->item) + i, s);
    }

    boolean isPublished = mqttClient.endPublish();
    if (isPublished)
      logIsPublished = "  Publishing Photo to MQTT OK !";
    else
      logIsPublished = "  Publishing Photo to MQTT Failed !";
  }

  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client [%s] Connection LOST 179 !\n", clientId);
  }

  Serial.println(logIsPublished);
  EndTime = millis();
  Serial.printf("MQTT_picture() spend time: %d ms\n", EndTime - StartTimeMQTT);
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
  config.pixel_format = PIXFORMAT_RGB565;
  //config.pixel_format = PIXFORMAT_RGB565;  //FRAMESIZE_QQVGA  //FRAMESIZE_240X240
  //config.frame_size =  FRAMESIZE_240X240;
  config.frame_size =  FRAMESIZE_96X96;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, 0); // lower the saturation
  }

  // set interrupt service routine for button (GPIO 4), trigger: LOW/HIGH/CHANGE/RISING/FALLING, FALLING: when release button 
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr_Callback, FALLING);  

  Serial.println("Camera Ready!...(standby, press button to start)");
  //tft_drawtext(4, 4, "Standby", 1, TFT_BLUE);

  //啟動WIFI連線
  setup_wifi();
  sprintf(clientId, "ESP32CAM_%04X", random(0xffff));  // Create a random client ID
  //啟動MQTT連線
  mqtt_nonblock_reconnect();  
}

// main loop
void loop() {
  //uint32_t StartTime, EndTime;
  camera_fb_t *fb = NULL;

  mqtt_nonblock_reconnect(); 

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  mqttClient.loop();

  //Serial.println("Start show screen.");
  //StartTime = micros(); //millis();
  showScreen(fb, TFT_YELLOW);
  //EndTime = micros(); //millis();
  //Serial.printf("Show screen. spend time: %d ms\n", EndTime - StartTime);
  //Serial.printf("Show screen. spend time: %d.%d ms\n", (EndTime - StartTime)/1000, (EndTime - StartTime)%1000);

#if (ID_COUNT_PER_BTN == 1 || BTN_CONTROL == false)
  identify(fb);
  esp_camera_fb_return(fb);
#else
  esp_camera_fb_return(fb);
  multi_identify();
#endif
}

void showScreen(camera_fb_t *fb, uint16_t color) {
  //int StartTime, EndTime;
  tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)fb->buf);
}

void identify(camera_fb_t *fb) {
  uint32_t StartTime, EndTime;
  
  // capture a image and classify it
#if BTN_CONTROL == true  
  if (triggerClassify) {
#endif    
//    Serial.println("Start classify.");
    StartTime = millis();
    String result = classify(fb);
    EndTime = millis();
//    Serial.printf("End classify. spend time: %d ms\n", EndTime - StartTime);

    // display result
    //Serial.printf("Result: %s\n", result);
    Serial.printf("Result: "); Serial.print(result); Serial.printf(", %d ms\n", EndTime - StartTime);
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
  uint16_t delayTime = NOT_FAST_DELAY_TIME;
  Serial.printf("Finish classify, wait for %d ms to next loop.\n\n\n", delayTime);
  delay(delayTime);
#endif //#if FAST_CLASSIFY != 1
//  tft.fillScreen(TFT_BLACK);
//  Serial.println("Finish classify.\n");
//  tft.fillRoundRect (0, 128-16, 160, 16, 0, TFT_BLACK);  // clear string
#endif //#if BTN_CONTROL == true  
}

//
// In this function, independently run esp_camera_fb_get() and esp_camera_fb_return()
//
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

    // send pic via MQTT
    MQTT_picture();

    // edge impulse classify
    res = run_classifier(&signal, &result, false /* debug */);

    // --- Free memory ---
    dl_matrix3du_free(resized_matrix);
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
}

// classify labels
String classify(camera_fb_t * fb) {
//  int StartTime, EndTime;
  int index;
  uint16_t ResultColor = TFT_GREEN;

  // capture image from camera
  if (!capture(fb)) return "Error";
//  tft_drawtext(4, 4, "Classifying...", 1, TFT_CYAN /*ST77XX_CYAN*/);

//  Serial.println("  Getting image...");
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  // send pic via MQTT
  MQTT_picture();

  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client [%s] Connection LOST line 468 !\n", clientId);
  }

  // edge impulse classify
//  Serial.println("  Run classifier...");
  // Feed signal to the classifier
//  StartTime = millis();
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
//  EndTime = millis();
//  Serial.printf("  run_classifier() spend time: %d ms\n", EndTime - StartTime);
  // --- Free memory ---
  dl_matrix3du_free(resized_matrix);

  // --- Returned error variable "res" while data object.array in "result" ---
//  ei_printf("run_classifier returned: %d\n", res);
  if (res != 0) return "Error";

  // --- print the predictions ---
//  ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
//            result.timing.dsp, result.timing.classification, result.timing.anomaly);
//  int index;
  float score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    // record the most possible label
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
    ei_printf("    %s: %f;", result.classification[ix].label, result.classification[ix].value);
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

  // --- return the most possible label ---
  return String(result.classification[index].label);
}

// capture image from cam
bool capture(camera_fb_t * fb) {

  // --- Convert frame to RGB888  ---
  //Serial.println("Converting to RGB888...");
  // Allocate rgb888_matrix buffer
  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  //Serial.println("fmt2rgb888...");  
  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

  // 這邊轉化有問題…圖片都變1/4 x 4，還有亂碼、上一張的內容…改回上面的fmt2rgb888 (變正常)
  //Serial.println("rgb565_to_888...");  
  //for (uint16_t i=0; i < fb->len; i+=2) {
  //  rgb565_to_888( *((uint16_t *)fb->buf + i), (rgb888_matrix->item + (i>>1)*3) );
  //}

  resized_matrix = rgb888_matrix; // due to capture 96x96, no need to resize.

// --- Free memory ---
//  dl_matrix3du_free(rgb888_matrix);  // don't free rgb888_matrix due to it assign to resized_matrix and resized_matrix will be free in up function.

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
    r = resized_matrix->item[pixel_ix];
    g = resized_matrix->item[pixel_ix + 1];
    b = resized_matrix->item[pixel_ix + 2];

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