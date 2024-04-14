constexpr int SDA_PIN = 21;        // SDA pin for I2C communication
constexpr int SCL_PIN = 22;        // SCL pin for I2C communication
constexpr unsigned long I2C_Freq = 400000; // I2C frequency in Hz

// Sampling configuration
constexpr int samplesPerSecond = 10;
unsigned long lastSampleTime = 0; // Time of the last sample
constexpr long sampleInterval = 1000 / samplesPerSecond;
int sampleIndex = 0; // Sample counter

constexpr int LED_BUILTIN = 2;    // Built-in LED pin on ESP32
ESP32Time rtc(-10800);            // ESP32Time object with GMT-3 offset
constexpr int GPS_BAUDRATE = 9600; // Baud rate for GPS module