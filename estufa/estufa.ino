#include <EtherCard.h>

// Biblioteca DHT11 -> http://playground.arduino.cc/Main/DHT11Lib
#include <dht11.h>

/* Arduino PINs */
const int coolerPin      = 5;
const int ledLampPin     = 6;
const int warmingLampPin = 7;
const int internalTempSensorPin  = 9;
const int externalTempSensorPin  = 5;

/* Control variables */
float actualInternalTemperature = 0.0;
float actualInternalHumidity    = 0.0;
float actualExternalTemperature = 0.0;
float actualExternalHumidity    = 0.0;

/* Limits and intervals */
const float minTemperature = 28.0;
const float maxTemperature = 29.0;

/* Auxiliary variables */
const long readInterval = 1;  // in minutes
long previousMillis = 0; // will store last time the read interval was executed

/* PIN status */
int ledLampStatus        = LOW;
int warmingLampPinStatus = LOW;
int coolerPinStatus      = LOW;

/* Ethernet connection settings */
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
byte Ethernet::buffer[700];
static uint32_t timer;
Stash stash;
const char website[] PROGMEM = "estufa.rennerocha.com";
int ethernetAvailable = 0;


int readTemperature(float& internalTemperature, float& internalHumidity, float& externalTemperature, float& externalHumidity) {
    dht11 internalDHT11;
    dht11 externalDHT11;

    int internalChk = internalDHT11.read(internalTempSensorPin);
    int externalChk = externalDHT11.read(externalTempSensorPin);

#ifdef DEBUG
    Serial.println("DHT11 reading");
#endif

    switch (internalChk) {
        case DHTLIB_OK:
            internalTemperature = (float)internalDHT11.temperature;
            internalHumidity = (float)internalDHT11.humidity;

#ifdef DEBUG
  Serial.print("Internal Temperature (°C): ");
  Serial.println(internalTemperature, 2);
  Serial.print("Internal Humidity (%): ");
  Serial.println(internalHumidity, 2);
#endif

            break;
        case DHTLIB_ERROR_CHECKSUM:
        case DHTLIB_ERROR_TIMEOUT:
        default:
            // Temperature/Humidity read failed
            internalTemperature = -1.0;
            internalHumidity    = -1.0;
    }
    
    switch (externalChk) {
        case DHTLIB_OK:
            externalTemperature = (float)externalDHT11.temperature;
            externalHumidity = (float)externalDHT11.humidity;

#ifdef DEBUG
  Serial.print("External Temperature (°C): ");
  Serial.println(internalTemperature, 2);
  Serial.print("External Humidity (%): ");
  Serial.println(internalHumidity, 2);
#endif

            break;
        case DHTLIB_ERROR_CHECKSUM:
        case DHTLIB_ERROR_TIMEOUT:
        default:
            // Temperature/Humidity read failed
            externalTemperature = -1.0;
            externalHumidity    = -1.0;
    }
    
    return 0;
}

int updateWarmingStatus(float temperature, float humidity) {
    int actualWarmingLampPinStatus = warmingLampPinStatus;

#ifdef DEBUG
  Serial.println("Updating warming status.");
#endif

    if(temperature < minTemperature) {
#ifdef DEBUG
  Serial.println("Starting warming.");
#endif
        warmingLampPinStatus = HIGH;
    } else if (temperature > maxTemperature) {
        // TODO Use cooler to raise temperature
#ifdef DEBUG
  Serial.println("Stopping warming.");
#endif
        warmingLampPinStatus = LOW;
    }

    if(actualWarmingLampPinStatus != warmingLampPinStatus) {
        digitalWrite(warmingLampPin, warmingLampPinStatus);
    }

    return 0;
}

int sendActualStatusToServer(float internalTemperature, float internalHumidity, float externalTemperature, float externalHumidity) {
#ifdef DEBUG
    Serial.println("Sending data to server");
#endif

    byte sd = stash.create();
    stash.print("internalTemperature=");
    stash.print(internalTemperature);
    stash.print("&internalHumidity=");
    stash.print(internalHumidity);
    stash.print("externalTemperature=");
    stash.print(externalTemperature);
    stash.print("&externalHumidity=");
    stash.print(externalHumidity);
    stash.print("&ledLampStatus=");
    stash.print(ledLampStatus);
    stash.print("&warmingLampPinStatus=");
    stash.print(warmingLampPinStatus);
    stash.print("&coolerPinStatus=");
    stash.print(coolerPinStatus);
    stash.save();

    int stash_size = stash.size();

    Stash::prepare(PSTR("POST http://$F/estufa HTTP/1.1" "\r\n"
      "Host: $F" "\r\n"
      "Content-Length: $D" "\r\n"
      "\r\n"
      "$H"), website, website, stash_size, sd);

    ether.tcpSend();
}

void setup() {

#ifdef DEBUG
  Serial.begin(57600);
  Serial.println("Starting greenhouse.");
#endif

    pinMode(ledLampPin, OUTPUT);
    digitalWrite(ledLampPin, ledLampStatus);

    pinMode(warmingLampPin, OUTPUT);
    digitalWrite(warmingLampPin, warmingLampPinStatus);

    pinMode(coolerPin, OUTPUT);
    digitalWrite(coolerPin, coolerPinStatus);

    ethernetAvailable = (
        ether.begin(sizeof Ethernet::buffer, mymac) != 0 
        && ether.dhcpSetup()
        && ether.dnsLookup(website));

#ifdef DEBUG
  Serial.print("Ethernet available: ");
  Serial.println(ethernetAvailable);
#endif

    readTemperature(actualInternalTemperature, actualInternalHumidity, actualExternalTemperature, actualExternalHumidity);
}


void loop() {
    unsigned long currentMillis = millis();

    ether.packetLoop(ether.packetReceive());

    if(currentMillis - previousMillis > readInterval * 1000 * 60) {
        previousMillis = currentMillis;
        readTemperature(actualInternalTemperature, actualInternalHumidity, actualExternalTemperature, actualExternalHumidity);

        /* Actions after sensor read */
        updateWarmingStatus(actualInternalTemperature, actualInternalHumidity);

        if(ethernetAvailable) {
            sendActualStatusToServer(actualInternalTemperature, actualInternalHumidity, actualExternalTemperature, actualExternalHumidity);
        } else {
            ethernetAvailable = (
              ether.begin(sizeof Ethernet::buffer, mymac) != 0 
              && ether.dhcpSetup()
              && ether.dnsLookup(website));
#ifdef DEBUG
  Serial.print("Ethernet available: ");
  Serial.println(ethernetAvailable);
#endif
        }
    }
}

