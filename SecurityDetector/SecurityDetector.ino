#include <SPI.h>
#include <WiFi.h>
#include <WifiIPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

// your network name also called SSID
char ssid[] = "HometelecomPZEX";
// your network password
char password[] = "6A7AE37421";

// IBM IoT Foundation Cloud Settings
#define MQTT_MAX_PACKET_SIZE 100
#define IBMSERVERURLLEN  64
#define IBMIOTFSERVERSUFFIX "messaging.internetofthings.ibmcloud.com"
char organization[] = "r66ltq";
char typeId[] = "CC3200";
char pubtopic[] = "iot-2/evt/status/fmt/json";
char subTopic[] = "iot-2/cmd/+/fmt/json";
char deviceId[] = "508cb1770ca5";
char clientId[64];

// Authentication method. Should be use-toke-auth
// When using authenticated mode
char authMethod[] = "use-token-auth";
// The auth-token from the information above
char authToken[] = "HwDESBJZ1h?tb?RNOd";

char mqttAddr[IBMSERVERURLLEN];
int mqttPort = 1883;

MACAddress mac;

// getTemp() function for cc3200
#ifdef TARGET_IS_CC3101
#include <Wire.h>
#include "Adafruit_TMP006.h"
Adafruit_TMP006 tmp006(0x41);
#endif
  
WifiIPStack ipstack;  
MQTT::Client<WifiIPStack, Countdown, MQTT_MAX_PACKET_SIZE> client(ipstack);

#define DEBOUNCE_TIME 150
const byte interruptPin = 3;
volatile byte state = LOW;
volatile unsigned long last_time = 0;   // time for first RISING edge 

int ledPin = RED_LED;

void setup() {
  uint8_t macOctets[6];
  
  Serial.begin(115200);
  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to Network named: ");
  // print the network name (SSID);
  Serial.println(ssid); 
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED) {
    // print dots while we wait to connect
    Serial.print(".");
    delay(300);
  
  Serial.println("\nYou're connected to the network");
  Serial.println("Waiting for an ip address");
  
  while (WiFi.localIP() == INADDR_NONE) {
    // print dots while we wait for an ip addresss
    Serial.print(".");
    delay(300);
  }

  // We are connected and have an IP address.
  Serial.print("\nIP Address obtained: ");
  Serial.println(WiFi.localIP());

  mac = WiFi.macAddress(macOctets);
  Serial.print("MAC Address: ");
  Serial.println(mac);
  
  // Use MAC Address as deviceId
  sprintf(deviceId, "%02x%02x%02x%02x%02x%02x", macOctets[0], macOctets[1], macOctets[2], macOctets[3], macOctets[4], macOctets[5]);
  Serial.print("deviceId: ");
  Serial.println(deviceId);

  sprintf(clientId, "d:%s:%s:%s", organization, typeId, deviceId);
  sprintf(mqttAddr, "%s.%s", organization, IBMIOTFSERVERSUFFIX);

  Serial.println("IBM IoT Foundation QuickStart example, view data in cloud here");
  Serial.print("--> http://quickstart.internetofthings.ibmcloud.com/#/device/");
  Serial.println(deviceId);

  #ifdef TARGET_IS_CC3101
  if (!tmp006.begin()) {
    Serial.println("No sensor found");
    while (1);
  }
  #endif

  // Reed Switch Interrupt
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(interruptPin, doorOpen, CHANGE );
  }
}

void doorOpen() {
  if(millis() - last_time > DEBOUNCE_TIME) {
      last_time = millis();
      
      int rc = -1;
    
      char* json = "{\"DoorStatus\":0}\0";

      if(digitalRead(interruptPin) == HIGH)
      {
        //If detected movement
        if(PIRStatus == 1) {
          json = "{\"DoorStatus\":1, \"PIRStatus\":1}\0";
        }else {
          json = "{\"DoorStatus\":1, \"PIRStatus\":0}\0";
        }
      }
    
//      double temp = getTemp();
      
//      dtostrf(temp,1,2, &json[43]);
//      json[48] = '}';
//      json[49] = '}';
//      json[50] = '\0';
      Serial.print("Publishing: ");
      Serial.println(json);
      MQTT::Message message;
      message.qos = MQTT::QOS0; 
      message.retained = false;
      message.payload = json; 
      message.payloadlen = strlen(json);
      rc = client.publish(pubtopic, message);
      if (rc != 0) {
        Serial.print("Message publish failed with return code : ");
        Serial.println(rc);
      }
      
  } 
}

void loop() {

  int rc = -1;
  if (!client.isConnected()) {
    Serial.print("Connecting to ");
    Serial.print(mqttAddr);
    Serial.print(":");
    Serial.println(mqttPort);
    Serial.print("With client id: ");
    Serial.println(clientId);
    
    while (rc != 0) {
      rc = ipstack.connect(mqttAddr, mqttPort);
    }

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    connectData.MQTTVersion = 3;
    connectData.clientID.cstring = clientId;
    connectData.username.cstring = authMethod;
    connectData.password.cstring = authToken;
    connectData.keepAliveInterval = 10;
    
    rc = -1;
    while ((rc = client.connect(connectData)) != 0)
      ;
    Serial.println("Connected\n");
    
    Serial.print("Subscribing to topic: ");
    Serial.println(subTopic);
    
    // Unsubscribe the topic, if it had subscribed it before.
    client.unsubscribe(subTopic);
    // Try to subscribe for commands
    if ((rc = client.subscribe(subTopic, MQTT::QOS0, messageArrived)) != 0) {
      Serial.print("Subscribe failed with return code : ");
      Serial.println(rc);
    } else {
      Serial.println("Subscribe success\n");
    }
  }


  // Wait for one second before publishing again
  client.yield(10000);
}


void messageArrived(MQTT::MessageData& md) {
  Serial.print("Message Received\t");
    MQTT::Message &message = md.message;
    int topicLen = strlen(md.topicName.lenstring.data) + 1;
//    char* topic = new char[topicLen];
    char * topic = (char *)malloc(topicLen * sizeof(char));
    topic = md.topicName.lenstring.data;
    topic[topicLen] = '\0';
    
    int payloadLen = message.payloadlen + 1;
//    char* payload = new char[payloadLen];
    char * payload = (char*)message.payload;
    payload[payloadLen] = '\0';
    
    String topicStr = topic;
    String payloadStr = payload;
    
    //Command topic: iot-2/cmd/blink/fmt/json

    if(strstr(topic, "/cmd/blink") != NULL) {
      Serial.print("Command IS Supported : ");
      Serial.print(payload);
      Serial.println("\t.....");
      
      pinMode(ledPin, OUTPUT);
      
      //Blink twice
      for(int i = 0 ; i < 2 ; i++ ) {
        digitalWrite(ledPin, HIGH);
        delay(250);
        digitalWrite(ledPin, LOW);
        delay(250);
      }
    } else {
      Serial.println("Command Not Supported:");            
    }
}

double getTemp() {
  return (double)tmp006.readObjTempC();
}
