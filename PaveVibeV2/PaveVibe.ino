#include <MPU9250_asukiaaa.h>  // Include MPU9250 sensor library
#include <Wire.h>             // Include Wire library for I2C
#include <TinyGPSPlus.h>        // Include TinyGPS++ library for GPS functionality
#include <ESP32Time.h>        // Include ESP32Time library for time functionality
#include <SD.h>               // Include SD library for SD card functionality
#include <SPI.h>              // Include SPI library for SPI communication

// Check if the ESP32 I2C hardware abstraction layer is available
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 21           // Define SDA pin
#define SCL_PIN 22           // Define SCL pin
#define I2C_Freq 400000      // Define I2C frequency as 400kHz
#endif

MPU9250_asukiaaa mySensor;    // Create an object for the MPU9250 sensor

// Sampling control definitions
const int amostrasPorSegundo = 10;                        // Samples per second
unsigned long tempoUltimaAmostra = 0;                     // Time of the last sample
const long intervaloAmostras = 1000 / amostrasPorSegundo; // Interval between samples in ms
int indiceAmostra = 0;                                    // Sample index initialization

int LED_BUILTIN = 2;  // Define the built-in LED pin

ESP32Time rtc(-10800); // Initialize ESP32Time object with an offset for GMT-3
#define GPS_BAUDRATE 9600  // Define the default baudrate for NEO-6M GPS module
TinyGPSPlus gps;   // Create a TinyGPS++ object for handling GPS data
String dataMessage;  // String to save data for SD card writing
File dataFile;  // File object for SD card operations
int rtcdia, rtcmes, rtcano, rtchora, rtcminuto, rtcsegundo, gpsdia, gpsmes, gpsano, gpshora, gpsminuto, gpssegundo, gpsUpdate{ 0 }; // Internal RTC and GPS time variables // Flag to indicate if GPS was updated on the last data
char latitudeStr[15], longitudeStr[15];        // String to store latitude and longitude
double gpsalt = 0;           // GPS altitude
float gpsvel = 0;            // GPS speed
// Sensor reading variables
float aX, aY, aZ, gX, gY, gZ, aSqrt, gXOffset, gYOffset, gZOffset{ 0 }; // Accelerometer and Gyroscope readings for X, Y, and Z axes // Square root of the sum of squares of the accelerometer readings, can be used for motion detection or other calculations // Offsets for gyroscope readings to calibrate and remove drift

void setup() {
  Serial.begin(115200); // Initialize serial communication at 115200 bits per second
  Serial2.begin(GPS_BAUDRATE); // Initialize GPS module serial communication
  delay(2000);
  clearSerialScreen();
  // Display startup message
  Serial.println(F("PaveVibe - Coded by Jairo Ivo"));
  while (!Serial); // Wait for the serial port to connect. Needed for native USB

  pinMode(LED_BUILTIN, OUTPUT); // Set the built-in LED pin as an output

//  Wire.begin(); // Initialize I2C bus; default pins are SDA=21 and SCL=22 on ESP32
  
  // Initialize I2C communication for MPU9250 sensor if ESP32 I2C HAL is defined
#ifdef _ESP32_HAL_I2C_H_
  Wire.begin(SDA_PIN, SCL_PIN, I2C_Freq); // Initialize I2C communication at specified pins and frequency
  mySensor.setWire(&Wire); // Set the Wire object for the sensor
#endif

  // Check for device at address 0x68
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) {
    ErrorLoopIndicator(); // Call error indicator loop if device is not found
  } else {
    // Initialize the MPU9250 sensor (accelerometer, gyroscope, magnetometer)
    mySensor.beginAccel();
    mySensor.beginGyro();
    mySensor.beginMag();
  }

  calculateGyroOffsets(); // Calculate and apply gyroscope offsets
  blinkLed(2); // Blink the LED 2 times to indicate accelerometer calibration completion

  Serial.println(F("Initiating synchronization of the internal RTC with the GPS!"));
  delay(500); // Short delay before starting the synchronization process

  // Wait until the RTC year is valid (assuming year is at least 2023 for this project)
  rtcSyncWithGps(); // Attempt to synchronize RTC with GPS
  blinkLed(4); // Blink the LED 4 times to indicate clock calibration completion

  syncGpsLatLong();
  updateTimeDateRTCdata();

  initSDCard(); // Initialize SD card
  checkSDFile(); // Check for data.csv on the SD card, create it if missing

  rtcsegundo = rtc.getSecond();
  while(rtcsegundo == rtc.getSecond()){
    Serial.println("Wait to start...");
  }

}

void calculateGyroOffsets() {
  // Initialize accumulators for gyro readings
  long gXTotal = 0, gYTotal = 0, gZTotal = 0;
  const int samples = 100; // Number of samples to collect for calibration

  // Collect 'samples' number of gyro readings
  for (int i = 0; i < samples; i++) {
    // Wait for a new gyro reading to be available
    while (mySensor.gyroUpdate() != 0);

    // Accumulate the gyro readings
    gXTotal += mySensor.gyroX();
    gYTotal += mySensor.gyroY();
    gZTotal += mySensor.gyroZ();

    // Introduce a delay between readings to allow for fresh data
    // Note: Consider reducing or removing this delay if real-time performance is critical
    delay(100);
  }

  // Calculate the average of the accumulated readings to find the offset
  gXOffset = gXTotal / samples;
  gYOffset = gYTotal / samples;
  gZOffset = gZTotal / samples;
}

void updateTimeDateRTCdata(){

  // Update RTC time variables
  rtcmes = rtc.getMonth(); rtcdia = rtc.getDay(); rtcano = rtc.getYear();
  rtchora = rtc.getHour(true); rtcminuto = rtc.getMinute(); rtcsegundo = rtc.getSecond();

}

void updateMPUdata(){

  // Update accelerometer values with the latest readings
  if (mySensor.accelUpdate() == 0) {
    aX = mySensor.accelX();
    aY = mySensor.accelY();
    aZ = mySensor.accelZ();
    aSqrt = mySensor.accelSqrt(); // The square root of the sum of squares of the accelerometer readings
  }
  // Apply offset to gyroscope values and update
  if (mySensor.gyroUpdate() == 0) {
    gX = mySensor.gyroX() - gXOffset;
    gY = mySensor.gyroY() - gYOffset;
    gZ = mySensor.gyroZ() - gZOffset;
  }

}

void loop() {
  // Get the current time in milliseconds since the program started
  unsigned long tempoAtual = millis();

    // Check if a second has passed to reduce GPS data processing load
  if ((rtc.getSecond()) != rtcsegundo) {
    // Check if there is any data available from the GPS module
    if (Serial2.available() > 0) {
      // Try to parse GPS data
      if (gps.encode(Serial2.read())) {
        // Check if the location data is valid
        if (gps.location.isValid()) {
          digitalWrite(LED_BUILTIN, LOW); // Turn off the LED if a valid location is obtained
          // Convert latitude and longitude to string with fixed format
          dtostrf(gps.location.lat(), 12, 8, latitudeStr); dtostrf(gps.location.lng(), 12, 8, longitudeStr);
          gpsUpdate = 1; // Set GPS update flag
          // Check if altitude data is valid
          if (gps.altitude.isValid()) {
            gpsalt = gps.altitude.meters(); // Update altitude
          }
          // Check if speed data is valid
          if (gps.speed.isValid()) {
            gpsvel = gps.speed.kmph(); // Update speed
          }
          // Check if GPS date and time data is valid
          if (gps.date.isValid() && gps.time.isValid()) {
            // Update GPS date and time variables
            gpsano = gps.date.year(); gpsmes = gps.date.month(); gpsdia = gps.date.day(); gpshora = gps.time.hour(); gpsminuto = gps.time.minute(); gpssegundo = gps.time.second();
          }
        }
      }
    }
  }

  // Check if it's time to collect and save data based on the predefined interval
  if (tempoAtual - tempoUltimaAmostra >= intervaloAmostras) {
    tempoUltimaAmostra = tempoAtual; // Update the time of the last sample collection

    // Increment and check the sample index, reset if it exceeds samples per second
    indiceAmostra++;
    if (indiceAmostra > amostrasPorSegundo) {
      indiceAmostra = 1; // Reset index after reaching the limit
      Serial.println();
    }
    updateTimeDateRTCdata();
    updateMPUdata();
    // Log the current sample index
    //Serial.println(indiceAmostra);
    // Reset the GPS update flag
    saveData(); // Save the collected data
    gpsUpdate = 0;
    digitalWrite(LED_BUILTIN, HIGH); // Turn on the built-in LED to indicate data collection

  }

}

void blinkLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
    delay(500); // Wait for half a second
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
    delay(500); // Wait for half a second
  }
}

void ErrorLoopIndicator(){
  while(1){
      digitalWrite(LED_BUILTIN, HIGH);
      delay(150);
      digitalWrite(LED_BUILTIN, LOW);
      delay(150);
  }
}

void clearSerialScreen() {
    for (int i = 0; i < 30; i++) {
        Serial.println();
    }
}

void syncGpsLatLong() {
  while((gps.location.lat()) == 0){
    if (Serial2.available() > 0) {
      //delay(150);
      if (gps.encode(Serial2.read())) {
        if (gps.location.isValid()) {
            // Convert latitude and longitude to string with fixed format
            dtostrf(gps.location.lat(), 12, 8, latitudeStr); dtostrf(gps.location.lng(), 12, 8, longitudeStr);
            gpsUpdate = 1; // Set GPS update flag
            // Print the formatted latitude and longitude strings
            Serial.print("Latitude: "); Serial.println(latitudeStr); Serial.print("Longitude: "); Serial.println(longitudeStr);
        }
      }
    }
  }
}

void rtcSyncWithGps() {
  while (rtc.getYear() < 2023 || rtc.getYear() > 2026) {
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
          Serial.println(F("Waiting for date and time data!"));
        }
        //Serial.println();
      }
    }
    if (millis() > 17000 && gps.charsProcessed() < 10)
      Serial.println(F("No valid gps data."));
  }
}
