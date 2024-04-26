#include <ESP32Time.h>       // For managing time on ESP32
#include <MPU9250_asukiaaa.h> // For MPU9250 sensor interface
#include <SD.h>              // For SD card operations
#include <SPI.h>             // For SPI communication protocols
#include <TinyGPSPlus.h>     // For GPS functionalities
#include <Wire.h>            // For I2C communication
#include "configSetup.h"          // Configuration file

MPU9250_asukiaaa mySensor; // Sensor interface object
TinyGPSPlus gps;                  // GPS data parser
String dataMessage;               // Data message storage for SD writing
File dataFile;                    // File object for SD operations
String fileName;
// Time data structure
struct TimeData {
  int day, month, year, hour, minute, second;
} rtcTime, gpsTime;

int gpsUpdate = 0; // GPS data update flag

char latitudeStr[15], longitudeStr[15]; // Human-readable latitude and longitude
double gpsAltitude = 0;                 // GPS altitude
float gpsSpeed = 0;                     // GPS speed

// Sensor data structure
struct SensorData {
  float ax, ay, az;          // Accelerometer readings
  float gx, gy, gz;          // Gyroscope readings
  float aSqrt;               // Square root of accelerometer sums
  float gxOffset = 0, gyOffset = 0, gzOffset = 0; // Gyroscope offsets
} sensorData;


void setup() {
  
  initProgram(); // Setup serial and i2c communication
  checkAndCalibrateMPU(); // Function to check connection with MPU and calibrate gyroscope
  syncGps(); // Synchronize the internal RTC with GPS time and GPS latitude and longitude
  updateTimeDateRTCdata(); // Update internal time and date with RTC data
  initSDCard(); // Initialize the SD card module
  checkSDFile(); // Ensure '.csv' file exists on the SD card, create if not found using date and hour in main name
  waitStart(); // Sync to start collecting data in the next second

}

void loop() {
  // Get the current time in milliseconds since the program started
  unsigned long currentTime = millis();
  updateGpsData();
  // Check if it's time to collect and save data based on the predefined interval
  if (currentTime - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentTime; // Update the time of the last sample collection
    updateSampleIndex();
    updateTimeDateRTCdata();
    updateMPUdata();
    saveData(); // Save the collected data
    digitalWrite(LED_BUILTIN, HIGH); // Turn on the built-in LED to indicate data collection
    gpsUpdate = 0; // Reset the GPS update flag
  }
}


