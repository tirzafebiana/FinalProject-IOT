#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>
#include <ESP32Servo.h>

// WiFi credentials
#define WIFI_SSID "ZTE_2.4G_dfAUHh"
#define WIFI_PASSWORD "TXvnnsXj"

// ThingsBoard credentials
#define THINGSBOARD_SERVER "mqtt.thingsboard.cloud"
#define TOKEN "x1KQGaRVsUdxuMHWhN7S"

// Pin definitions
#define DHTPIN 15
#define DHTTYPE DHT22
#define MQ_PIN 2
#define SERVO_PIN 5
#define BUZZER_PIN 19
 
// MQ-135 sensor setup
#define placa "ESP-32"
#define Voltage_Resolution 5
#define type "MQ-135"
#define ADC_Bit_Resolution 10
#define RatioMQ135CleanAir 3.6

DHT dht(DHTPIN, DHTTYPE);
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, MQ_PIN, type);
WiFiClient wifiClient;
PubSubClient client(wifiClient);
Servo servo;

void setupWiFi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32_Device", TOKEN, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  dht.begin();

  // Initialize MQ135 sensor
  MQ135.init();
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println(" done!");

  if (isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected)");
    while (1);
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground)");
    while (1);
  }
  
  MQ135.serialDebug(true);

  // Initialize WiFi and MQTT connection
  setupWiFi();
  client.setServer(THINGSBOARD_SERVER, 1883);

  // Initialize servo and buzzer
  servo.attach(SERVO_PIN);
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Read NH3 concentration
  MQ135.setA(102.2); MQ135.setB(-2.473);
  float NH3 = MQ135.readSensor();

  // Read CO2 concentration
  MQ135.setA(110.47); MQ135.setB(-2.862);
  float CO2 = MQ135.readSensor();

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C | Humidity: ");
  Serial.print(humidity);
  Serial.print(" % | NH3: ");
  Serial.print(NH3);
  Serial.print(" ppm | CO2: ");
  Serial.println(CO2);

  // Create JSON payload
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity; payload += ",";
  payload += "\"NH3\":"; payload += NH3; payload += ",";
  payload += "\"CO2\":"; payload += CO2;
  payload += "}";

  // Send payload to ThingsBoard
  char attributes[100];
  payload.toCharArray(attributes, 100);
  client.publish("v1/devices/me/telemetry", attributes);

  // Condition to trigger buzzer and servo
  if (NH3 > 5 || CO2 > 5 || temperature > 30 || humidity > 80) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    delay(3000);
    servo.write(50);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    servo.write(0);
  }

  delay(5000); // Wait 5 seconds before next loop
}
