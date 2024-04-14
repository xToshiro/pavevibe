void updateTimeDateRTCdata() {
  // Update RTC time variables
  rtcTime.month = (rtc.getMonth() + 1); rtcTime.day = rtc.getDay(); rtcTime.year = rtc.getYear();
  rtcTime.hour = rtc.getHour(true); // true for 24-hour format
  rtcTime.minute = rtc.getMinute(); rtcTime.second = rtc.getSecond();
}

void updateMPUdata() {
  // Update accelerometer values with the latest readings
  if (mySensor.accelUpdate() == 0) {
    sensorData.ax = mySensor.accelX(); sensorData.ay = mySensor.accelY(); sensorData.az = mySensor.accelZ(); sensorData.aSqrt = mySensor.accelSqrt(); // Square root of sum of squares of accelerometer readings
  }
  // Apply offsets to gyroscope values and update
  if (mySensor.gyroUpdate() == 0) {
    sensorData.gx = mySensor.gyroX() - sensorData.gxOffset; sensorData.gy = mySensor.gyroY() - sensorData.gyOffset; sensorData.gz = mySensor.gyroZ() - sensorData.gzOffset;
  }
}

void blinkLed(int count) {
  // Blink the built-in LED a specified number of times
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
    delay(500); // Wait for half a second
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
    delay(500); // Wait for another half second
  }
}

void ErrorLoopIndicator() {
  // Continuously blink the LED to indicate an error
  while(1) {
      digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
      delay(100); // Wait for 150 milliseconds
      digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
      delay(100); // Wait for 150 milliseconds
  }
}

void clearSerialScreen() {
  // Clear the serial monitor screen by printing 30 new lines
  for (int i = 0; i < 30; i++) {
    Serial.println();
  }
}

void syncGps() {
  // Synchronize the internal RTC with GPS time
  Serial.println(F("Initiating synchronization of the internal RTC with the GPS!"));
  delay(500); // Short delay before starting synchronization
  // Continue trying to synchronize RTC with GPS until the year is within valid range (2023-2026)
  while (rtc.getYear() < 2023 || rtc.getYear() > 2026) {
    if (Serial2.available() > 0) {
      if (gps.encode(Serial2.read())) {
        delay(150); // Small delay may be necessary for some GPS modules to process data
        if (gps.date.isValid() && gps.time.isValid()) {
          // Set the RTC time based on GPS time
          rtc.setTime(gps.time.second(), gps.time.minute(), gps.time.hour(), gps.date.day(), (gps.date.month()), gps.date.year());
        }
      }
    }
    // Provide feedback if no valid GPS data is received within a certain timeframe
    if (millis() > 17000 && gps.charsProcessed() < 10) {
      Serial.println("No valid gps data.");
    }
  }
  Serial.println("Good synchronization between RTC and GPS.");
  Serial.print("GPS date&time: ");
  // Print and set GPS date and time
  Serial.print(gps.date.year()); Serial.print("-"); Serial.print(gps.date.month()); Serial.print("-"); Serial.print(gps.date.day()); Serial.print(" ");
  Serial.print(gps.time.hour()); Serial.print(":"); Serial.print(gps.time.minute()); Serial.print(":"); Serial.println(gps.time.second());
  // Print the synchronized RTC date and time
  Serial.print("RTC date&time: "); Serial.println(rtc.getTime("%Y-%m-%d %H:%M:%S"));
  // Synchronize GPS latitude and longitude
  // Continue to attempt to get valid GPS data until latitude is non-zero
  Serial.println("Waiting for start location.");
  while(gpsUpdate == 0){
    if (Serial2.available() > 0) {
      if (gps.encode(Serial2.read())) {
        if (gps.location.isValid()) {
            // Convert latitude and longitude to string with fixed format
            dtostrf(gps.location.lat(), 12, 8, latitudeStr); dtostrf(gps.location.lng(), 12, 8, longitudeStr);
            gpsUpdate = 1; // Set GPS update flag
            // Print the formatted latitude and longitude strings
            Serial.print("Start location found: ");
            Serial.print("Latitude: "); Serial.print(latitudeStr);
            Serial.print("  /  Longitude: "); Serial.println(longitudeStr);
        }
      }
    }
  }
  blinkLed(4); // Blink LED four times to indicate setup completion
}

void updateSampleIndex() {
  // Increment and check the sample index, reset if it exceeds samples per second
  sampleIndex++;
  if (sampleIndex > samplesPerSecond) {
    Serial.println();
    sampleIndex = 1; // Reset index after reaching the limit
  }
}

void updateGpsData() {
  // Check if a second has passed to reduce GPS data processing load
  if ((rtc.getSecond()) != rtcTime.second) {
    // Check if there is any data available from the GPS module
    if (Serial2.available() > 0) {
      // Try to parse GPS data
      if (gps.encode(Serial2.read())) {
        // Check if the location data is valid
        if (gps.location.isValid()) {
          digitalWrite(LED_BUILTIN, LOW); // Turn off the LED if a valid location is obtained
          // Convert latitude and longitude to string with fixed format
          dtostrf(gps.location.lat(), 12, 8, latitudeStr); 
          dtostrf(gps.location.lng(), 12, 8, longitudeStr);
          gpsUpdate = 1; // Set GPS update flag
          // Update altitude if data is valid
          if (gps.altitude.isValid()) {
            gpsAltitude = gps.altitude.meters();
          }
          // Update speed if data is valid
          if (gps.speed.isValid()) {
            gpsSpeed = gps.speed.kmph();
          }
          // Update GPS time and date variables if data is valid
          if (gps.date.isValid() && gps.time.isValid()) {
            gpsTime.year = gps.date.year(); gpsTime.month = gps.date.month(); gpsTime.day = gps.date.day(); gpsTime.hour = gps.time.hour(); gpsTime.minute = gps.time.minute(); gpsTime.second = gps.time.second();
          }
        }
      }
    }
  }
}

void initProgram() {
  Serial.begin(115200); // Initialize serial communication at 115200 bps
  Serial2.begin(GPS_BAUDRATE); // Initialize GPS module serial communication
  delay(2000);
  clearSerialScreen();
  // Display startup message
  Serial.println(F("PaveVibe - Coded by Jairo Ivo"));
  while (!Serial); // Wait for the serial port to connect, necessary for native USB

  pinMode(LED_BUILTIN, OUTPUT); // Set the built-in LED pin as an output

  // Initialize I2C communication for the MPU9250 sensor
  Wire.begin(SDA_PIN, SCL_PIN, I2C_Freq); // Start I2C with custom pins and frequency
  mySensor.setWire(&Wire); // Assign Wire object to the sensor for communication
}

void checkAndCalibrateMPU() {
   // Check for sensor presence on the I2C bus at address 0x68
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) {
    ErrorLoopIndicator(); // Execute error handling routine if sensor is not detected
  } else {
    mySensor.beginAccel(); // Initialize the accelerometer
    mySensor.beginGyro();  // Initialize the gyroscope
    mySensor.beginMag();   // Initialize the magnetometer

    delay(1000);
    Serial.println(F("Calibrating gyroscope, do not move!"));
    delay(500);

    calculateGyroOffsets(); // Calculate gyroscope offsets for calibration
    blinkLed(2); // Blink LED twice to indicate successful sensor setup
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
    gXTotal += mySensor.gyroX(); gYTotal += mySensor.gyroY(); gZTotal += mySensor.gyroZ();
    // Introduce a delay between readings to allow for fresh data
    // Note: Consider reducing or removing this delay if real-time performance is critical
    delay(100);
  }
  // Calculate the average of the accumulated readings to find the offsets
  sensorData.gxOffset = gXTotal / samples; sensorData.gyOffset = gYTotal / samples; sensorData.gzOffset = gZTotal / samples;
  Serial.println(F("Good calibration."));
}

void waitStart() {
  delay(2000);
  Serial.println("Wait to start..."); // Delay start until the next second ticks
  int rtcSecond = rtc.getSecond(); // Store current second from RTC
  while(rtcSecond == rtc.getSecond()) {
  }
  clearSerialScreen();
}