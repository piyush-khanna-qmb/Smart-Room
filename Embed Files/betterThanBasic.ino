#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "TECNO SPARK Go 2020";
const char *password = "12345678";
String serverName = "https://long-yaks-invite.loca.lt/";

int inputPins[8]= {4, 15, 13, 12, 14, 27, 33, 32};
int outputPins[8]= {18, 19, 21, 22, 23, 25, 26, 17};
String names[8]= {"fan", "tubelight", "tabletop", "nightbulb", "laser", "bluelight", "extension", "test"};
bool lastOffline[8];
bool currOffline[8];
bool lastOnline[8];
bool currOnline[8];
int workMode= 1; 

void connectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");

  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi!");
    workMode = 1;
    return;
  }

  // Try to connect for up to 5 seconds
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    workMode = 1;
  } else {
    Serial.println("\nFailed to connect to WiFi within 5 seconds.");
    workMode = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  for(int i= 0; i<8; i++)
    pinMode(inputPins[i], INPUT_PULLUP);

  for(int i= 0; i<8; i++)
    pinMode(outputPins[i], OUTPUT);

  WiFi.begin(ssid, password);
  connectToWiFi();

  for(int i= 0; i<8; i++)
  {
    lastOffline[i]= false;
    currOffline[i]= false;
    lastOnline[i]= false;
    currOnline[i]= false;
  }
  makeInitialRequest();
}

void loop()
{
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
  for(int i= 0; i<8; i++)
  {
    digitalWrite(outputPins[i], currOffline[i]);
  }
}

void implementOnline()
{
  for(int i= 0; i<8; i++)
  {
    digitalWrite(outputPins[i], currOnline[i]);
  }
}

bool updateOnlineStates(String st)
{
  bool changed= false;
  for(int i= 0; i<8; i++)
    lastOnline[i]= currOnline[i];
    
  for(int i= 0; i<8; i++)
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
  for(int i= 0; i<8; i++)
    lastOffline[i]= currOffline[i];

  for(int i= 0; i<8; i++)
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
  for(int i= 0; i<8; i++)
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
