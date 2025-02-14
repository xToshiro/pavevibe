#include <Wire.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <MPU9250_asukiaaa.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Configurações do Wi-Fi
const char* ssid = "TramaWifi";
const char* password = "tramawifi@det";

// Servidores NTP
const char* ntpServer1 = "a.st1.ntp.br";
const char* ntpServer2 = "b.st1.ntp.br";
const char* ntpServer3 = "d.st1.ntp.br";

// Fuso horário local (-3 horas)
const long utcOffsetInSeconds = -3 * 3600;

// Inicializa o cliente UDP e NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer1, utcOffsetInSeconds);

// Inicializa o RTC
RTC_DS3231 rtc;

// Inicializa o MPU9250 em um segundo barramento I2C
TwoWire I2C_MPU = TwoWire(1);
MPU9250_asukiaaa mySensor;

// Variável para índice de dados
int dataIndex = 0;
unsigned long lastEpoch = 0;

// Inicializa o SD card
const int chipSelect = 5; // Pino CS do módulo SD card
File dataFile;

// Variável para controle de tempo
unsigned long previousMicros = 0;
const unsigned long intervalMicros = 10000; // 10000 microssegundos = 10 milissegundos

// Pino do LED
const int ledPin = 2;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT); // Configura o pino do LED como saída

  // Inicializa o I2C para o RTC nos pinos SDA (26) e SCL (27)
  Wire.begin(26, 27);
  Wire.setClock(400000); // Configura o I2C para Fast Mode (400kHz)

  if (!rtc.begin()) {
    Serial.println("Não foi possível encontrar o RTC DS3231");
    blinkLED();
  }

  if (rtc.lostPower()) {
    Serial.println("O RTC perdeu a energia, ajustando a data e hora...");

    if (checkRTCDate()) {
      adjustRTCWithNTP();
    }
  }

  if (!checkRTCDate()) {
    adjustRTCWithNTP();
  }

  // Inicializa o MPU9250 no segundo barramento I2C nos pinos SDA (21) e SCL (22)
  I2C_MPU.begin(32, 33);
  I2C_MPU.setClock(400000); // Configura o I2C para Fast Mode (400kHz)
  mySensor.setWire(&I2C_MPU);
  mySensor.beginAccel();  // Inicializa o acelerômetro

  // Verifica se o acelerômetro está funcionando corretamente
  if (mySensor.accelUpdate() != 0) {
    Serial.println("Falha ao detectar o acelerômetro");
    blinkLED();
  }

  // Inicializa lastEpoch com o tempo atual
  lastEpoch = rtc.now().unixtime();
  previousMicros = micros(); // Inicializa a variável de tempo

  // Criação da string intermediária para o nome do arquivo
  String filePrefix = "/";
  String fileSuffix = ".csv";
  String epochString = String(lastEpoch);
  String completeFileName = filePrefix + epochString + fileSuffix;

  // Passa a string completa para o nome do arquivo
  String fileName = completeFileName;

  // Inicializa o SD card
  if (!SD.begin(chipSelect, SPI, 4000000)) {
    Serial.println("Falha ao inicializar o cartão SD");
    blinkLED();
  }

  // Cria ou abre o arquivo de dados no SD card com o nome baseado na epoch
  dataFile = SD.open(fileName, FILE_WRITE);
  if (!dataFile) {
    Serial.println("Falha ao abrir o arquivo de dados");
    blinkLED();
  }

  // Escreve o cabeçalho no arquivo CSV
  dataFile.println("Epoch Time, Data Index, Accel X, Accel Y, Accel Z, Accel Sqrt");
  dataFile.flush();

  // Exibe cabeçalho no serial monitor
  Serial.println("Epoch Time, Data Index, Accel X, Accel Y, Accel Z, Accel Sqrt");
}

void loop() {
  DateTime now = rtc.now();
  unsigned long currentEpoch = now.unixtime();

  // Exibe data e hora no início
  printEpochTime(currentEpoch);

  while (true) {
    now = rtc.now();
    currentEpoch = now.unixtime();
    if (currentEpoch != lastEpoch) {
      dataIndex = 0;
      lastEpoch = currentEpoch;
    }

    // Controle de tempo para coleta de dados
    unsigned long currentMicros = micros();
    if (currentMicros - previousMicros >= intervalMicros) {
      previousMicros += intervalMicros;
      collectAndPrintMPUData(currentEpoch);
      dataIndex++;
    }
  }
}

bool checkRTCDate() {
  DateTime now = rtc.now();
  return now.year() > 2022;
}

void adjustRTCWithNTP() {
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();

  // Tenta conectar ao WiFi por até 10 segundos
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar ao WiFi");
    blinkLED();
  } else {
    Serial.println("Conectado ao WiFi");
  }

  timeClient.begin();
  timeClient.update();

  // Tenta pegar a data e hora dos servidores NTP
  bool ntpSuccess = false;
  for (int i = 0; i < 3; i++) {
    timeClient.setPoolServerName(i == 0 ? ntpServer1 : (i == 1 ? ntpServer2 : ntpServer3));
    timeClient.update();
    if (timeClient.getEpochTime() > 0) {
      ntpSuccess = true;
      break;
    }
  }

  if (ntpSuccess) {
    unsigned long epochTime = timeClient.getEpochTime();
    DateTime ntpTime = DateTime(epochTime);
    rtc.adjust(ntpTime);
    Serial.println("Data e hora ajustadas com sucesso.");
  } else {
    Serial.println("Falha ao obter data e hora dos servidores NTP.");
    blinkLED();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi desligado.");
}

void printEpochTime(unsigned long epochTime) {
  Serial.println(epochTime);
}

void collectAndPrintMPUData(unsigned long epochTime) {
  if (mySensor.accelUpdate() == 0) {
    float aX = mySensor.accelX();
    float aY = mySensor.accelY();
    float aZ = mySensor.accelZ();
    float aSqrt = mySensor.accelSqrt();

    // Exibe os dados no serial monitor
    Serial.print(epochTime);
    Serial.print(", ");
    Serial.print(dataIndex);
    Serial.print(", ");
    Serial.print(aX, 6);
    Serial.print(", ");
    Serial.print(aY, 6);
    Serial.print(", ");
    Serial.print(aZ, 6);
    Serial.print(", ");
    Serial.println(aSqrt, 6);

    // Salva os dados no arquivo CSV
    dataFile.print(epochTime);
    dataFile.print(", ");
    dataFile.print(dataIndex);
    dataFile.print(", ");
    dataFile.print(aX, 6);
    dataFile.print(", ");
    dataFile.print(aY, 6);
    dataFile.print(", ");
    dataFile.print(aZ, 6);
    dataFile.print(", ");
    dataFile.println(aSqrt, 6);

    // Ajuste da chamada para flush() para ser feita em intervalos maiores
    if (dataIndex % 50 == 0) { // Chamada flush a cada 50 dados para melhorar performance
      dataFile.flush();
    }

  } else {
    Serial.println("Cannot read accel values");
    blinkLED();
  }
}

// Função para piscar o LED rapidamente em caso de erro
void blinkLED() {
  while (true) {
    digitalWrite(ledPin, HIGH);
    delay(100); // Liga o LED por 100ms
    digitalWrite(ledPin, LOW);
    delay(100); // Desliga o LED por 100ms
  }
}
