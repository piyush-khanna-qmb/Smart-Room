/*

  PATCHE 6.3.1  [Remedial update]
    
    TIME:
    03 Jan, 2024 | 4:51PM
    
    UPDATE:
    . Evidentally, while triggering light bulb via finalIntrusion, we were changing offlineStates and this in turn toggled all currentOfflineStates.
    When this was implemented while intrusion systems were turned on online, and light was sutrned on when someone came in room, it triggerred implementOfflineStates which changed the sensor and lught back to off, or default offline switch value.
    . To get rid of this behavior, let's just change the digitalWrite(21) directly.
    
    STAGE: Tested. Worked Fine.

   ==========================================================================

  PATCH: 6.3.2  [Functional update]
    
    TIME:
    04 Jan, 2024 | 5:54PM
    
    UPDATE:
    . Attached external LED on pin 26 to show the status of HCSR sensors.
    > Normally stays off.
    > Single blink if baahr sensor corrupts
    > Double blink if andar sensor corrupts
    
    STAGE: Tested and Passed!
*/

#include <WiFi.h>
#include <NimBLEDevice.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-1234567890ab"

#define DEBUG true

String serverName = "https://smart-room-a4xe.onrender.com/";

int inputPins[7] = {4, 15, 13, 12, 14, 27, 33};
int outputPins[6] = {18, 19, 21, 22, 23, 25};
String names[7] = {"fan", "tubelight", "tabletop", "nightbulb", "laser", "bluelight", "sensor"};
bool lastOffline[7];
bool currOffline[7];
bool lastOnline[7];
bool currOnline[7];
bool finalIntrusionState = false;
bool shouldAlert = false;
int statusPin = 2, netPin = 32;
int sensorStatusLED = 26;
#define BAAHR_ECHO_PIN 34
#define BAAHR_TRIG_PIN 17
#define ANDAR_ECHO_PIN 35
#define ANDAR_TRIG_PIN 16

long duration;
float distance;
unsigned long lastIntruTime;
boolean netConnected = false;

Preferences preferences;
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

String receivedData = "";
bool deviceConnected = false;
bool bluetoothInitialized = false;
unsigned long lastConnectionAttempt = 0;
const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30 seconds
const unsigned long WIFI_ATTEMPT_TIMEOUT = 10000; // 10 seconds
int numOfPeople = 0;
bool mayComeIn = false, mayGoOut = false, decOne = false, incOne = false, lastLightState = false;

enum WifiState {
  CHECKING_STORED_CREDS,
  WAITING_FOR_BLE_DATA,
  CONNECTING_WIFI,
  WIFI_CONNECTED
};

enum SystemState {
  PureOffline,
  OnlyNet,
  PassiveIntrusion,
  ActiveIntrusion
};

WifiState currentState = CHECKING_STORED_CREDS;

SystemState systemState = PureOffline;

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
      //Serial.println("New device connected!");
      newDeviceConnectedPattern();
      deviceConnected = true;
    }

    void onDisconnect(NimBLEServer* pServer) override {
      //Serial.println("Device disconnected!");
      deviceConnected = false;
      // Only restart advertising if we're still in BLE mode
      if (currentState == WAITING_FOR_BLE_DATA) {
        NimBLEDevice::getAdvertising()->start();
        //Serial.println("Restarted BLE advertising...");
      }
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        //Serial.print("Data received: ");
        //Serial.println(value.c_str());
        receivedData = String(value.c_str());
      }
    }
};

void initializeBluetooth() {
  if (!bluetoothInitialized) {
    //Serial.println("Initializing Bluetooth for the first time...");
    delay(1000);

    // Try to release any previous BLE instances
    if (NimBLEDevice::getInitialized()) {
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
    //Serial.println("Starting BLE advertising...");
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
  }
}

void stopBluetoothAdvertising() {
  if (NimBLEDevice::getAdvertising()->isAdvertising()) {
    NimBLEDevice::getAdvertising()->stop();
    //Serial.println("Stopped BLE advertising.");
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
    netConnected = true;
    digitalWrite(statusPin, HIGH);
    return true;
  } else {
    //Serial.println("\nFailed to connect to Wi-Fi.");
    netConnected = false;
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

void showCorruptStatus(int times)
{
  for(int corrTimes= 0; corrTimes < times; corrTimes++)
  {
    digitalWrite(sensorStatusLED, HIGH);
    delay(200);
    digitalWrite(sensorStatusLED, LOW);
    delay(200);
  }
}

boolean isBaahrActive() {
  digitalWrite(BAAHR_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(BAAHR_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(BAAHR_TRIG_PIN, LOW);
  int duration = pulseIn(BAAHR_ECHO_PIN, HIGH);
// Serial.print("Baahr distance: ");
// Serial.println((duration * 0.034 / 2));
  if ((duration * 0.034 / 2) <= 5.0) {
   //Serial.println("\n\n\n==> Can't get value from Baahar HCSR \n\n\n");
   showCorruptStatus(1);
    return false;
  }
  else if ((duration * 0.034 / 2) <= 50.0)
    return true;
  return false;

}
boolean isAndarActive() {
  digitalWrite(ANDAR_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ANDAR_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ANDAR_TRIG_PIN, LOW);
  int duration = pulseIn(ANDAR_ECHO_PIN, HIGH);
// Serial.print("Andar distance: ");
// Serial.println((duration * 0.034 / 2));
//  delay(200);
  if ((duration * 0.034 / 2) <= 5.0) {
   //Serial.println("\n\n\n==> Can't get value from Baahar HCSR \n\n\n");
    showCorruptStatus(2);
    return false;
  }
  return ((duration * 0.034 / 2) <= 55.0);
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
// Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  for (int i = 0; i < 7; i++)
    pinMode(inputPins[i], INPUT_PULLUP);

  for (int i = 0; i < 6; i++)
    pinMode(outputPins[i], OUTPUT);

  pinMode(statusPin, OUTPUT);
  pinMode(netPin, INPUT_PULLUP);
  pinMode(sensorStatusLED, OUTPUT);
  netConnected = false;

  for (int i = 0; i < 7; i++)
  {
    lastOffline[i] = false;
    currOffline[i] = false;
    lastOnline[i] = false;
    currOnline[i] = false;
  }

  lastIntruTime = millis();
  pinMode(BAAHR_TRIG_PIN, OUTPUT);
  pinMode(BAAHR_ECHO_PIN, INPUT);
  pinMode(ANDAR_TRIG_PIN, OUTPUT);
  pinMode(ANDAR_ECHO_PIN, INPUT);

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

  if (digitalRead(netPin) == HIGH && finalIntrusionState == false)
  {
    systemState = PureOffline;
    // Remove net connection
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFi.disconnect(true);
      digitalWrite(statusPin, LOW);
      currentState = CHECKING_STORED_CREDS;
    }

    // Update offline values;
    bool offChange = updateOfflineStates();
    if (offChange == true)
    {
      implementOffline();
    }

    // Do not check HCSR

    // Set total noOfPeople 0
    if (numOfPeople != 0)
      numOfPeople = 0;
  }

  else if (digitalRead(netPin) == LOW && finalIntrusionState == false)
  {
    systemState = OnlyNet;
    if (WiFi.status() != WL_CONNECTED) // Initially when net is not connected and button is just pressed
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

    // Update Pins both online and offline
    String stString = getStatusString();
    if (stString.equals("Dikkat"))
    {
      //Serial.println(stString);
      //Serial.println("Dikkat clause");
    }
    else
    {
      bool onChange = updateOnlineStates(stString);
      if (onChange == true)
      {
       //Serial.println("Values changed from server side");
        implementOnline();
      }
      bool offChange = updateOfflineStates();
      if (offChange == true)
      {
       //Serial.println("Values changed manually");
        implementOffline();
       //Serial.println("Sending to server for updation");
        triggerDevices();
      }
    }

    // Do not check HCSR

    // Set total noOfPeople 0
    if (numOfPeople != 0)
      numOfPeople = 0;
  }

  else if (digitalRead(netPin) == HIGH && finalIntrusionState == true)
  {
    systemState = PassiveIntrusion;
    // Remove net connection
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFi.disconnect(true);
      digitalWrite(statusPin, LOW);
      currentState = CHECKING_STORED_CREDS;
    }

    // Do not send Intrusion Alert
    shouldAlert = false;

    // Check HCSR
    hcsrFunctions();

    // Update offline values;
    bool offChange = updateOfflineStates();
    if (offChange == true)
    {
      implementOffline();
    }


    if (!lastLightState && numOfPeople > 0) {
      lastLightState = true;
      if (shouldAlert)
        sendIntrusionAlert();
    }
    if (numOfPeople <= 0 && lastLightState) {
      lastLightState = false;
    }
  }

  else if (digitalRead(netPin) == LOW && finalIntrusionState == true)
  {
    systemState = ActiveIntrusion;
    // Stay Connected to internet
    if (WiFi.status() != WL_CONNECTED) // Initially when net is not connected and button is just pressed
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

    
    // Check Intrusion Alert
    shouldAlert = true;

    // Check HCSR
    hcsrFunctions();

    // Update only offline values
    bool offChange = updateOfflineStates();
    if (offChange == true)
    {
      implementOffline();
    }

    if (!lastLightState && numOfPeople > 0) {
      lastLightState = true;
      if (shouldAlert)
        sendIntrusionAlert();
    }
    if (numOfPeople <= 0 && lastLightState) {
      lastLightState = false;
    }
  }
}

bool andarStatus, baahrStatus;

void hcsrFunctions()
{
  baahrStatus = isBaahrActive();
  andarStatus = isAndarActive();
  if (baahrStatus)
  {
    if (mayGoOut) {
      decOne = true;
      mayGoOut = false;
    }
    else {
      if (!decOne && !mayComeIn) {
        mayComeIn = true;
      }
    }
  }
  if (andarStatus)
  {
    if (mayComeIn) {
      incOne = true;
      mayComeIn = false;
    }
    else {
      if (!incOne && !mayGoOut) {
        mayGoOut = true;
      }
    }
  }

  if (!andarStatus && !baahrStatus)
  {
    if (incOne) {
      numOfPeople++;
      incOne = false;
    }
    if (decOne) {
      if (numOfPeople > 0)
        numOfPeople--;
      decOne = false;
    }

  }

  // Todo: COmment Below
 //Serial.print("\n\nFinal Intrusion is now: ");
 //Serial.println(finalIntrusionState);

//Serial.print("Num of people in room= ");
//Serial.println(numOfPeople);

//  delay(500);
}

String getStatusString()
{
  HTTPClient http;

  String sPath = serverName + "get-all-devices-status/";
  http.begin(sPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  // Check for a successful response
  if (httpResponseCode > 0) {
    payload = http.getString();
  } else {
    payload = "Dikkat";
  }

  http.end();  // Close connection
  return payload;
}

void implementOffline()
{
  for (int i = 0; i < 6; i++)
  {
    digitalWrite(outputPins[i], currOffline[i]);
  }

  finalIntrusionState = currOffline[6];
  if (WiFi.status() == WL_CONNECTED)
  {
      triggerDevice("sensor", finalIntrusionState);
      currOnline[6] = finalIntrusionState;
  }
}

void implementOnline()
{
  for (int i = 0; i < 6; i++)
  {
    digitalWrite(outputPins[i], currOnline[i]);
  }
  finalIntrusionState = currOnline[6];
  if (WiFi.status() == WL_CONNECTED)
  {
    triggerDevice("sensor", finalIntrusionState);
    currOnline[6] = finalIntrusionState;
  }
}

bool updateOnlineStates(String st)
{
  bool changed = false;
  for (int i = 0; i < 7; i++)
    lastOnline[i] = currOnline[i];

  for (int i = 0; i < 7; i++)
  {
    char c = st.charAt(i);
    if (c == '0')
    {
      currOnline[i] = false;
      if (lastOnline[i] != false)
        changed = true;
    }
    else
    {
      currOnline[i] = true;
      if (lastOnline[i] != true)
        changed = true;
    }
  }

  return changed;
}

bool updateOfflineStates()
{
  bool changed = false;
  for (int i = 0; i < 7; i++)
    lastOffline[i] = currOffline[i];

  // Uncomment to get finalIntrus state
//  if(finalIntrusionState)
//   Serial.println("Controlled by finalIntru");
//  else
//   Serial.println("Controlled by light switch");
  
  for (int i = 0; i < 7; i++)
  {
    if (i == 2 && finalIntrusionState)
    {
     //Serial.println("Control in hands of hcsr");
      if (numOfPeople > 0) {
        digitalWrite(21, true);
      }
      else {
        digitalWrite(21, false);
      }
    }
    else if (digitalRead(inputPins[i]) == HIGH)
    {
     //Serial.println("Control in hands of switch");
      currOffline[i] = false;
      if (lastOffline[i] != false)
        changed = true;
    }
    else
    {
     //Serial.println("Control in hands of switch");
      currOffline[i] = true;
      if (lastOffline[i] != true)
        changed = true;
    }
  }
  return changed;
}

//void makeInitialRequest() {
//  HTTPClient http;
//  String sPath = serverName + "sync-device-states/";
//
//  // Create state string from current device states
//  String stateString = "";
//  for(int i = 0; i < 6; i++) {
//    stateString += lastOnline[i] ? "1" : "0";
//  }
//
//  // Create JSON document
//  StaticJsonDocument<200> jsonDoc;
//  jsonDoc["states"] = stateString;
//
//  //Serialize JSON
//  String jsonString;
//  serializeJson(jsonDoc, jsonString);
//
//  // Make PUT request
//  http.begin(sPath);
//  http.addHeader("Content-Type", "application/json");
//  int httpResponseCode = http.PUT(jsonString);
//
//  if (httpResponseCode > 0) {
//    String payload = http.getString();
//  }
//
//  http.end();  // Close connection
//}

void triggerDevices()
{
  for (int i = 0; i < 7; i++)
  {
    if (lastOffline[i] != currOffline[i])
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

  //Serialize JSON to string
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
    ////Serial.print("HTTP Response code: ");
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
    //Serial.print("HTTP Response Code: ");
    //Serial.println(httpResponseCode);
    String payload = http.getString();
    //Serial.println("Response payload: " + payload);
  } else {
    //Serial.print("HTTP Request failed with error code: ");
    //Serial.println(httpResponseCode);
    //Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}
