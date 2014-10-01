#include <EtherCard.h>

// Biblioteca DHT11 -> http://playground.arduino.cc/Main/DHT11Lib
#include <dht11.h>


/* Arduino PINs */
const int coolerPin      = 5;
const int ledLampPin     = 6;
const int warmingLampPin = 7;
const int tempSensorPin  = 9;

/* Control variables */
float actualTemperature = 0.0;
float actualHumidity    = 0.0;

/* Limits and intervals */
const float minTemperature = 28.0;
const float maxTemperature = 31.0;

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


int readTemperature(float& temperature, float& humidity) {
    dht11 DHT11;
    int chk = DHT11.read(tempSensorPin);

#ifdef DEBUG
    Serial.println("DHT11 reading");
#endif

    switch (chk) {
        case DHTLIB_OK:
            temperature = (float)DHT11.temperature;
            humidity = (float)DHT11.humidity;

#ifdef DEBUG
  Serial.print("Temperature (°C): ");
  Serial.println(temperature, 2);
  Serial.print("Humidity (%): ");
  Serial.println(humidity, 2);
#endif

            break;
        case DHTLIB_ERROR_CHECKSUM:
        case DHTLIB_ERROR_TIMEOUT:
        default:
            // Temperature/Humidity read failed
            temperature = -1.0;
            humidity    = -1.0;
            return -1;
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

int sendActualStatusToServer(float temperature, float humidity) {
#ifdef DEBUG
    Serial.println("Sending data to server");
#endif

    byte sd = stash.create();
    stash.print("temperature=");
    stash.print(temperature);
    stash.print("&humidity=");
    stash.print(humidity);
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

    readTemperature(actualTemperature, actualHumidity);
}


void loop() {
    unsigned long currentMillis = millis();

    ether.packetLoop(ether.packetReceive());

    if(currentMillis - previousMillis > readInterval * 1000 * 60) {
        previousMillis = currentMillis;
        readTemperature(actualTemperature, actualHumidity);

        /* Actions after sensor read */
        updateWarmingStatus(actualTemperature, actualHumidity);

        if(ethernetAvailable) {
            sendActualStatusToServer(actualTemperature, actualHumidity);
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

