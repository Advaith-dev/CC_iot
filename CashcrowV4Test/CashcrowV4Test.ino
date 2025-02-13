#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <esp_now.h>

// MAC Address of the receiver ESP32
uint8_t receiverAddress[] = {0x08, 0xd1, 0xf9, 0xed, 0xcd, 0x10};
// Structure to hold the message
typedef struct struct_message {
    char text[32];
} struct_message;

struct_message message;

const char* ssid = "Uwu";
const char* pass = "orldhello";
const char* serverURL = "http://192.168.127.144:5000/classify";

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define TRIGGER_PIN 13
#define ECHO_PIN    15
#define DISTANCE_THRESHOLD_CM 20.0 

int imgCount = 0;
bool sensorTriggered = false;
int uId = 123;

void sendImageTask(void* parameter);

// Callback function to confirm message delivery
void onSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.print("Message Sent: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    sensorTriggered = false;
    imgCount = 0;
    delay(3000);
}

float measureDistance() {
  // Clear the trigger
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  
  // Send a 10 microsecond pulse to trigger
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  
  // Read the echo time in microseconds
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms (approx. 5m)
  
  float distance = (duration * 0.0343) / 2.0;
  return distance;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("Connected to WiFi: %s\n", ssid);
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  int channel = WiFi.channel();
  Serial.printf("Wi-Fi Channel: %d\n", channel);
  //Serial.println(channel);

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  // Create a task for sending images
  //xTaskCreate(sendImageTask, "SendImageTask", 8192, NULL, 1, NULL);

    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    // Register send callback
    esp_now_register_send_cb(onSent);

    // Add receiver
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = channel;  
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void loop() {
  if (!sensorTriggered) {
    float distance = measureDistance();
    distance = 10.0;
    
    Serial.printf("Distance: %.1f cm\n", distance);

    if (distance > 0 && distance < DISTANCE_THRESHOLD_CM) {
      sensorTriggered = true;
      Serial.println("Ultrasonic sensor triggered. Starting image capture process...");
      xTaskCreate(sendImageTask, "SendImageTask", 8192, NULL, 1, NULL);
    }
  }

  delay(300);
}

void sendImageTask(void* parameter) {
  for (int i = 0; i < 5; i++){
    String response;

    HTTPClient http;
    http.begin(serverURL);

    Serial.println(imgCount);
    http.addHeader("imgCount", String(imgCount));
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("Unit-ID", String(uId));

      // Capture an image
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      http.end();
      vTaskDelete(NULL);
    }
    Serial.println("Image Captured");
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
      
    // Send the image via HTTP req
    int httpResponseCode = http.POST(fb->buf, fb->len);
    if (httpResponseCode > 0) {
      response = http.getString();
      Serial.println("Server response: " + response);
    } else {
      Serial.println("HTTP POST failed");
    }

    // Return the frame buffer to the camera
    esp_camera_fb_return(fb);
    http.end();
    if(++imgCount == 5 ){
      strcpy(message.text, response.c_str()); 
      esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&message, sizeof(message));
      if (result == ESP_OK) {
        Serial.println("Message sent successfully");
      } 
      else {
        Serial.println("Error sending message");
        sensorTriggered = false;
      }
 
    }

  }   
  vTaskDelete(NULL);
}
