#include <Homie.h>
#include <DHT.h>

#define FW_NAME "garage-door"
#define FW_VERSION "1.5.0"
#define PIN_RELAY         D1
#define PIN_SENSOR        D7
#define OPENER_EVENT_MS   250
#define DEBOUNCER_MS      500     // Use a debounce interval of 500 milliseconds 
#define DHT_PIN           D5
#define DHT_TYPE          DHT11
#define PUB_INTERVAL      60
#define device_stats_interval 600 // 1.4.3 Set device stats interval to 10 mins

unsigned long lastTemperatureSent = 0;
unsigned long openerEvent         = 0;
int lastSensorValue               = 0;
volatile int sensorValue          = LOW; // we have methods reading potentially while we are writing this value: volatile

void publishSensorFunction();

DHT dht(DHT_PIN, DHT_TYPE);
Bounce debouncer = Bounce();     // Instantiate a Bounce object
HomieNode environmentNode("environmentMonitor", "Multi-Sensor", "sensor"); // Parameters are arbitrary values
HomieNode openerNode("doorOpener", "Relay", "switch");  

void onHomieEvent(const HomieEvent& event) {
  switch (event.type) {
    case HomieEventType::STANDALONE_MODE:
      Serial << "Standalone mode started" << endl;
      break;
    case HomieEventType::CONFIGURATION_MODE:
      Serial << "Configuration mode started" << endl;
      break;
    case HomieEventType::NORMAL_MODE:
      Serial << "Normal mode started" << endl;
      break;
    case HomieEventType::OTA_STARTED:
      Serial << "OTA started" << endl;
      break;
    case HomieEventType::OTA_PROGRESS:
      Serial << "OTA progress, " << event.sizeDone << "/" << event.sizeTotal << endl;
      break;
    case HomieEventType::OTA_FAILED:
      Serial << "OTA failed" << endl;
      break;
    case HomieEventType::OTA_SUCCESSFUL:
      Serial << "OTA successful" << endl;
      break;
    case HomieEventType::ABOUT_TO_RESET:
      Serial << "About to reset" << endl;
      break;
    case HomieEventType::WIFI_CONNECTED:
      Serial << "Wi-Fi connected, IP: " << event.ip << ", gateway: " << event.gateway << ", mask: " << event.mask << endl;
      break;
    case HomieEventType::WIFI_DISCONNECTED:
      Serial << "Wi-Fi disconnected, reason: " << (int8_t)event.wifiReason << endl;
      break;
    case HomieEventType::MQTT_PACKET_ACKNOWLEDGED:
      Serial << "MQTT packet acknowledged, packetId: " << event.packetId << endl;
      break;
    case HomieEventType::READY_TO_SLEEP:
      Serial << "Ready to sleep" << endl;
      break;
    case HomieEventType::MQTT_READY:
      Serial << "MQTT connected" << endl;
      break;
    case HomieEventType::MQTT_DISCONNECTED:
      Serial << "MQTT disconnected, reason: " << (int8_t)event.mqttReason << endl;
      break;
    case HomieEventType::SENDING_STATISTICS:
      Serial << "Sending statistics" << endl;
      publishSensorFunction();
      break;
    //default:
    //break;
  }
}
void publishSensorFunction() {
    float temp_f = dht.readTemperature(true);
    float humidity = dht.readHumidity();
    environmentNode.setProperty("temperature").send(String(temp_f));
    environmentNode.setProperty("humidity").send(String(humidity));  
}

bool doorOperatorFunction(const HomieRange& range, const String& value) {
  bool request = false;
  Serial.print("Button is now: ");
  Serial.println(value);
  if (value == "True" || value=="ON" || value=="true" || value == "on") {
    request = true;
  } else {
      // no work to do here as OpenHab2 is not in contol of turning door off
    return false;
  }
  digitalWrite(PIN_RELAY, request ? HIGH : LOW);
  openerNode.setProperty("button").send( request ? "true" : "false");
  Serial.println("Set Button property in doorOperatorFunction");
  openerEvent = millis();

  return true;
}
 void setupHandler() {
   dht.begin();
   debouncer.attach(PIN_SENSOR); // Attach to a pin for advanced users. Only attach the pin this way once you have previously set it up. Otherwise use attach(int pin, int mode). Int options are (INPUT, INPUT_PULLUP or OUTPUT)
   debouncer.interval(DEBOUNCER_MS); //Sets the debounce interval in milliseconds
}
void loopHandler() {
  debouncer.update(); //Updates the pin's state. Bounce does not use interrupts so must "update" the object before reading its value. Has to be done as often as possible (that means to include it in your loop()). Only call update() once per loop().

  sensorValue = debouncer.read();  // Returns the pin's state (HIGH or LOW)
 /*
  * signal open/closed based on reed sensor state*/
  if (sensorValue != lastSensorValue) { // The will track on and off via the not equals
    Serial.print("Door is now: ");
    Serial.println(sensorValue ? "open" : "close");
    openerNode.setProperty("door").send(sensorValue ? "Open" : "Close");
    lastSensorValue = sensorValue;     
  }

  /*
   * Turn off the relay after OPENER_EVENT_MS */
  if (openerEvent && (millis() - openerEvent >= OPENER_EVENT_MS)) {
    bool request = false;
    digitalWrite(PIN_RELAY, request ? HIGH : LOW);
    openerNode.setProperty("button").send(request ? "true" : "false"); // Tell Openhab2 that switch is back off
    Serial.println("Set Button property in loopHandler");
    openerEvent = 0;
  }
}

 void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_SENSOR, INPUT_PULLUP);

  digitalWrite(PIN_RELAY, LOW);  
 
  Homie_setFirmware(FW_NAME, FW_VERSION);

  openerNode.advertise("button").setName("Button")
                              .settable(doorOperatorFunction)     // expect "true" or "false"
                              .setDatatype("boolean");
                              
  openerNode.advertise("door").setName("Door")
                              .setDatatype("string")
                              .setFormat("%s");
                              
  environmentNode.advertise("temperature").setName("Temperature")
                              .setDatatype("float")
                              .setFormat("%.1f")
                              .setUnit("ºF");

  environmentNode.advertise("humidity").setName("Humidity")
                              .setDatatype("float")
                              .setFormat("%.1f")
                              .setUnit("%");
  
  Homie.setSetupFunction(setupHandler)
       .setLoopFunction(loopHandler) // Tell Homie which functions are available for it.
       .onEvent(onHomieEvent);  
  Homie.setup();
}
 void loop() {
  Homie.loop();
}
