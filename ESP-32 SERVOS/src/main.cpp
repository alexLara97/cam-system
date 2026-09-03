#include <WiFi.h>
#include <ESP32Servo.h>    // SERVOS
#include <PubSubClient.h>  // MQTT


const char *ssid = "xxx";
const char *password = "xxxx";

// IP estatica
IPAddress local_IP(192, 168, 10, 28);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 10, 17);
IPAddress secundaryDNS(8, 8, 8, 8);

// MQTT
const char *mqtt_server = "192.168.10.17"; // IP de broker mosquito
const int mqtt_port = 1883;
const char *mqtt_client_id = "esp32-servos";

const char *topic_pan = "casa/camara/servo/pan";
const char *topic_tilt = "casa/camara/servo/tilt";
const char *topic_estado = "casa/camara/servo/estado";

WiFiClient espClient;
PubSubClient client(espClient);

// SERVOS
Servo servoPan;
Servo servoTilt;
const int PIN_SERVO_PAN = 17;
const int PIN_SERVO_TILT = 16;
// Limites mecanicos de los servos
const int PAN_MIN = 0;
const int PAN_MAX = 180;
const int TILT_MIN = 30;
const int TILT_MAX = 150;

int currentPan = 90;
int currentTilt = 90;

unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 2000; //ms

// Funciones para utilidades
int angleLimit(int val, int minVal, int maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

void publishEstado() {
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"pan\":%d,\"tilt\":%d}", currentPan, currentTilt);
    client.publish(topic_estado, payload, true);
}

// Callback MQTT (Al recibir un mensaje)
void callback(char *topic, byte *payload, unsigned int length) {
    char msg[16];
    if (length >= sizeof(msg)) length = sizeof(msg) - 1;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    int angle = atoi(msg);
    
    if (strcmp(topic, topic_pan) == 0) {
        currentPan = angleLimit(angle, PAN_MIN, PAN_MAX);
        servoPan.write(currentPan);
    } else if (strcmp(topic, topic_tilt) == 0) {
        currentTilt = angleLimit(angle, TILT_MIN, TILT_MAX);
        servoTilt.write(currentTilt);
    }

    publishEstado();
}

// Reconexion MQTT
void reconnectMQTT() {
    while (!client.connected()) {
        if (client.connect(mqtt_client_id)) {
            client.subscribe(topic_pan);
            client.subscribe(topic_tilt);
            publishEstado(); // publica posicion incial al reconectar
        } else {
            delay(3000);
        }
    }
}

void setupWiFi() {
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

void setup() {
    Serial.begin(115200);

    servoPan.setPeriodHertz(50); // servos estandar: 50 Hz
    servoTilt.setPeriodHertz(50);
    servoPan.attach(PIN_SERVO_PAN, 500, 2400);   // rango de pulso típico en microsegundos
    servoTilt.attach(PIN_SERVO_TILT, 500, 2400);

    servoPan.write(currentPan);
    servoTilt.write(currentTilt);

    setupWiFi();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();

    // Publicacion periodica de estado - Para que el HMI sepa que sigue operativo
    if (millis() - lastStatePublish > STATE_PUBLISH_INTERVAL) {
        lastStatePublish = millis();
        publishEstado();
    }
}