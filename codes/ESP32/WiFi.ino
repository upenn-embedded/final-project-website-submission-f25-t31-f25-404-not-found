#include <WiFi.h>
#include <WebSocketsServer.h>

// WiFi and Pin
const char* ssid = "iPhone";
const char* password = "aaaaaaaa";

// WebSocket Server Port 81
WebSocketsServer wsServer = WebSocketsServer(81);

int r;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("ESP32 WebSocket starting...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  //  WebSocket
  wsServer.begin();
  Serial.println("WebSocket Server started!");
}

void loop() {
  wsServer.loop();

  // For testing purposes
  // ---------------------------
  // Send a message every 3 seconds
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 3000) {
    lastTime = millis();

    // int r = random(3);
    const char* msg;

    if (r == 0) msg = "L_HAND";
    else if (r == 1) msg = "R_HAND";
    else msg = "R_FOOT";

    Serial.print("Sending: ");
    Serial.println(msg);

    wsServer.broadcastTXT(msg);

    r++;
    if (r>2) r=0;
    // wsServer.broadcastTXT("R_FOOT");
  }
}
