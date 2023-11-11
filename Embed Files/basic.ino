// Corresponds to single manual switch registry and sending same info to database

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "Oppo Hotpot";
const char *password = "Piyush12@";
String serverName = "https://pink-poems-yawn.loca.lt/";

int pin = 12;
int state = 0, lastState = -1;

void setup()
{
  Serial.begin(115200);
  pinMode(pin, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  delay(2000);
  makeRequest();
  delay(2000);
}

void loop()
{
  if (digitalRead(pin) == HIGH)
    state = 0;
  else
    state = 1;

  if (state != lastState)
  {
    if (state == 0)
    {
      Serial.println("Turning Light OFF \U0001F311");
      sendPushRequest(0);
    }
    else
    {
      Serial.println("Turning Light ON \u2600");
      sendPushRequest(1);
    }
    delay(200);
  }
  lastState = state;

}

void makeRequest()
{
  HTTPClient http;

  String sPath = serverName;
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

void sendPushRequest(int vall)
{
  DynamicJsonDocument jsonDoc(200); // Adjust the size based on your JSON data
  jsonDoc["value"] = vall;

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  
  HTTPClient http;
  String url = serverName + "valueSent/";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(jsonString);

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