#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ===== WIFI CONFIGURATION ===== //
const char* ssid = "TP-Link";
const char* password = "asdfghjkl";
// ============================== //

// Backend Configuration
const char* backendHost = "iot-backend-uy96.onrender.com";  // Render backend host
const int frameInterval = 100; // milliseconds (10 FPS)

// Camera configuration
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

unsigned long lastFrameTime = 0;

void setup() {
  Serial.begin(9600);
  delay(2000);
  
  Serial.println("\n\n🚀 ESP32-CAM Starting (HTTP POST Mode)...");
  
  if (initCamera()) {
    Serial.println("✅ Camera: READY");
  } else {
    Serial.println("❌ Camera: FAILED");
    return;
  }
  
  if (connectWiFi()) {
    Serial.println("✅ WiFi: CONNECTED");
    Serial.println("📡 Ready to stream video to server");
  } else {
    Serial.println("❌ WiFi: FAILED");
  }
}

bool initCamera() {
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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // 640x480
  config.jpeg_quality = 12;  // Lower = better quality (0-63)
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  return err == ESP_OK;
}

bool connectWiFi() {
  Serial.println("\n📡 Connecting to WiFi...");
  Serial.printf("Network: %s\n", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting");
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Connected!");
      Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\n❌ Failed to connect");
  return false;
}

void sendFrameViaHTTP() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi disconnected, reconnecting...");
    connectWiFi();
    return;
  }
  
  // Capture frame from camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Camera capture failed");
    return;
  }

  // Create HTTP client
  HTTPClient http;
  WiFiClientSecure client;
  
  // IMPORTANT: Skip SSL certificate validation for Render.com
  // This is safe for development/testing
  client.setInsecure();
  
  // Build URL
  String url = "https://" + String(backendHost) + "/frame";
  
  // Start HTTP connection
  http.begin(client, url);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("Content-Length", String(fb->len));
  
  // Send POST request with image data
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  // Check response
  if (httpResponseCode > 0) {
    // Success - print status occasionally to avoid spam
    if (millis() % 5000 < frameInterval) {
      Serial.printf("✅ Frame sent (size: %d bytes, HTTP: %d)\n", fb->len, httpResponseCode);
    }
    
    // Optional: Read response (usually just {"success":true})
    String response = http.getString();
    if (millis() % 5000 < frameInterval && response.length() > 0) {
      Serial.printf("📥 Server response: %s\n", response.c_str());
    }
  } else {
    Serial.printf("❌ HTTP POST failed: %d\n", httpResponseCode);
    Serial.printf("   Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  // Cleanup
  http.end();
  esp_camera_fb_return(fb);
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi disconnected, reconnecting...");
    connectWiFi();
    delay(5000);
    return;
  }

  // Send frames at specified interval
  unsigned long currentTime = millis();
  if (currentTime - lastFrameTime >= frameInterval) {
    sendFrameViaHTTP();
    lastFrameTime = currentTime;
  }
  
  delay(10);  // Small delay to prevent watchdog issues
}

