#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MQUnifiedsensor.h>
#include "BTL_IoT_inferencing.h"

#define RXD2 16
#define TXD2 17

float AQI_min = 1.0;
float AQI_max = 315.0;
#define NUM_FEATURES 4 // 4 cảm biến
#define NUM_STEPS 24   // 24 mẫu gần nhất (24x4 = 96)
float features[NUM_FEATURES * NUM_STEPS] = {0};

// ===== WiFi & MQTT (ThingsBoard) =====
const char *ssid = "OPPO A53";
const char *password = "12345678";
const char *mqttServer = "eu.thingsboard.cloud";
const uint16_t mqttPort = 1883;
const char *accessToken = "CA9jXfJgs0RGW9emImRF";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== MQ7 Sensor =====
#define Board ("ESP32")
#define Pin_MQ7 34
#define Type_MQ7 ("MQ-7")
#define Voltage_Resolution (3.3)
#define ADC_Bit_Resolution (12)
#define RatioMQ7CleanAir (27.5)
MQUnifiedsensor MQ7(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin_MQ7, Type_MQ7);

// ===== GP2Y1010AU0F (PM2.5) =====
#define GP_LED_CTRL 23
#define GP_VO_PIN 35
const int samplingTime = 280;
const int deltaTime = 40;
const int sleepTime = 9680;

float temperature = NAN, humidity = NAN;
float CO_ppm = 0, dustDensity = 0, AQI = 0;
char payload[256];

float AQI_CO(float CO)
{
  if (CO <= 4.4)
    return (50.0 / 4.4) * CO;
  else if (CO <= 9.4)
    return 50 + (50.0 / 5.0) * (CO - 4.4);
  else if (CO <= 12.4)
    return 100 + (50.0 / 3.0) * (CO - 9.4);
  else if (CO <= 15.4)
    return 150 + (50.0 / 3.0) * (CO - 12.4);
  else if (CO <= 30.4)
    return 200 + (100.0 / 15.0) * (CO - 15.4);
  else
    return 300 + (200.0 / 45.0) * (CO - 30.4);
}

float AQI_PM25(float pm)
{
  if (pm <= 12)
    return (50.0 / 12.0) * pm;
  else if (pm <= 35.4)
    return 50 + (50.0 / 23.4) * (pm - 12.0);
  else if (pm <= 55.4)
    return 100 + (50.0 / 20.0) * (pm - 35.4);
  else if (pm <= 150.4)
    return 150 + (100.0 / 95.0) * (pm - 55.4);
  else if (pm <= 250.4)
    return 200 + (100.0 / 100.0) * (pm - 150.4);
  else
    return 300 + (200.0 / 150.0) * (pm - 250.4);
}

void setup_wifi()
{
  WiFi.begin(ssid, password);
  Serial.print("Kết nối Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi connected!");
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("MQTT...");
    if (client.connect("ESP32_Gateway", accessToken, NULL))
      Serial.println("Connected!");
    else
    {
      Serial.print("state=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // UART2

  setup_wifi();
  client.setServer(mqttServer, mqttPort);

  // MQ7 setup
  MQ7.setRegressionMethod(1);
  MQ7.setA(99.042);
  MQ7.setB(-1.518);
  MQ7.init();

  float calcR0 = 0;
  for (int i = 0; i < 50; i++)
  {
    MQ7.update();
    calcR0 += MQ7.calibrate(RatioMQ7CleanAir);
    delay(200);
  }
  MQ7.setR0(calcR0 / 50.0);

  pinMode(GP_LED_CTRL, OUTPUT);
  digitalWrite(GP_LED_CTRL, HIGH);
}

float runPrediction(float *features)
{
  // Kiểm tra dữ liệu đầu vào hợp lệ
  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++)
  {
    if (isnan(features[i]) || isinf(features[i]))
    {
      Serial.println("Có giá trị NaN hoặc Inf trong chuỗi dữ liệu đầu vào!");
      return -1;
    }
  }

  // Tạo tín hiệu đầu vào
  signal_t signal;
  int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0)
  {
    ei_printf("Lỗi tạo tín hiệu đầu vào (%d)\n", err);
    return -1;
  }

  // Chạy mô hình
  ei_impulse_result_t result = {0};
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
  if (res != EI_IMPULSE_OK)
  {
    ei_printf("Lỗi khi chạy mô hình (%d)\n", res);
    return -1;
  }

  // Lấy kết quả dự đoán
  float normalized_pred = result.classification[0].value;
  if (isnan(normalized_pred) || isinf(normalized_pred))
  {
    Serial.println("Giá trị dự đoán NaN hoặc Inf!");
    return -1;
  }

  float predicted_AQI = normalized_pred * (AQI_max - AQI_min) + AQI_min;
  Serial.printf("Dự đoán AQI: %.3f\n", normalized_pred);
  Serial.printf("AQI chuẩn hóa: %.3f\n", predicted_AQI);
  return predicted_AQI;
}

void loop()
{
  if (!client.connected())
    reconnect();
  client.loop();

  // Nhận dữ liệu từ UART
  if (Serial2.available())
  {
    String json = Serial2.readStringUntil('\n');
    Serial.print("UART nhận: ");
    Serial.println(json);

    StaticJsonDocument<200> doc;
    if (deserializeJson(doc, json) == DeserializationError::Ok)
    {
      temperature = doc["t"];
      humidity = doc["h"];
    }
  }

  // Đọc MQ7 và GP2Y1010
  MQ7.update();
  CO_ppm = MQ7.readSensor();

  digitalWrite(GP_LED_CTRL, LOW);
  delayMicroseconds(samplingTime);
  int voMeasured = analogRead(GP_VO_PIN);
  delayMicroseconds(deltaTime);
  digitalWrite(GP_LED_CTRL, HIGH);
  delayMicroseconds(sleepTime);

  float voltage = voMeasured * (3.3 / 4095.0);
  dustDensity = 170 * voltage - 0.1;
  if (dustDensity < 0)
    dustDensity = 0;

  float aqiCO = AQI_CO(CO_ppm);
  float aqiPM = AQI_PM25(dustDensity);
  AQI = max(aqiCO, aqiPM);

  // Dịch dữ liệu cũ sang trái để thêm mẫu mới ở cuối
  memmove(features, features + NUM_FEATURES, sizeof(float) * NUM_FEATURES * (NUM_STEPS - 1));
  // Thêm mẫu mới vào cuối chuỗi
  features[(NUM_STEPS - 1) * NUM_FEATURES + 0] = temperature;
  features[(NUM_STEPS - 1) * NUM_FEATURES + 1] = humidity;
  features[(NUM_STEPS - 1) * NUM_FEATURES + 2] = CO_ppm;
  features[(NUM_STEPS - 1) * NUM_FEATURES + 3] = dustDensity;

  // float predicted_AQI = runPrediction(temperature, humidity, CO_ppm, dustDensity);
  float predicted_AQI = runPrediction(features);

  if (!isnan(temperature) && !isnan(humidity))
  {
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.2f,\"humidity\":%.2f,"
             "\"CO\":%.2f,\"PM25\":%.2f,\"AQI\":%.2f, \"predicted_AQI\":%.2f}",
             temperature, humidity, CO_ppm, dustDensity, AQI, predicted_AQI);

    client.publish("v1/devices/me/telemetry", payload);
    Serial.println(payload);
  }

  delay(3000);
}
