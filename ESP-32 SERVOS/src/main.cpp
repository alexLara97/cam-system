#include <WiFi.h>
#include <ESP32Servo.h>    // SERVOS
#include <PubSubClient.h>  // MQTT


const char *ssid = "XXX";
const char *password = "XXXX";

// IP estatica
IPAddress local_IP(192, 168, 10, 28);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 10, 17);
IPAddress secundaryDNS(8, 8, 8, 8);

// MQTT
const char *mqtt_server = "192.168.10.17"; // IP de broker mosquito
const uint8_t mqtt_port = 1883;
const char *mqtt_client_id = "esp32-servos";

const char *topic_pan = "";
const char *topic_tilt = "";
const char *topic_estado = "";

WiFiClient espClient;
PubSubClient(espClient);

// SERVOS
Servo servoPan;
Servo servotilt;
const int PIN_SERVO_PAN = 13;
const int PIN_SERVO_TILT = 12;
// Limites mecanicos de los servos
const int PAN_MIN = 0;
const int PAN_MAX = 180;
const int TILT_MIN = 30;
const int TILT_MAX = 150;

int currentPan = 90;
int currentTilt = 90;

unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 2000; //ms


void setup() {
    // WIFI CONNECT
    // Static IP configuration
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secundaryDNS)) {
    Serial.println("Error in Static IP");
    }
    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
}

void loop() {

}