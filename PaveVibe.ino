#include <MPU9250_asukiaaa.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <ESP32Time.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_Freq 400000
#endif

MPU9250_asukiaaa mySensor;
// Definições de controle de amostragem
const int amostrasPorSegundo = 10;
unsigned long tempoUltimaAmostra = 0;
const long intervaloAmostras = 1000 / amostrasPorSegundo; // Calcula o intervalo entre amostras em ms
int indiceAmostra = 0; // Inicia o índice da amostra

int LED_BUILTIN = 2;

// ESP32Time rtc;
ESP32Time rtc(-10800);  // offset in seconds GMT-3

#define GPS_BAUDRATE 9600  // the default baudrate of NEO-6M is 9600

TinyGPSPlus gps;  // the TinyGPS++ object

String dataMessage;  // save data to sdcard

File dataFile;


// Internal RTC Variables
int rtcdia, rtcmes, rtcano, rtchora, rtcminuto, rtcsegundo{ 0 };
// GPS Variables
int gpsdia, gpsmes, gpsano, gpshora, gpsminuto, gpssegundo{ 0 };
float gpslat, gpslong{ 0 };
char latitudeStr[15];
char longitudeStr[15];
double gpsalt = 0;
float gpsvel = 0;

// Sensor reading variables
float aX, aY, aZ, aSqrt;
float gX, gY, gZ;
float gXOffset = 0, gYOffset = 0, gZOffset = 0;

int gpsUpdate = 0; // Informs if the gps was updated on the last data

void setup() {
  Serial.begin(115200);
  Serial2.begin(GPS_BAUDRATE);

  Serial.println(F("PaveVibe - Coded by Jairo Ivo"));
  while (!Serial);

  initSDCard();
  checkSDFile();  // Check the data.csv file on the memory card or create it if it does not exist

  pinMode(LED_BUILTIN, OUTPUT); // Configura o pino do LED como saída

#ifdef _ESP32_HAL_I2C_H_
  Wire.begin(SDA_PIN, SCL_PIN, I2C_Freq);
  mySensor.setWire(&Wire);
#endif

  mySensor.beginAccel();
  mySensor.beginGyro();
  mySensor.beginMag();

  calculateGyroOffsets(); // Calcula e aplica os offsets do giroscópio

  blinkLed(2); // Pisca o LED 3 vezes para indicar a conclusão da calibração do acelerometro

  Serial.println(F("Initiating synchronization of the internal RTC with the gps!"));
  delay(500);
  while (rtc.getYear() < 2023) {
    Serial.println(F("."));
    rtcSyncWithGps();
    //delay(50);
  }

  blinkLed(4); // Pisca o LED 3 vezes para indicar a conclusão da calibração do relogio

}

void calculateGyroOffsets() {
  long gXTotal = 0, gYTotal = 0, gZTotal = 0;
  const int samples = 100;

  for(int i = 0; i < samples; i++) {
    while (mySensor.gyroUpdate() != 0);
    gXTotal += mySensor.gyroX();
    gYTotal += mySensor.gyroY();
    gZTotal += mySensor.gyroZ();
    delay(100);
  }

  gXOffset = gXTotal / samples;
  gYOffset = gYTotal / samples;
  gZOffset = gZTotal / samples;
}

void loop() {
  unsigned long tempoAtual = millis();
  if (tempoAtual - tempoUltimaAmostra >= intervaloAmostras) {
    saveData();
    tempoUltimaAmostra = tempoAtual; // Atualiza o tempo da última amostra
    digitalWrite(LED_BUILTIN, HIGH);
    // Incrementa e verifica o índice da amostra
    indiceAmostra++;
    if (indiceAmostra > amostrasPorSegundo) {
      indiceAmostra = 1; // Reinicia o índice após atingir o limite
    }
    
    Serial.print(F("- RTC date&time: ")); Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));  // (String) returns time with specified format
    rtcmes = rtc.getMonth(); rtcdia = rtc.getDay(); rtcano = rtc.getYear(); rtchora = rtc.getHour(true); rtcminuto = rtc.getMinute(); rtcsegundo = rtc.getSecond();

    // Exemplo de aplicação de offset nos valores do acelerômetro (ajustar conforme necessário)
    if (mySensor.accelUpdate() == 0) {
      aX = mySensor.accelX();
      aY = mySensor.accelY();
      aZ = mySensor.accelZ();
      aSqrt = mySensor.accelSqrt();
    }

    // Aplica o offset aos valores do giroscópio (ajustar conforme necessário)
    if (mySensor.gyroUpdate() == 0) {
      gX = mySensor.gyroX() - gXOffset;
      gY = mySensor.gyroY() - gYOffset;
      gZ = mySensor.gyroZ() - gZOffset;
    }
    Serial.println(indiceAmostra);
    // A função blinkLed não é chamada no loop principal para não afetar a taxa de amostragem
  }
  gpsUpdate = 0;
  if ((rtc.getSecond()) != rtcsegundo) {
    if (Serial2.available() > 0) {
      if (gps.encode(Serial2.read())) {
        //digitalWrite(LED_BUILTIN, LOW);
        if (gps.location.isValid()) {
          digitalWrite(LED_BUILTIN, LOW);
          dtostrf(gps.location.lat(), 12, 8, latitudeStr);
          dtostrf(gps.location.lng(), 12, 8, longitudeStr);
          gpsUpdate = 1;
          if (gps.altitude.isValid()) {
            gpsalt = (gps.altitude.meters());
          } else {
            Serial.println(F("- alt: INVALID"));
            delay(150);
          }
        } else {
          Serial.println(F("- location: INVALID"));
          delay(150);
        }
        if (gps.speed.isValid()) {
          gpsvel = (gps.speed.kmph());
        } else {
          Serial.println(F("- speed: INVALID"));
          delay(150);
        }
        if (gps.date.isValid() && gps.time.isValid()) {
          gpsano = (gps.date.year());
          gpsmes = (gps.date.month());
          gpsdia = (gps.date.day());
          gpshora = (gps.time.hour());
          gpsminuto = (gps.time.minute());
          gpssegundo = (gps.time.second());
        } else {
          Serial.println(F("- gpsDateTime: INVALID"));
          delay(150);
        }
        //gpsUpdate = 1;
        //Serial.println();
      }
    }
  }
}

void blinkLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
}

void rtcSyncWithGps() {
  if (Serial2.available() > 0) {
    //delay(150);
    if (gps.encode(Serial2.read())) {
      delay(150);
      Serial.print(F("- GPS date&time: "));
      if (gps.date.isValid() && gps.time.isValid()) {
        Serial.print(gps.date.year()); Serial.print(F("-")); Serial.print(gps.date.month()); Serial.print(F("-")); Serial.print(gps.date.day()); Serial.print(F(" ")); 
        Serial.print(gps.time.hour()); Serial.print(F(":")); Serial.print(gps.time.minute()); Serial.print(F(":")); Serial.println(gps.time.second());
        rtc.setTime((gps.time.second()), (gps.time.minute()), (gps.time.hour()), (gps.date.day()), ((gps.date.month()) + 1), (gps.date.year()));  // 17th Jan 2021 15:24:30
        //rtc.setTime(1609459200);  // 1st Jan 2021 00:00:00
        //rtc.offset = 7200; // change offset value
        Serial.print(F("- RTC date&time: ")); Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));  // (String) returns time with specified format
        // formating options  http://www.cplusplus.com/reference/ctime/strftime/
      } else {
        Serial.println(F("No valid date and time data!"));
      }
      Serial.println();
    }
  }
  if (millis() > 5000 && gps.charsProcessed() < 10)
    Serial.println(F("No valid gps data: check connection"));
}
