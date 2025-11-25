#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char *ssid = "TP-Link";
const char *password = "asdfghjkl";

// WebSocket server - CAR ENDPOINT
const char *websocket_server = "192.168.0.115";
const int websocket_port = 3000;
const char *websocket_path = "/ws/car";  // Changed to car endpoint

// Motor A (Left wheels) pins
#define ENA 33
#define IN1 25
#define IN2 26

// Motor B (Right wheels) pins
#define ENB 32
#define IN3 14
#define IN4 12

// PWM settings
const int freq = 30000;
const int resolution = 8;
int channelA = -1;
int channelB = -1;

// Default motor speed (0-255)
uint8_t motorSpeed = 200;

WebSocketsClient webSocket;

void setup()
{
  Serial.begin(115200);

  // Initialize motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Setup PWM channels (ESP32 core v3+)
  channelA = ledcAttach(ENA, freq, resolution);
  channelB = ledcAttach(ENB, freq, resolution);
  if (channelA < 0 || channelB < 0)
  {
    Serial.println("⚠️  Failed to attach LEDC channels");
  }

  // Stop motors initially
  stopCar();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize WebSocket - CAR ENDPOINT
  webSocket.begin(websocket_server, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop()
{
  webSocket.loop();
}

// ===== MOTOR CONTROL FUNCTIONS ===== //

void motorAForward()
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void motorABackward()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
}

void motorAStop()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void motorBForward()
{
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void motorBBackward()
{
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void motorBStop()
{
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void setMotorASpeed(uint8_t speed)
{
  if (channelA >= 0)
  {
    ledcWrite(channelA, speed);
  }
}

void setMotorBSpeed(uint8_t speed)
{
  if (channelB >= 0)
  {
    ledcWrite(channelB, speed);
  }
}

// ===== CAR CONTROL FUNCTIONS ===== //

void moveForward()
{
  motorAForward();
  motorBForward();
  setMotorASpeed(motorSpeed);
  setMotorBSpeed(motorSpeed);
  Serial.println("⬆️  Moving FORWARD");
}

void moveBackward()
{
  motorABackward();
  motorBBackward();
  setMotorASpeed(motorSpeed);
  setMotorBSpeed(motorSpeed);
  Serial.println("⬇️  Moving BACKWARD");
}

void turnLeft()
{
  motorABackward();
  motorBForward();
  setMotorASpeed(motorSpeed);
  setMotorBSpeed(motorSpeed);
  Serial.println("⬅️  Turning LEFT");
}

void turnRight()
{
  motorAForward();
  motorBBackward();
  setMotorASpeed(motorSpeed);
  setMotorBSpeed(motorSpeed);
  Serial.println("➡️  Turning RIGHT");
}

void stopCar()
{
  motorAStop();
  motorBStop();
  setMotorASpeed(0);
  setMotorBSpeed(0);
  Serial.println("🛑 Car STOPPED");
}

// ===== WEBSOCKET FUNCTIONS ===== //

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
  {
    Serial.printf("✅ Connected to Car WebSocket server\n");

    // Send identification message
    String identifyMsg = "{\"type\":\"esp32_car\",\"device\":\"car_controller\",\"status\":\"ready\"}";
    webSocket.sendTXT(identifyMsg);
    Serial.println("📤 Sent car identification message");
    break;
  }

  case WStype_DISCONNECTED:
    Serial.printf("❌ Disconnected from Car WebSocket server\n");
    break;

  case WStype_TEXT:
  {
    Serial.printf("📨 Car received: %s\n", payload);
    handleCommand((char *)payload);
    break;
  }

  case WStype_ERROR:
    Serial.printf("❌ Car WebSocket error\n");
    break;
  }
}

void handleCommand(char *command)
{
  // Quick check for car command structure
  if (command[0] != '{' || strstr(command, "\"command\"") == NULL) {
    Serial.println("🔄 Quick ignore - not a car command");
    return;
  }

  // Parse JSON command
  DynamicJsonDocument doc(200);
  DeserializationError error = deserializeJson(doc, command);

  if (error)
  {
    Serial.printf("❌ JSON parse error: %s\n", error.c_str());
    return;
  }

  // Check if this is actually a car command
  if (!doc.containsKey("command")) {
    Serial.println("🔄 Ignoring non-command message");
    return;
  }

  String cmd = doc["command"];
  int speed = doc["speed"] | motorSpeed;

  // Update motor speed if provided
  if (doc.containsKey("speed"))
  {
    motorSpeed = constrain(speed, 0, 255);
    Serial.printf("⚙️  Motor speed set to: %d\n", motorSpeed);
  }

  // Execute the command
  if (cmd == "forward")
  {
    moveForward();
  }
  else if (cmd == "backward")
  {
    moveBackward();
  }
  else if (cmd == "left")
  {
    turnLeft();
  }
  else if (cmd == "right")
  {
    turnRight();
  }
  else if (cmd == "stop")
  {
    stopCar();
  }
  else
  {
    Serial.printf("⚠️  Unknown command: %s\n", cmd.c_str());
    return;
  }

  // Send acknowledgment back to server
  DynamicJsonDocument ackDoc(100);
  ackDoc["type"] = "ack";
  ackDoc["command"] = cmd;
  ackDoc["status"] = "executed";

  String ackMsg;
  serializeJson(ackDoc, ackMsg);
  webSocket.sendTXT(ackMsg);
  Serial.printf("✅ Sent ACK for: %s\n", cmd.c_str());
}
