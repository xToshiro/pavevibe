// Retorna o nome do arquivo baseado na data e hora atual
String getFileName() {
  char fileName[25];  // Buffer para armazenar o nome do arquivo
  snprintf(fileName, sizeof(fileName), "/D%02d-%02d-%04d--H%02d.csv", rtcdia, rtcmes, rtcano, rtchora);
  return String(fileName);
}

// Verifica se o arquivo existe e, se não, cria com o cabeçalho apropriado
void checkSDFile() {
  String fileName = getFileName();
  File file = SD.open(fileName.c_str());
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    delay(1000);
    writeFile(SD, fileName.c_str(), "RTCData, RTCHora, GPSData, GPSHora, Lat, Long, Altgps, Vel, GPSUpdate, Ax, Ay, Az, Gx, Gy, Gz, indiceAmostra \r\n");
  } else {
    Serial.println("File already exists");
  }
  file.close();
  delay(1000);
}


// Inicializa o cartão SD e verifica seu status e tamanho
void initSDCard() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    ErrorLoopIndicator();
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    ErrorLoopIndicator();
    return;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// Escreve no arquivo se ele não existir
void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    //Serial.println("Failed to open file for writing");
    ErrorLoopIndicator();
    return;
  }
  if (file.print(message)) {
    //Serial.println("File written");
  } else {
    //Serial.println("Write failed");
    ErrorLoopIndicator();
  }
  file.close();
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
  // Monta a mensagem de dados a ser gravada
  String dataMessage = String(rtcdia) + "/" + String(rtcmes) + "/" + String(rtcano) + "," + String(rtchora) + ":" + String(rtcminuto) + ":" + String(rtcsegundo) + "," + String(gpsdia) + "/" + String(gpsmes) + "/" + String(gpsano) + "," + String(gpshora) + ":" + String(gpsminuto) + ":" + String(gpssegundo) + "," + String(latitudeStr) + "," + String(longitudeStr) + ","
                      + String(gpsalt) + "," + String(gpsvel) + "," + String(gpsUpdate) + "," + String(aX) + "," + String(aY) + "," + String(aZ) + "," + String(gX) + "," + String(gY) + "," + String(gZ) + "," + String(indiceAmostra) + "\r\n";

  // Mostra a mensagem no Serial Monitor
  Serial.print(F("- data appended: ")); Serial.print(dataMessage);

  // Obtem o nome do arquivo baseado na data e hora atual
  String fileName = getFileName();

  // Adiciona a mensagem de dados ao arquivo
  appendFile(SD, fileName.c_str(), dataMessage.c_str());
}
