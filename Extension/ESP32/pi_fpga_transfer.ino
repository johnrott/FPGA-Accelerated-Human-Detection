#include <WiFi.h>
#include <SPI.h>


const char* WIFI_SSID = "XXXXXXXXXXX";
const char* WIFI_PASS = "XXXXXXXXXXX";


WiFiServer server(5000);


#define PIN_SCK   18
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_CS    5

SPIClass spi(VSPI);

#define IMG_W     128
#define IMG_H     256
#define IMG_BYTES (IMG_W * IMG_H)

uint8_t image_buf[IMG_BYTES];

uint8_t  last_fpga_response  = 0;
uint32_t fpga_pass_count     = 0;
uint32_t fpga_fail_count     = 0;
uint32_t checksum_fail_count = 0;

bool readExact(WiFiClient &client, uint8_t *buf, size_t len) {
  size_t received = 0;
  unsigned long lastByteTime = millis();

  while (received < len) {
    if (!client.connected()) return false;

    if (client.available() == 0) {
      if (millis() - lastByteTime > 5000) return false;
      taskYIELD();
      continue;
    }

    int n = client.read(buf + received, len - received);
    if (n > 0) {
      received += n;
      lastByteTime = millis();
    }
  }

  return true;
}

bool waitForMagic(WiFiClient &client) {
  uint8_t prev = 0, curr = 0;
  unsigned long lastByteTime = millis();

  while (client.connected()) {
    if (client.available() == 0) {
      if (millis() - lastByteTime > 5000) return false;
      taskYIELD();
      continue;
    }

    curr = client.read();
    lastByteTime = millis();

    if (prev == 0xAA && curr == 0x55) return true;
    prev = curr;
  }

  return false;
}

uint16_t readU16BE(uint8_t *buf) {
  return ((uint16_t)buf[0] << 8) | buf[1];
}

uint32_t readU32BE(uint8_t *buf) {
  return ((uint32_t)buf[0] << 24) |
         ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8)  |
         ((uint32_t)buf[3]);
}

uint8_t checksumU8(uint8_t *data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++) sum += data[i];
  return sum & 0xFF;
}

inline uint8_t transferByte(uint8_t value) {
  return spi.transfer(value);
}

uint8_t sendFrameToFPGA(uint8_t frameId, uint16_t width, uint16_t height) {
  uint16_t pixel_sum = 0;
  for (uint32_t i = 0; i < IMG_BYTES; i++) pixel_sum += image_buf[i];

  spi.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  delayMicroseconds(10);

  transferByte(0xA5);
  transferByte(frameId);
  transferByte((width  >> 8) & 0xFF);
  transferByte( width        & 0xFF);
  transferByte((height >> 8) & 0xFF);
  transferByte( height       & 0xFF);

  spi.transferBytes(image_buf, nullptr, IMG_BYTES);

  transferByte((pixel_sum >> 8) & 0xFF);
  transferByte( pixel_sum       & 0xFF);

  uint8_t response = transferByte(0x00);

  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();

  return response;
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  Serial.println("\nESP32 starting...");

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(500);
  Serial.println("SPI ready.");

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("TCP server started on port 5000.");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("\nRaspberry Pi connected.");

  unsigned long frameCount = 0;
  unsigned long fpsStart   = millis();

  fpga_pass_count     = 0;
  fpga_fail_count     = 0;
  checksum_fail_count = 0;

  while (client.connected()) {

    if (!waitForMagic(client)) {
      Serial.println("Lost connection waiting for magic.");
      break;
    }

    uint8_t header[9];
    if (!readExact(client, header, 9)) {
      Serial.println("Failed to read header.");
      break;
    }

    uint16_t width      = readU16BE(&header[0]);
    uint16_t height     = readU16BE(&header[2]);
    uint8_t  frameId    = header[4];
    uint32_t payloadLen = readU32BE(&header[5]);

    if (width != IMG_W || height != IMG_H || payloadLen != IMG_BYTES) {

      Serial.printf("Bad header: %dx%d bytes=%d\n", width, height, payloadLen);
      break;
    }

    if (!readExact(client, image_buf, IMG_BYTES)) {
      Serial.println("Failed to read payload.");
      break;
    }

    uint8_t receivedChecksum = 0;
    if (!readExact(client, &receivedChecksum, 1)) {
      Serial.println("Failed to read checksum.");
      break;
    }

    bool checksumPass = (receivedChecksum == checksumU8(image_buf, IMG_BYTES));

    if (!checksumPass) {
      checksum_fail_count++;

      continue;
    }

    last_fpga_response = sendFrameToFPGA(frameId, width, height);

    if      (last_fpga_response == 0xAA) fpga_pass_count++;
    else if (last_fpga_response == 0xEE) fpga_fail_count++;

    frameCount++;

    unsigned long now = millis();
    if (now - fpsStart >= 1000) {
      float fps = frameCount * 1000.0f / (now - fpsStart);

      Serial.printf(
        "FPS=%.1f | frame=%d | FPGA pass=%d fail=%d | chk_fail=%d\n",
        fps, frameId, fpga_pass_count, fpga_fail_count, checksum_fail_count
      );

      frameCount = 0;
      fpsStart   = now;
    }
  }

  client.stop();
  Serial.println("Raspberry Pi disconnected.");
}