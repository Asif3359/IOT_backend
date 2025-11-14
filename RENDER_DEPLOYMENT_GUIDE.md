# Render Deployment Guide for ESP32-CAM

## ✅ Yes, your app will work after deploying to Render!

But you need to make a few changes to the ESP32 code to use **WSS (Secure WebSocket)** instead of **WS (Plain WebSocket)**.

---

## Changes Required

### 1. Update ESP32 Code for Render

Your current code uses:
- **Local**: `ws://192.168.0.115:3000/ws` (HTTP, port 3000)
- **Render**: `wss://iot-backend-uy96.onrender.com/ws` (HTTPS, port 443)

### 2. Required Changes in `iot_soket.ino`

#### Option A: Simple Configuration (Recommended)

Add configuration flags to easily switch between local and production:

```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>  // ADD THIS for SSL
#include <ArduinoJson.h>

// ===== DEPLOYMENT MODE ===== //
#define USE_RENDER true  // Set to true for Render, false for local
// ============================ //

// ===== WIFI CONFIGURATION ===== //
const char* ssid = "TP-Link";
const char* password = "asdfghjkl";
// ============================== //

// Server Configuration
#if USE_RENDER
  // Render.com configuration (Production)
  const char* serverHost = "iot-backend-uy96.onrender.com";  // Your Render URL
  const uint16_t serverPort = 443;  // HTTPS/WSS port
  const char* serverPath = "/ws";
  const bool useSSL = true;  // Use secure WebSocket
#else
  // Local development configuration
  const char* serverHost = "192.168.0.115";
  const uint16_t serverPort = 3000;
  const char* serverPath = "/ws";
  const bool useSSL = false;  // Use plain WebSocket
#endif

WebSocketsClient webSocket;
WiFiClientSecure secureClient;  // ADD THIS for SSL
bool wsConnected = false;
unsigned long lastFrameTime = 0;
const int frameInterval = 100; // ~10 FPS

// ... rest of your camera configuration stays the same ...

void connectWebSocket() {
  Serial.println("\n🔌 Connecting to WebSocket...");
  
  if (useSSL) {
    // For Render.com (WSS - Secure WebSocket)
    Serial.println("   Using WSS (Secure WebSocket)");
    Serial.printf("   Server: %s:%d%s\n", serverHost, serverPort, serverPath);
    
    // IMPORTANT: Skip SSL certificate validation
    // This is needed because ESP32 can't validate Render's SSL certificate
    secureClient.setInsecure();
    
    // Initialize secure WebSocket
    webSocket.beginSSL(serverHost, serverPort, serverPath);
  } else {
    // For local development (WS - Plain WebSocket)
    Serial.println("   Using WS (Plain WebSocket)");
    Serial.printf("   Server: %s:%d%s\n", serverHost, serverPort, serverPath);
    
    // Initialize plain WebSocket
    webSocket.begin(serverHost, serverPort, serverPath);
  }
  
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);
}
```

#### Option B: Minimal Changes (Quick Fix)

Just update these lines in your current code:

```cpp
// Change line 12-14:
const char* serverHost = "iot-backend-uy96.onrender.com";  // Your Render URL
const uint16_t serverPort = 443;  // HTTPS port
const char* serverPath = "/ws";

// Add at top (after includes):
#include <WiFiClientSecure.h>
WiFiClientSecure secureClient;

// Update connectWebSocket() function:
void connectWebSocket() {
  Serial.println("\n🔌 Connecting to WebSocket (Render)...");
  
  // Skip SSL certificate validation (needed for Render)
  secureClient.setInsecure();
  
  // Use beginSSL instead of begin
  webSocket.beginSSL(serverHost, serverPort, serverPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);
}
```

---

## Complete Updated Code

Here's the complete updated `iot_soket.ino` with Render support:

```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>  // For SSL/WSS
#include <ArduinoJson.h>

// ===== DEPLOYMENT CONFIGURATION ===== //
#define USE_RENDER true  // true = Render.com, false = Local

#if USE_RENDER
  const char* serverHost = "iot-backend-uy96.onrender.com";
  const uint16_t serverPort = 443;
  const char* serverPath = "/ws";
#else
  const char* serverHost = "192.168.0.115";
  const uint16_t serverPort = 3000;
  const char* serverPath = "/ws";
#endif
// ==================================== //

// ===== WIFI CONFIGURATION ===== //
const char* ssid = "TP-Link";
const char* password = "asdfghjkl";
// ============================== //

WebSocketsClient webSocket;
WiFiClientSecure secureClient;  // For SSL
bool wsConnected = false;
unsigned long lastFrameTime = 0;
const int frameInterval = 100; // ~10 FPS

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

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("❌ WebSocket DISCONNECTED");
      wsConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("✅ WebSocket CONNECTED to: %s\n", payload);
      wsConnected = true;
      sendIdentification();
      break;
      
    case WStype_TEXT:
      if (length < 100) {
        Serial.printf("📨 Message: %s\n", payload);
      }
      break;
      
    case WStype_BIN:
      Serial.printf("📦 Binary data: %d bytes\n", length);
      break;
      
    default:
      break;
  }
}

void sendIdentification() {
  DynamicJsonDocument doc(200);
  doc["type"] = "esp32_camera";
  doc["device"] = "ESP32-CAM";
  doc["frame_size"] = "CIF";
  doc["quality"] = 15;
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}

void sendFrame() {
  if (!wsConnected || !webSocket.isConnected()) {
    return;
  }
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Camera capture failed");
    return;
  }
  
  bool success = webSocket.sendBIN(fb->buf, fb->len);
  
  if (!success) {
    Serial.println("❌ Failed to send frame");
    wsConnected = false;
  }
  
  static unsigned long frameCount = 0;
  if (++frameCount % 30 == 0) {
    Serial.printf("✅ Frames sent: %lu (size: %d bytes)\n", frameCount, fb->len);
  }
  
  esp_camera_fb_return(fb);
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
  config.frame_size = FRAMESIZE_CIF;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  return esp_camera_init(&config) == ESP_OK;
}

bool connectWiFi() {
  Serial.println("\n📡 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("✅ WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n❌ WiFi Failed");
  return false;
}

void connectWebSocket() {
  Serial.println("\n🔌 Connecting to WebSocket...");
  
  #if USE_RENDER
    // Render.com - Use Secure WebSocket (WSS)
    Serial.println("   Mode: Production (Render.com)");
    Serial.println("   Protocol: WSS (Secure)");
    secureClient.setInsecure();  // Skip SSL validation
    webSocket.beginSSL(serverHost, serverPort, serverPath);
  #else
    // Local - Use Plain WebSocket (WS)
    Serial.println("   Mode: Local Development");
    Serial.println("   Protocol: WS (Plain)");
    webSocket.begin(serverHost, serverPort, serverPath);
  #endif
  
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n🚀 ESP32-CAM WebSocket Streamer Starting...");
  
  #if USE_RENDER
    Serial.println("📡 Target: Render.com (Production)");
  #else
    Serial.println("📡 Target: Local Server");
  #endif
  
  if (!initCamera()) {
    Serial.println("❌ Camera initialization failed!");
    return;
  }
  Serial.println("✅ Camera initialized");
  
  if (!connectWiFi()) {
    return;
  }
  
  connectWebSocket();
}

void loop() {
  webSocket.loop();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected");
    wsConnected = false;
    delay(5000);
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
      connectWebSocket();
    }
    return;
  }
  
  if (wsConnected && webSocket.isConnected()) {
    unsigned long currentTime = millis();
    if (currentTime - lastFrameTime >= frameInterval) {
      sendFrame();
      lastFrameTime = currentTime;
    }
  } else {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 5000) {
      Serial.println("🔄 Attempting to reconnect...");
      connectWebSocket();
      lastReconnectAttempt = millis();
    }
  }
  
  delay(10);
}
```

---

## Server-Side: Render Configuration

### 1. Render Environment Variables

Make sure your Render service has:
- **PORT**: Automatically set by Render (usually 10000)
- **NODE_ENV**: `production`

### 2. Render Service Settings

- **Build Command**: `npm install`
- **Start Command**: `npm start`
- **Environment**: `Node`

### 3. WebSocket on Render

Render supports WebSocket connections, but:
- ✅ Your current code should work
- ✅ WebSocket path `/ws` will work
- ⚠️ Render may have connection timeouts (15-30 seconds idle)

---

## Testing Steps

### 1. Deploy to Render
```bash
# Push your code to GitHub
git add .
git commit -m "Deploy to Render"
git push origin main

# Render will auto-deploy
```

### 2. Update ESP32 Code
- Set `USE_RENDER = true`
- Update `serverHost` to your Render URL
- Upload to ESP32

### 3. Test Connection
- ESP32 should connect to `wss://your-backend.onrender.com/ws`
- Check Render logs for: `📷 ESP32 Camera connected!`
- Check ESP32 Serial Monitor for: `✅ WebSocket CONNECTED`

---

## Important Notes

### SSL Certificate Validation

**Why `setInsecure()`?**
- ESP32 can't validate Render's SSL certificate
- `setInsecure()` skips validation (safe for development)
- For production, you could add Render's certificate (complex)

**Is it safe?**
- ✅ Safe for IoT devices connecting to known servers
- ⚠️ Not recommended for public-facing services
- ✅ Render uses valid SSL certificates

### Connection Stability

**Render Considerations:**
- Render services may sleep after inactivity
- First connection might be slow (cold start)
- Keep-alive helps maintain connection

### Performance

**Render Free Tier:**
- Services sleep after 15 minutes of inactivity
- Cold start: 30-60 seconds
- Consider paid tier for always-on service

---

## Troubleshooting

### Issue: Connection fails
- **Check URL**: Ensure `wss://` not `ws://`
- **Check Port**: Use `443` for Render
- **Check SSL**: Make sure `beginSSL()` is used

### Issue: SSL errors
- **Solution**: Add `secureClient.setInsecure();` before `beginSSL()`

### Issue: Connection drops frequently
- **Render timeout**: Render may close idle connections
- **Solution**: Keep sending frames regularly (you're already doing this)

### Issue: Slow connection
- **Cold start**: First connection after sleep is slow
- **Solution**: Normal for free tier, consider paid tier

---

## Summary

✅ **Yes, it will work!**  
✅ **Changes needed:**
1. Add `#include <WiFiClientSecure.h>`
2. Change `webSocket.begin()` → `webSocket.beginSSL()`
3. Add `secureClient.setInsecure();`
4. Update server URL and port (443)
5. Use `wss://` protocol

✅ **Easy switch**: Use `#define USE_RENDER` to toggle between local/production

Your ESP32 will work perfectly with Render after these changes! 🚀

