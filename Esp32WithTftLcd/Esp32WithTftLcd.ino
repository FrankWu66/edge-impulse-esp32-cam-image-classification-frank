/*
  Live Image Classification on ESP32-CAM and ST7735 TFT display 
  using MobileNet v1 from Edge Impulse
  Modified from https://github.com/edgeimpulse/example-esp32-cam.

  Note: 
  The ST7735 TFT size has to be at least 120x120.
  Do not use Arduino IDE 2.0 or you won't be able to see the serial output!
*/


/*
Note: Modify below Library file:
1. C:\Users\{account}\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
<only remain below, comment others (musb comment!)
#define USER_SETUP_INFO "User_Setup"
#define ST7735_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 160
#define ST7735_GREENTAB
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts
#define SMOOTH_FONT
#define SPI_FREQUENCY  27000000

2. C:\Users\{account}\Documents\Arduino\libraries\TJpg_Decoder\src\TJpg_Decoder.h
<insert below line 104: "bool _swap = false;">
  void setFormat(uint8_t jdFormat);
  uint8_t  _jdFormat = JD_FORMAT;

3. C:\Users\{account}\Documents\Arduino\libraries\TJpg_Decoder\src\TJpg_Decoder.cpp
<insert below line 36: "void TJpg_Decoder::setSwapBytes(bool swapBytes){">
// for user to change JD_FORMAT
// Specifies output pixel format.
//  0: RGB888 (24-bit/pix)
//  1: RGB565 (16-bit/pix)
//  2: Grayscale (8-bit/pix)
//
void TJpg_Decoder::setFormat(uint8_t jdFormat){
  _jdFormat = jdFormat;
}

<inser below line 525: jdec.swap = _swap;>
  jdec.jdFormat = _jdFormat;

4. C:\Users\{account}\Documents\Arduino\libraries\TJpg_Decoder\src\tjpgd.c
<Modify line 934: "if (JD_FORMAT == 1) {" as below>
	// Convert RGB888(0) to RGB565(1) if needed 
	//if (JD_FORMAT == 1) {
  if (jd->jdFormat == 1) {

<Modify line 979: "jd_prepare" as below>
...
  uint8_t tmp = jd->swap; // Copy the swap flag
  uint8_t tmp2 = jd->jdFormat;
...
  jd->swap = tmp; // Restore the swap flag
  jd->jdFormat = tmp2;

5. C:\Users\{account}\Documents\Arduino\libraries\TJpg_Decoder\src\tjpgd.h
<inser below line 89: uint8_t swap;>
	uint8_t jdFormat;


*/

#include <esp32-cam-cat-dog_frank_inferencing.h>  // replace with your deployed Edge Impulse library

#define CAMERA_MODEL_AI_THINKER

#include "img_converters.h"
#include "image_util.h"
#include "esp_camera.h"
#include "camera_pins.h"

//#include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

#include "SPI.h"
#include <TFT_eSPI.h>              // Hardware-specific library
//#include <TJpg_Decoder.h>

#define TFT_SCLK 14 // SCL
#define TFT_MOSI 13 // SDA
#define TFT_RST  12 // RES (RESET)
#define TFT_DC    2 // Data Command control pin
#define TFT_CS   15 // Chip select control pin
                    // BL (back light) and VCC -> 3V3

#define BTN       4 // button (shared with flash led)
#define BTN_CONTROL false   // true: use button to capture and classify; false: loop to capture and classify.
#define FAST_CLASSIFY 1

#define SHOW_WIDTH  96
#define SHOW_HEIGHT 96
#define RGB565_SIZE SHOW_WIDTH*SHOW_HEIGHT*2

// after rotate 270 degrees
#define TFT_LCD_WIDTH   160
#define TFT_LCD_HEIGHT  128

dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
//uint16_t *rgb565 = (uint16_t *) malloc(RGB565_SIZE);  // for swap pixel data from fb->buf.

//uint16_t *tmpDecordedRgb565 = NULL;  // for JpegDec decord_output use.

int interruptPin = BTN;
bool triggerClassify = false;

/*
uint16_t  dmaBuffer1[16 * 16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t  dmaBuffer2[16 * 16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t* dmaBufferPtr = dmaBuffer1;
bool dmaBufferSel = 0;
*/

TFT_eSPI tft = TFT_eSPI();         // Invoke custom library

/*
// This next function will be called during decoding of the jpeg file to render each
// 16x16 or 8x8 image tile (Minimum Coding Unit) to the TFT.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;
  if (dmaBufferSel) dmaBufferPtr = dmaBuffer2;
  else dmaBufferPtr = dmaBuffer1;
  dmaBufferSel = !dmaBufferSel; // Toggle buffer selection
  //  pushImageDMA() will clip the image block at screen boundaries before initiating DMA
  tft.pushImageDMA(x, y, w, h, bitmap, dmaBufferPtr); // Initiate DMA - blocking only if last DMA is not complete
  // The DMA transfer of image block to the TFT is now in progress...
  //outputIndex++;
  //Serial.printf("    tft_output:%d , x: %d, y: %d, w: %d, h:%d, memcpy size: %d\n", outputIndex, x, y, w, h);
  // Return 1 to decode next block.
  return 1;
}

bool decord_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  //if ( y >= tft.height() ) {Serial.printf("  tft.height():%d", tft.height());return 0;  }
  //tft.pushImageDMA(x, y, w, h, bitmap, dmaBufferPtr); // Initiate DMA - blocking only if last DMA is not complete

  //if (x > 0) return 1; 

  if (tmpDecordedRgb565 == NULL) return 0;

  outputIndex++;
  //each call this function, only draw w:16, h:8 for each x,y region set.
  for (uint16_t indexh=0; indexh < h; indexh++) {
    //memcpy ( (tmpDecordedRgb565 + (y+indexh)*240*3 + x*3), ((uint8_t*)bitmap)+indexh*w*3, w*3);
    memcpy ( (tmpDecordedRgb565 + (y+indexh)*240 + x), bitmap+(indexh*w), w*2);
    //Serial.printf("    decord_output:%d , x: %d, y: %d, w: %d, h:%d, memcpy size: %d, addr: 0x%x, bitmap: 0x%x\n", outputIndex, x, y, w, h , w*2,  (tmpDecordedRgb565 + (y+indexh)*240 + x), bitmap+indexh*w);
  }
  tft.pushImage(x+80, y, w, h, bitmap);
  return 1;
}
*/

// Interrupt Service Routine (IRS) callback function, declare as IRAM_ATTR means put it in RAM (increase meet rate)
// Note: don't know why... isr need locate above setup()
void IRAM_ATTR isr_Callback() {  
  // Only run simply code to avoid system crash
  triggerClassify = !triggerClassify;
  //Serial.printf("triggerClassify: %d \n", triggerClassify);
}

// setup
void setup() {
  Serial.begin(115200);

#if BTN_CONTROL == true  
  // button
  pinMode(BTN, INPUT);
#endif

/*
  // TFT display init
  tft.initR(INITR_GREENTAB); // you might need to use INITR_REDTAB or INITR_BLACKTAB to get correct text colors
  tft.setRotation(3);  // 1 turn 90 degrees, 2: 180, 3: 270
  tft.fillScreen(ST77XX_BLACK);
*/

  // Initialise the TFT
  tft.init();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setRotation(3);//1:landscape 3:inv. landscape

//  tft.initDMA(); // To use SPI DMA you must call initDMA() to setup the DMA engine
  // The jpeg image can be scaled down by a factor of 1, 2, 4, or 8
//  TJpgDec.setJpgScale(1);
  // The colour byte order can be swapped by the decoder
  // using TJpgDec.setSwapBytes(true); or by the TFT_eSPI library:
//  tft.setSwapBytes(true);
  //TJpgDec.setSwapBytes(true);
  // The decoder must be given the exact name of the rendering function above
//  TJpgDec.setCallback(tft_output);


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
}

// main loop
void loop() {
  uint32_t StartTime, EndTime;
  camera_fb_t *fb = NULL;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  //Serial.println("Start show screen.");
  StartTime = micros(); //millis();
  showScreen(fb, TFT_YELLOW);
  EndTime = micros(); //millis();
  //Serial.printf("Show screen. spend time: %d ms\n", EndTime - StartTime);
  Serial.printf("Show screen. spend time: %d.%d ms\n", (EndTime - StartTime)/1000, (EndTime - StartTime)%1000);

  // capture a image and classify it
#if BTN_CONTROL == true  
  if (triggerClassify) {
#endif    
    Serial.println("Start classify.");
    StartTime = millis();
    String result = classify(fb);
    EndTime = millis();
    Serial.printf("End classify. spend time: %d ms\n", EndTime - StartTime);

    // display result
    //Serial.printf("Result: %s\n", result);
    Serial.print("Result: "); Serial.println(result);
    tft_drawtext(4, 120 - 16, result, 2, TFT_GREEN /*ST77XX_GREEN*/);

#if BTN_CONTROL == true  
    // wait for next press button to continue show screen
    while (triggerClassify){
      delay (200);
      //Serial.printf("while triggerClassify: %d \n", triggerClassify);
    }
    //tft.fillScreen(ST77XX_BLACK);
    tft.fillScreen(TFT_BLACK);
    //delay(1000);
  }
#else 
  //delay for next loop.
#if FAST_CLASSIFY != 1
  uint16_t delayTime = 6000;
  Serial.printf("Finish classify, wait for %d ms to next loop.\n\n\n", delayTime);
  delay(delayTime);
#endif
  tft.fillScreen(TFT_BLACK);
  Serial.println("Finish classify.\n");
#endif 

  esp_camera_fb_return(fb);
}

void showScreen(camera_fb_t *fb, uint16_t color) {
  //int StartTime, EndTime;

  // Note: larger than 96x96 may draw a not identify screen, need to separate screen and do multi pushImage.
//#if BTN_CONTROL == true    
  tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)fb->buf);
//#else
//  tft.pushImage((TFT_LCD_WIDTH-SHOW_WIDTH), (TFT_LCD_HEIGHT-SHOW_HEIGHT), SHOW_WIDTH, SHOW_HEIGHT, (uint16_t *)fb->buf);  
//#endif
/*
  // --- Convert frame to RGB565 and display on the TFT ---
  Serial.println("  Converting to RGB565 and display on TFT...");
  uint8_t *rgb565 = (uint8_t *) malloc(240 * 240 * 3);
  //uint8_t *rgb565 = (uint8_t *) malloc(96 * 96 * 3); 
  StartTime = millis();
  jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_2X); // scale to half size
  EndTime = millis();
  Serial.printf("  jpg2rgb565() spend time: %d ms\n", EndTime - StartTime);
  //jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_NONE); // scale to half size
  tft.drawRGBBitmap(0, 0, (uint16_t*)rgb565, 120, 120);

  // --- Free memory ---
  //rgb565 = NULL;
  free(rgb565);
*/

/*
memcpy (rgb565, fb->buf, RGB565_SIZE);
for (uint32_t index = 0; index < RGB565_SIZE/2; index++) {
  *(rgb565 + index) = __builtin_bswap16(*(rgb565 + index));  //need to swap uint16_t data for RGB draw
}
tft.drawRGBBitmap((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2, rgb565, SHOW_WIDTH, SHOW_HEIGHT);
*/

//  tft.startWrite();
  // Draw the image, top left at 0,0 - DMA request is handled in the call-back tft_output() in this sketch
  //TJpgDec.drawJpg(0, 0, panda, sizeof(panda));
 // TJpgDec.drawJpg((TFT_LCD_WIDTH-SHOW_WIDTH)/2, (TFT_LCD_HEIGHT-SHOW_HEIGHT)/2,  fb->buf, fb->len);
//  TJpgDec.drawJpg(-56, -40, fb->buf, fb->len);
  // Must use endWrite to release the TFT chip select and release the SPI channel
//  tft.endWrite();
  //drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color),
//  tft.drawRect (32, 16, 96, 96, color);

}

// classify labels
String classify(camera_fb_t * fb) {
  int StartTime, EndTime;
  // run image capture once to force clear buffer
  // otherwise the captured image below would only show up next time you pressed the button!
//  capture_quick();

  // capture image from camera
  if (!capture(fb)) return "Error";
  tft_drawtext(4, 4, "Classifying...", 1, TFT_CYAN /*ST77XX_CYAN*/);

  Serial.println("  Getting image...");
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  Serial.println("  Run classifier...");
  // Feed signal to the classifier
  StartTime = millis();
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
  EndTime = millis();
  Serial.printf("  run_classifier() spend time: %d ms\n", EndTime - StartTime);
  // --- Free memory ---
  dl_matrix3du_free(resized_matrix);

  // --- Returned error variable "res" while data object.array in "result" ---
  ei_printf("run_classifier returned: %d\n", res);
  if (res != 0) return "Error";

  // --- print the predictions ---
  ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
  int index;
  float score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    // record the most possible label
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
    ei_printf("    %s: \t%f\r\n", result.classification[ix].label, result.classification[ix].value);
    tft_drawtext(4, 12 + 8 * ix, String(result.classification[ix].label) + " " + String(result.classification[ix].value * 100) + "%", 1, TFT_ORANGE /*ST77XX_ORANGE*/);
  }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("    anomaly score: %f\r\n", result.anomaly);
#endif

  // --- return the most possible label ---
  return String(result.classification[index].label);
}

/*
// quick capture (to clear buffer)
void capture_quick() {
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) return;
  esp_camera_fb_return(fb);
}
*/

// capture image from cam
bool capture(camera_fb_t * fb) {
/*
  Serial.println("Capture image...");
  esp_err_t res = ESP_OK;
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }
*/

  // --- Convert frame to RGB888  ---
  //Serial.println("Converting to RGB888...");
  // Allocate rgb888_matrix buffer
  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  //Serial.println("fmt2rgb888...");  
  //fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

  //Serial.println("rgb565_to_888...");  
  for (uint16_t i=0; i < fb->len; i+=2) {
    rgb565_to_888( *((uint16_t *)fb->buf + i), (rgb888_matrix->item + (i>>1)*3) );
  }

  resized_matrix = rgb888_matrix; // due to capture 96x96, no need to resize.

/*
  Serial.println("TJpgDec.setCallback(decord_output);...");  
  tmpDecordedRgb565 = (uint16_t *) malloc(240*240*2);
  memset(tmpDecordedRgb565, 0x0, 240*240*2);
  //tft.setSwapBytes(false);
  // TJpgDec.setSwapBytes(false); default is false?
  //TJpgDec.setFormat (0); // 0: RGB888 (24-bit/pix)
  TJpgDec.setCallback(decord_output);
  outputIndex = 0;
  TJpgDec.drawJpg(0, 0, fb->buf, fb->len);

  // rollback TJpgDec.setCallback(tft_output);
  Serial.println("rollback TJpgDec.setCallback(tft_output);...");  
  //tft.setSwapBytes(true);
  //TJpgDec.setSwapBytes(true); default is false?
  //TJpgDec.setFormat (1); // 1: RGB565 (16-bit/pix) , default value
  TJpgDec.setCallback(tft_output);

  Serial.printf("fmt2rgb888...from TJpegDec RGB5656, fb->width:%d, fb->height:%d\n", fb->width, fb->height);  
  fmt2rgb888((uint8_t *)tmpDecordedRgb565, fb->width * fb->height * 2, PIXFORMAT_RGB565, rgb888_matrix->item);
*/


  //Serial.println("memcpy...");  
  //memcpy (rgb565, fb->buf, fb->len);
  //Serial.println("fmt2rgb888...");  
  //fmt2rgb888(rgb565, fb->len, fb->format, rgb888_matrix->item);

  // --- Resize the RGB888 frame to 96x96 in this example ---
  //Serial.println("Resizing the frame buffer...");
  //resized_matrix = dl_matrix3du_alloc(1, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3);
  //image_resize_linear(resized_matrix->item, rgb888_matrix->item, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3, fb->width, fb->height);



  // --- Free memory ---
//  dl_matrix3du_free(rgb888_matrix);  // don't free rgb888_matrix due to it assign to resized_matrix and resized_matrix will be free in up function.
//  esp_camera_fb_return(fb);
//  free(tmpDecordedRgb565);

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
/*  
  tft.setCursor(x, y);
  tft.setTextSize(font_size); // font size 1 = 6x8, 2 = 12x16, 3 = 18x24
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(strcpy(new char[text.length() + 1], text.c_str()));
*/

tft.setCursor (x, y, font_size);
tft.setTextColor(color);
tft.print(strcpy(new char[text.length() + 1], text.c_str()));

}
