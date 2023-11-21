/*
v0.3

Added functionality to switch between various ssids to connect tp networks in case 1 fails.
Also, got the server hosted for real.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String ssids[4]= {"TECNO SPARK Go 2020", "Maachis", "Oppo Hotpot", "KIETROBO"};
String passes[4]= {"12345678", "JalaaloJi", "Piyush12@", "Kiet123456"};
int wifiNum= 0;
String serverName = "https://smart-room-a4xe.onrender.com/";

int inputPins[7]= {4, 15, 13, 12, 14, 27, 33};
int outputPins[7]= {18, 19, 21, 22, 23, 25, 26};
String names[7]= {"fan", "tubelight", "tabletop", "nightbulb", "laser", "bluelight", "extension"};
bool lastOffline[7];
bool currOffline[7];
bool lastOnline[7];
bool currOnline[7];
int workMode= 1;
int statusPin= 2, netPin= 32;

void connectToWiFi() 
{
  Serial.println("Attempting to connect to WiFi...");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi!");
    workMode = 1;
    return;
  }

  // Try to connect for up to 5 seconds
  WiFi.begin(ssids[wifiNum], passes[wifiNum]);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) {
    digitalWrite(statusPin, HIGH);
    delay(500);
    digitalWrite(statusPin, LOW);
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {  // Just newly connected
    Serial.println("\nWiFi connected successfully!");
    workMode = 1;
    digitalWrite(statusPin, HIGH);
    makeInitialRequest();
  } else {
    Serial.println("\nFailed to connect to WiFi within 5 seconds.");
    workMode = 0;
    WiFi.disconnect(true);
    digitalWrite(statusPin, LOW);
  }
  wifiNum= (wifiNum+1)%4;
}

void setup()
{
  Serial.begin(115200);
  for(int i= 0; i<7; i++)
    pinMode(inputPins[i], INPUT_PULLUP);

  for(int i= 0; i<7; i++)
    pinMode(outputPins[i], OUTPUT);

  pinMode(statusPin, OUTPUT);
  pinMode(netPin, INPUT_PULLUP);

  for(int i= 0; i<7; i++)
  {
    lastOffline[i]= false;
    currOffline[i]= false;
    lastOnline[i]= false;
    currOnline[i]= false;
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
    connectToWiFi();
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
      Serial.println(stString);
      Serial.println("Dikkat clause");
    }
    else
    {
      bool onChange= updateOnlineStates(stString);
      if(onChange == true)
      {
        Serial.println("Values changed from server side");
        implementOnline();
      }
      bool offChange= updateOfflineStates();
      if(offChange == true)
      {
        Serial.println("Values changed manually");
        implementOffline();
        Serial.println("Sending to server for updation");
        triggerDevices();
      }
    }
  }
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
  for(int i= 0; i<7; i++)
  {
    digitalWrite(outputPins[i], currOffline[i]);
  }
}

void implementOnline()
{
  for(int i= 0; i<7; i++)
  {
    digitalWrite(outputPins[i], currOnline[i]);
  }
}

bool updateOnlineStates(String st)
{
  bool changed= false;
  for(int i= 0; i<7; i++)
    lastOnline[i]= currOnline[i];
    
  for(int i= 0; i<7; i++)
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
  for(int i= 0; i<7; i++)
    lastOffline[i]= currOffline[i];

  for(int i= 0; i<7; i++)
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
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Response payload: " + payload);
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();  // Close connection
}

void triggerDevices()
{
  for(int i= 0; i<7; i++)
  {
    if(lastOffline[i] != currOffline[i])
    {
      triggerDevice(names[i], currOffline[i]);
    }
  }
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
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Response payload: " + payload);
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}