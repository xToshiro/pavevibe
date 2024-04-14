// Returns the filename based on the current date and time
String getFileName() {
  char fileName[25];  // Buffer to store the filename
  // Format the filename with day, month, year, and hour from the RTC data
  snprintf(fileName, sizeof(fileName), "/D%02d-%02d-%04d--H%02d.csv", rtcTime.day, rtcTime.month, rtcTime.year, rtcTime.hour);
  return String(fileName);
}

// Check if the data file exists on the SD card and create it with a header if it does not or is empty
void checkSDFile() {
  fileName = getFileName();  // Get the current file name based on the RTC date and time
  Serial.print("Checking file: "); Serial.println(fileName);

  // Tenta abrir o arquivo apenas para leitura para verificar sua existência
  File file = SD.open(fileName.c_str(), FILE_READ);
  if (!file) {
    Serial.println("File does not exist or cannot be opened for reading.");
    file = SD.open(fileName.c_str(), FILE_WRITE);  // Cria um novo arquivo para escrita
    if (file) {
      Serial.println("File created. Writing header...");
      const char *header = "Date, Time, GPSDate, GPSTime, Latitude, Longitude, Altitude, Speed, GPSUpdate, AccX, AccY, AccZ, GyroX, GyroY, GyroZ, SampleIndex\r\n";
      if (file.print(header)) {
        Serial.println("Header written successfully.");
      } else {
        Serial.println("Failed to write header.");
        ErrorLoopIndicator();  // Indicate an error in a non-recoverable loop
      }
      file.close();
    } else {
      Serial.println("Failed to create file.");
      ErrorLoopIndicator();  // Indicate an error in a non-recoverable loop
    }
  } else {
    Serial.println("File exists, no need to create.");
    file.close();
  }
}

// Initialize the SD card and check its status and size
void initSDCard() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    ErrorLoopIndicator();  // Indicate an error in a non-recoverable loop
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    ErrorLoopIndicator();  // Indicate an error in a non-recoverable loop
    return;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);  // Convert bytes to megabytes
  Serial.printf("SD Card Size: %lluMB\n", cardSize);  // Print the size of the SD card
}

// Write to a file, creating it if it does not exist
void writeFile(fs::FS &fs, const char *path, const char *message) {
  //Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    //Serial.println("Failed to open file for writing");
    ErrorLoopIndicator();  // Indicate an error in a non-recoverable loop
    return;
  }
  if (file.print(message)) {
    //Serial.println("File written successfully");
  } else {
    //Serial.println("Write failed");
    ErrorLoopIndicator();  // Indicate an error if writing fails
  }
  file.close();  // Close the file to save the data properly
}

// Adiciona dados ao arquivo
void appendFile(fs::FS &fs, const char *path, const char *message) {
  //Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    //Serial.println("Failed to open file for appending");
    ErrorLoopIndicator();
    return;
  }
  if (file.print(message)) {
    //Serial.println("Message appended");
  } else {
    //Serial.println("Append failed");
    ErrorLoopIndicator();
  }
  file.close();
}

void saveData() {
  // Construct the data message to be saved on the SD card
  dataMessage = String(rtcTime.day) + "/" + String(rtcTime.month) + "/" + String(rtcTime.year) + "," 
              + String(rtcTime.hour) + ":" + String(rtcTime.minute) + ":" + String(rtcTime.second) + ","
              + String(gpsTime.day) + "/" + String(gpsTime.month) + "/" + String(gpsTime.year) + ","
              + String(gpsTime.hour) + ":" + String(gpsTime.minute) + ":" + String(gpsTime.second) + ","
              + String(latitudeStr) + "," + String(longitudeStr) + "," 
              + String(gpsAltitude) + "," + String(gpsSpeed) + "," + String(gpsUpdate) + ","
              + String(sensorData.ax) + "," + String(sensorData.ay) + "," + String(sensorData.az) + ","
              + String(sensorData.gx) + "," + String(sensorData.gy) + "," + String(sensorData.gz) + ","
              + String(sampleIndex) + "\r\n";
  // Display the data message in the Serial Monitor
  Serial.print("Data appended: "); Serial.print(dataMessage);
  // Append the data message to the file on the SD card
  appendFile(SD, fileName.c_str(), dataMessage.c_str());
}

