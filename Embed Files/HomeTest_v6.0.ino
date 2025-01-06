/*
 * First Commercial Product with updatable settings, 8 switches and dual sensors.
 * 
 * [DONE] 1. Get data from bluetooth using LW, Low space lib
 * [DONE] 2. Try to connect to wifi using got creds from bluetooth
 * [DONE] 3. Try to save creds in preferences
 * [DONE] 4. Before starting the bluetooth, try to connect to stored creds. If not then go to step 1
 * [DONE] 5. If disconnected, try to connect again using same procedure
 * [DONE] 6. Integrate BT with Dual sensor code
 * [DONE] 7. Add StatusPin led blink gestures
 * 
 * [PERFECTLY WORKING!!]
*/
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h> 

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-1234567890ab"

String serverName = "https://smart-room-a4xe.onrender.com/";

int inputPins[6]= {4, 15, 13, 12, 14, 27};
int outputPins[6]= {18, 19, 21, 22, 23, 25};
String names[6]= {"fan", "tubelight", "tabletop", "nightbulb", "laser", "bluelight"};
bool lastOffline[6];
bool currOffline[6];
bool lastOnline[6];
bool currOnline[6];
int workMode= 1;
int intrusionPin= 33, statusPin= 2, netPin= 32;
#define BAAHR_ECHO_PIN 34
#define BAAHR_TRIG_PIN 17
#define ANDAR_ECHO_PIN 35
#define ANDAR_TRIG_PIN 16

long duration;
float distance;
unsigned long lastIntruTime;
boolean tempOn;
boolean ext;
boolean tuptup;

Preferences preferences;
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

String receivedData = "";
bool deviceConnected = false;
bool bluetoothInitialized = false;
unsigned long lastConnectionAttempt = 0;
const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30 seconds
const unsigned long WIFI_ATTEMPT_TIMEOUT = 10000; // 10 seconds
int numOfPeople= 0;
bool mayComeIn= false, mayGoOut= false, decOne= false, incOne= false, lastLightState= false;

enum SystemState {
  CHECKING_STORED_CREDS,
  WAITING_FOR_BLE_DATA,
  CONNECTING_WIFI,
  WIFI_CONNECTED
};

SystemState currentState = CHECKING_STORED_CREDS;


void newDeviceConnectedPattern()
{
  delay(200);
  digitalWrite(statusPin, LOW);
  delay(200);
  digitalWrite(statusPin, HIGH);
  delay(200);
  digitalWrite(statusPin, LOW);
  delay(200);
  digitalWrite(statusPin, HIGH);
  delay(200);
  digitalWrite(statusPin, LOW);
  delay(200);
  digitalWrite(statusPin, HIGH);
  delay(200);
  digitalWrite(statusPin, LOW);
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
//    Serial.println("New device connected!");
    newDeviceConnectedPattern();
    deviceConnected = true;
  }

  void onDisconnect(NimBLEServer* pServer) override {
//    Serial.println("Device disconnected!");
    deviceConnected = false;
    // Only restart advertising if we're still in BLE mode
    if (currentState == WAITING_FOR_BLE_DATA) {
      NimBLEDevice::getAdvertising()->start();
//      Serial.println("Restarted BLE advertising...");
    }
  }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
//      Serial.print("Data received: ");
//      Serial.println(value.c_str());
      receivedData = String(value.c_str());
    }
  }
};

void initializeBluetooth() {
  if (!bluetoothInitialized) {
//    Serial.println("Initializing Bluetooth for the first time...");
    delay(1000);
    
    // Try to release any previous BLE instances
    if(NimBLEDevice::getInitialized()) {
      NimBLEDevice::deinit(true);
    }
    
    NimBLEDevice::init("ESP32_NimBLE");
     delay(100);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
  
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );
    
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pService->start();
    bluetoothInitialized = true;
  }
}

void startBluetoothAdvertising() {
  if (!NimBLEDevice::getAdvertising()->isAdvertising()) {
//    Serial.println("Starting BLE advertising...");
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
  }
}

void stopBluetoothAdvertising() {
  if (NimBLEDevice::getAdvertising()->isAdvertising()) {
    NimBLEDevice::getAdvertising()->stop();
//    Serial.println("Stopped BLE advertising.");
  }
}


bool connectToWiFi(const String& ssid, const String& pass) {
  if (ssid.isEmpty() || pass.isEmpty()) {
    return false;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < WIFI_ATTEMPT_TIMEOUT) {
    delay(500);
    makeStoredCredsPattern();
  }

  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("\nConnected to Wi-Fi: " + ssid);
    //Serial.println("IP Address: " + WiFi.localIP().toString());
    workMode = 1;
    makeInitialRequest();
    digitalWrite(statusPin, HIGH);
    return true;
  } else {
    //Serial.println("\nFailed to connect to Wi-Fi.");
    workMode = 0;
    WiFi.disconnect(true);
    digitalWrite(statusPin, LOW);
    return false;
  }
}

void saveWiFiCredentials(const String& ssid, const String& pass) {
  preferences.begin("wifi_creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", pass);
  preferences.end();
  //Serial.println("Wi-Fi credentials saved to flash.");
}

bool loadAndConnectWiFi() {
  preferences.begin("wifi_creds", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();
  
  if (ssid.length() > 0 && password.length() > 0) {
    //Serial.println("Found stored credentials. Attempting to connect...");
    return connectToWiFi(ssid, password);
  }
  return false;
}

bool processReceivedData() {
  if (receivedData.startsWith("WIFI:")) {
    int firstColon = receivedData.indexOf(':');
    int secondColon = receivedData.indexOf(':', firstColon + 1);
    if (firstColon != -1 && secondColon != -1) {
      String ssid = receivedData.substring(firstColon + 1, secondColon);
      String password = receivedData.substring(secondColon + 1);
      
      if (connectToWiFi(ssid, password)) {
        saveWiFiCredentials(ssid, password);
        return true;
      }
    } else {
      //Serial.println("Invalid format. Expected format: WIFI:<ssid>:<pass>");
    }
  } else {
    //Serial.println("Invalid data format. Expected: WIFI:<ssid>:<pass>");
  }
  return false;
}


boolean isBaahrActive() {
    digitalWrite(BAAHR_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(BAAHR_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(BAAHR_TRIG_PIN, LOW);
    int duration = pulseIn(BAAHR_ECHO_PIN, HIGH);
//    Serial.print("Baahr distance: ");
//    Serial.println((duration * 0.034 / 2));
    return ((duration * 0.034 / 2) <= 50.0);
}
boolean isAndarActive() {
    digitalWrite(ANDAR_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ANDAR_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ANDAR_TRIG_PIN, LOW);
    int duration = pulseIn(ANDAR_ECHO_PIN, HIGH);
//    Serial.print("Andar distance: ");
//    Serial.println((duration * 0.034 / 2));
    return ((duration * 0.034 / 2) <= 50.0);
}

void makeStoredCredsPattern()
{
  digitalWrite(statusPin, HIGH);
  delay(200);
  digitalWrite(statusPin, LOW);
  delay(200);
  digitalWrite(statusPin, HIGH);
  delay(200);
  digitalWrite(statusPin, LOW);
  delay(800);
}

void makeWaitingBLEPattern()
{
  digitalWrite(statusPin, HIGH);
  delay(1000);
  digitalWrite(statusPin, LOW);
  delay(1000);
}


void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  for(int i= 0; i<6; i++)
    pinMode(inputPins[i], INPUT_PULLUP);

  for(int i= 0; i<6; i++)
    pinMode(outputPins[i], OUTPUT);

  pinMode(statusPin, OUTPUT);
  pinMode(netPin, INPUT_PULLUP);

  for(int i= 0; i<6; i++)
  {
    lastOffline[i]= false;
    currOffline[i]= false;
    lastOnline[i]= false;
    currOnline[i]= false;
  }

  ext= false;
  tempOn= false;
  lastIntruTime= millis();
  pinMode(BAAHR_TRIG_PIN, OUTPUT);
  pinMode(BAAHR_ECHO_PIN, INPUT);
  pinMode(ANDAR_TRIG_PIN, OUTPUT);
  pinMode(ANDAR_ECHO_PIN, INPUT);
  pinMode(intrusionPin, INPUT_PULLUP);

  delay(1000);
  
  //Serial.println("Initializing Bluetooth...");
  initializeBluetooth();
  
  if (bluetoothInitialized) {
    //Serial.println("Bluetooth initialization successful");
    currentState = CHECKING_STORED_CREDS;
  } else {
    //Serial.println("Bluetooth initialization failed - retrying in loop");
  }
}

void loop()
{
  if(digitalRead(netPin) == HIGH)
  {
    workMode= 0;
    digitalWrite(statusPin, LOW);
  }
  else
  {
    switch (currentState) {
    case CHECKING_STORED_CREDS:
      if (loadAndConnectWiFi()) {
        stopBluetoothAdvertising();
        currentState = WIFI_CONNECTED;
      } else {
        startBluetoothAdvertising();
        currentState = WAITING_FOR_BLE_DATA;
      }
      break;

    case WAITING_FOR_BLE_DATA:
      makeWaitingBLEPattern();
      if (!receivedData.isEmpty()) {
        if (processReceivedData()) {
          stopBluetoothAdvertising();
          currentState = WIFI_CONNECTED;
        }
        receivedData = "";
      }
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        unsigned long currentTime = millis();
        if (currentTime - lastConnectionAttempt > CONNECTION_RETRY_INTERVAL) {
          //Serial.println("WiFi connection lost. Attempting to reconnect...");
          digitalWrite(statusPin, LOW);
          lastConnectionAttempt = currentTime;
          currentState = CHECKING_STORED_CREDS;
        }
      }
      break;
  }
  }

  //Serial.print("Workmode: ");
  //Serial.println(workMode);
  hcsrFunctions();
  
  if (workMode == 0) 
  {
    bool offChange= updateOfflineStates();
    if(offChange == true)
    {
      implementOffline();
    }
  }
  else
  {
    String stString= getStatusString();
    if(stString.equals("Dikkat"))
    {
      workMode= 0;
//      Serial.println(stString);
//      Serial.println("Dikkat clause");
    }
    else
    {
      bool onChange= updateOnlineStates(stString);
      if(onChange == true)
      {
//        Serial.println("Values changed from server side");
        implementOnline();
      }
      bool offChange= updateOfflineStates();
      if(offChange == true)
      {
//        Serial.println("Values changed manually");
        implementOffline();
//        Serial.println("Sending to server for updation");
        triggerDevices();
      }
    }
  }
}

void hcsrFunctions()
{
  if(digitalRead(intrusionPin)==LOW)
  {
    if(isBaahrActive())
    {
        if(mayGoOut) {
            decOne= true;
            mayGoOut= false;
        }
        else {
            if(!decOne && !mayComeIn) {
                mayComeIn= true;
            }
        }
    }
    if(isAndarActive())
    {
        if(mayComeIn) {
            incOne= true;
            mayComeIn= false;
        }
        else {
            if(!incOne && !mayGoOut) {
                mayGoOut= true;
            }
        }
    }

    if(!isAndarActive() && !isBaahrActive())
    {
        if(incOne) {
        numOfPeople++;
        incOne= false;
        }
        if(decOne) {
            if(numOfPeople > 0)
              numOfPeople--;
            decOne= false;
        }

        if(!lastLightState && numOfPeople>0) {
            digitalWrite(21, HIGH);
            lastLightState= true;
        }
        if(numOfPeople <= 0 && lastLightState) {
            digitalWrite(21, LOW);
            lastLightState= false;
        } 

    }
    
    Serial.print("Num of people in room= ");
    Serial.println(numOfPeople);
  }
  else if(digitalRead(intrusionPin)==HIGH && numOfPeople!=0)
    numOfPeople= 0;
}

String getStatusString()
{
  HTTPClient http;

  String sPath = serverName+"get-all-devices-status/";
  http.begin(sPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  // Check for a successful response
  if (httpResponseCode > 0) {
     payload = http.getString();
  } else {
    payload= "Dikkat";
  }

  http.end();  // Close connection
  return payload;
}

void implementOffline()
{
  for(int i= 0; i<6; i++)
  {
    digitalWrite(outputPins[i], currOffline[i]);
  }
}

void implementOnline()
{
  for(int i= 0; i<6; i++)
  {
    digitalWrite(outputPins[i], currOnline[i]);
  }
}

bool updateOnlineStates(String st)
{
  bool changed= false;
  for(int i= 0; i<6; i++)
    lastOnline[i]= currOnline[i];
    
  for(int i= 0; i<6; i++)
  {
    char c= st.charAt(i);
    if(c == '0')
    {
      currOnline[i]= false;
      if(lastOnline[i] != false)
        changed= true;
    }
    else
    {
      currOnline[i]= true;
      if(lastOnline[i] != true)
        changed= true;
    }
  }
  return changed;
}

bool updateOfflineStates()
{
  bool changed= false;
  for(int i= 0; i<6; i++)
    lastOffline[i]= currOffline[i];

  for(int i= 0; i<6; i++)
  {
    if(digitalRead(inputPins[i]) == HIGH)
    {
      currOffline[i]= false;
      if(lastOffline[i] != false)
        changed= true;
    }
    else
    {
      currOffline[i]= true;
      if(lastOffline[i] != true)
        changed= true;
    }
  }
  return changed;
}

void makeInitialRequest()
{
  HTTPClient http;

  String sPath = serverName+"entire-shutdown/";
  http.begin(sPath.c_str());
  int httpResponseCode = http.GET();

  // Check for a successful response
  if (httpResponseCode > 0) {
//    Serial.print("HTTP Response Code: ");
//    Serial.println(httpResponseCode);
    String payload = http.getString();
//    Serial.println("Response payload: " + payload);
  } else {
//    Serial.print("HTTP Request failed with error code: ");
//    Serial.println(httpResponseCode);
  }

  http.end();  // Close connection
}

void triggerDevices()
{
  for(int i= 0; i<6; i++)
  {
    if(lastOffline[i] != currOffline[i])
    {
      triggerDevice(names[i], currOffline[i]);
    }
  }
}

void sendIntrusionAlert()
{
  String url = serverName + "intrusion-alert/";
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["source"] = "awesome-field";

  // Serialize JSON to string
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Create HTTPClient object
  HTTPClient http;

  // Specify content type and destination URL
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Send POST request with JSON data
  int httpResponseCode = http.POST(jsonString);

  // Check for a successful request
  if (httpResponseCode > 0) {
    //Serial.print("HTTP Response code: ");
    //Serial.println(httpResponseCode);
    //Serial.println("POST request successful!");
  } else {
    //Serial.print("HTTP Error code: ");
    //Serial.println(httpResponseCode);
    //Serial.println("POST request failed");
  }

  // Free resources
  http.end();
}

void triggerDevice(String stringValue, bool booleanValue) {

  HTTPClient http;
  String url = serverName + "trigger-device/";

  DynamicJsonDocument jsonDoc(200); // Adjust the size based on your JSON data
  jsonDoc["name"] = stringValue;
  jsonDoc["state"] = booleanValue;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.PUT(jsonString); // Use PUT instead of POST

  if (httpResponseCode > 0) {
//    Serial.print("HTTP Response Code: ");
//    Serial.println(httpResponseCode);
    String payload = http.getString();
//    Serial.println("Response payload: " + payload);
  } else {
//    Serial.print("HTTP Request failed with error code: ");
//    Serial.println(httpResponseCode);
//    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}
