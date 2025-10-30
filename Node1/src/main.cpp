#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 13
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// UART 2: TX = 17, RX = 16
HardwareSerial SerialNode(2);

void setup() {
  Serial.begin(115200);
  SerialNode.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  dht.begin();
  Serial.println("Node 1: Sẵn sàng gửi");
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println(" Lỗi đọc DHT22!");
    delay(2000);
    return;
  }

  String json = String("{\"t\":") + String(t, 2) +
                ",\"h\":" + String(h, 2) + "}";

  SerialNode.println(json);  // Gửi JSON qua UART
  Serial.print("Gửi: ");
  Serial.println(json);

  delay(3000);
}