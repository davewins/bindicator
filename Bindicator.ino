#include <ArduinoJson.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include "ArduinoOTA.h"    //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
#include <DateTime.h>
#include <WiFiClientSecure.h>

unsigned long lastMs = 0;
unsigned long ms = millis();
//firstRun is used so that when you boot up the ESP - it checks calls the API straight away. Otherwise it will only check once per hour
int firstRun = 1;

//for LED status
// On a NodeMCU board the built-in led is on GPIO pin 2 (D4)
#define BUILTIN_LED D4
#define BLUE_LED 0
#define GREEN_LED 4

#include <Ticker.h>
Ticker ticker;

String friendlyName = "Bindicator";        // CHANGE: Alexa device name
WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
boolean wifiConnected = false;
String serial;
String persistent_uuid;
WiFiUDP ntpUDP;

// Tewkesbury Borough Council REST API Endpoint to get the refuse dates
const char* host = "api-2.tewkesbury.gov.uk";
const int httpsPort = 443;
//This holds the JSON Response from the API call
const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(2) + 4 * JSON_OBJECT_SIZE(6) + 520;
//Because this is HTTPS, I had to get the fingerprint from the Certificate
const char* fingerprint = "37 D7 90 0D 3B 9F 13 9E 43 D3 B1 C9 AB B8 39 90 6E 1F 5C 7B";
//Your PostCode Here - remember - this is only for Tewkesbury Borough Council right now!
String postCode = "gl207rl";
//This is the REST API that Tewkesbury use
String url = "/general/rounds/" + postCode + "/nextCollection";

byte MonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //specify days in each month
String binColour = "";
String binToCollect = "";

void setupDateTime() {
  // setup this after wifi connected
  // you can use custom timeZone,server and timeout
  // DateTime.setTimeZone(-4);
  //   DateTime.setServer("asia.pool.ntp.org");
  //   DateTime.begin(15 * 1000);
  DateTime.setTimeZone(0);
  DateTime.begin();
  if (!DateTime.isTimeValid()) {
    Serial.println("Failed to get time from server.");
  }
}

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

void setBin(int Bin)
{
  Serial.print("Bin = ");
  Serial.println(Bin);
  if (Bin == BLUE_LED) {
    digitalWrite(BLUE_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  } else {
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
  Serial.begin(115200);
  pinMode(BLUE_LED, OUTPUT); //BLUE LED
  pinMode(GREEN_LED, OUTPUT); //GREEN LED
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  Serial.println("Booting");

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  if (!wifiManager.autoConnect("Bindicator")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, HIGH);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Bindicator");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready OTA8");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  setupDateTime();
  Serial.println(DateTime.now());
  DateTimeParts p = DateTime.getParts();
  Serial.printf("Year: %04d\n", p.getYear());
  if (p.getYear() % 4 == 0) {                                        //if its a leap year assign February (entry [1]) 29 days
    MonthDays[1] = 29;
  }
}

void loop() {
  ArduinoOTA.handle();

  if (millis() - ms > 3600000 || firstRun) {
    if (firstRun ) {
      firstRun = 0;
      Serial.println("First run");
    }
    ms = millis();
    Serial.println("--------------------");
    if (!DateTime.isTimeValid()) {
      Serial.println("Failed to get time from server, retry.");
      DateTime.begin();
      DateTimeParts p = DateTime.getParts();
      Serial.printf("Year: %04d\n", p.getYear());
      if (p.getYear() % 4 == 0) {                                        //if its a leap year assign February (entry [1]) 29 days
        MonthDays[1] = 29;
      }
    } else {
      Serial.printf("Up     Time:   %lu seconds\n", millis() / 1000);
      Serial.printf("Local  Time:   %ld\n", DateTime.now());
      Serial.printf("Local  Time:   %s\n", DateTime.toString().c_str());
      Serial.printf("UTC    Time:   %ld\n", DateTime.utcTime());
      Serial.printf("UTC    Time:   %s\n", DateTime.formatUTC(DateFormatter::DATE_ONLY).c_str());
    }
    WiFiClientSecure client;
    Serial.print("connecting to ");
    Serial.println(host);

    Serial.printf("Using fingerprint '%s'\n", fingerprint);
    client.setFingerprint(fingerprint);

    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return;
    }

    Serial.print("requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: ESP8266\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("request sent");
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    String line = client.readStringUntil('\n');

    Serial.println("reply was:");
    Serial.println("==========");
    Serial.println(line);
    Serial.println("==========");
    Serial.println("closing connection");

    DynamicJsonDocument jsonBuffer(capacity);
    auto error = deserializeJson(jsonBuffer, line);
    if (!error) {
      const char* status = jsonBuffer["status"]; // "OK"
      Serial.print("Status: ");
      Serial.println(status);
      JsonArray body = jsonBuffer["body"];

      String binType = "";
      String collectionDateString = "";
      String nextBin = "";
      int collectionDate = 0;
      String todaysDate = "";
      int todaysDay = 0;
      int dayDifference = 0;

      for (JsonVariant value : body) {
        binType = value["collectionType"].as<char*>();
        collectionDateString = value["NextCollection"].as<char*>();
        collectionDate = collectionDateString.substring(8, 10).toInt();

        if (binType == "Refuse") {
          binColour = "Green";
        }
        if (binType == "Recycling") {
          binColour = "Blue";
        }
        if (binType == "Food" || binType == "Garden") {
          break;
        }

        Serial.print("Collection day for : ");
        Serial.print(binColour);
        Serial.print(" is : ");
        Serial.println(collectionDate);

        DateTimeParts p = DateTime.getParts();
        Serial.printf("Month: %04d\n", p.getMonth());
        int previous_month = (p.getMonth() - 1);                      //calculate the index for the previous month (keeping in mind
        //that the indexing starts at 0
        if (p.getMonth() == 0) {                                          //if the current month is 0, the previous month is 11 (zero based indexes)
          previous_month = 11;
        }

        todaysDate = DateTime.formatUTC(DateFormatter::DATE_ONLY).c_str();
        todaysDay = todaysDate.substring(8, 10).toInt();
        Serial.print("Todays day: ");
        Serial.println(todaysDay);
        Serial.print("*PreviousMonth: ");
        Serial.println(previous_month);
        dayDifference = collectionDate - todaysDay;
        Serial.print("*DayDifference:");
        Serial.println(dayDifference);
        Serial.print("*PreviousMonthDays:");
        Serial.println(MonthDays[previous_month]);
        Serial.print("*CurrentMonthDays:");
        Serial.println(MonthDays[p.getMonth()]);
        if (dayDifference < 0) {                                  //Below Zero means we're crossing over the month boundary
          dayDifference += MonthDays[p.getMonth()];               //So add the current months days to the difference to get the correct difference (hopefully)
          Serial.print("*NewDayDifference:");
          Serial.println(dayDifference);
          if ((previous_month == 0) && (p.getYear() % 4) == 0)    // Is it a leap year?
            dayDifference++;                                       // Yes.  Add 1 more day.
          Serial.print("*FinalDayDifference:");
          Serial.println(dayDifference);
        }

        Serial.print("difference = ");
        Serial.println(dayDifference);
        if (dayDifference < 7 && dayDifference > 0) {           //Leave the bin colour for todays collection just in case you forgot to put the bin out!
          Serial.print("Bin to collect: ");
          binToCollect = binColour;
          Serial.println(binToCollect);
        }
      }

    }
    Serial.print("Please put out the ");
    Serial.print(binToCollect);
    Serial.println(" bin");
    if (binToCollect == "Blue") {
      setBin(BLUE_LED);
    }
    if (binToCollect == "Green") {
      setBin(GREEN_LED);
    }
  }

}
