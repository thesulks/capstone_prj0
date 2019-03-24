#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <time.h>

ESP8266WiFiMulti WiFiMulti;

// Data wire is plugged into port D2 on the Arduino
#define ONE_WIRE_BUS D2

#define TIMEZONE_SEOUL 9
#define YEAR 0
#define MONTH 1
#define DATE 2
#define HOUR 3
#define MIN 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

/* Setup function. */
void setup(void)
{
  // start serial port
  Serial.begin(115200);
  Serial.println("*** Temperature sensor ***");

  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  // Assign address manually. The addresses below will beed to be changed
  // to valid device addresses on your bus. Device address can be retrieved
  // by using either oneWire.search(deviceAddress) or individually via
  // sensors.getAddress(deviceAddress, index)
  // Note that you will need to use your specific address here
  //insideThermometer = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };

  // Method 1:
  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then 
  // use those addresses and manually assign them (see above) once you know 
  // the devices on your bus (and assuming they don't change).
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  
  // method 2: search()
  // search() looks for the next device. Returns 1 if a new address has been
  // returned. A zero might mean that the bus is shorted, there are no devices, 
  // or you have already retrieved all of them. It might be a good idea to 
  // check the CRC to make sure you didn't get garbage. The order is 
  // deterministic. You will always get the same devices in the same order
  //
  // Must be called before search()
  //oneWire.reset_search();
  // assigns the first address found to insideThermometer
  //if (!oneWire.search(insideThermometer)) Serial.println("Unable to find address for insideThermometer");

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("iptime_rod", "temppw19");

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("...DONE");

  configTime(TIMEZONE_SEOUL * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("...DONE");
}

/*
 * Main function. It will request the tempC from the sensors and display on Serial.
 */
void loop(void)
{ 
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;
    float tempC;

    String ymdhm[5];
    String aws_url = "http://ec2-52-78-98-155.ap-northeast-2.compute.amazonaws.com:9000/";
    time_t now = time(nullptr);
    struct tm *timeinfo;
    timeinfo = localtime(&now);

    if (timeinfo->tm_year == 70) return;

    ymdhm[YEAR]=String(timeinfo->tm_year + 1900, DEC);  // years since 1900
    ymdhm[MONTH]=String(timeinfo->tm_mon + 1, DEC);     // months since January(0)
    ymdhm[DATE]=String(timeinfo->tm_mday, DEC); 
    ymdhm[HOUR]=String(timeinfo->tm_hour, DEC); 
    ymdhm[MIN]=String(timeinfo->tm_min, DEC); 

    //if(timeinfo->tm_min<10)      a[MIN]="0"+a[MIN];
    if(timeinfo->tm_mon<10)      ymdhm[MONTH]="0" + ymdhm[MONTH];
    if(timeinfo->tm_mday<10)     ymdhm[DATE]="0" + ymdhm[DATE];
    if(timeinfo->tm_min<10)      ymdhm[MIN]="0" + ymdhm[MIN];
    if(timeinfo->tm_hour<10)     ymdhm[HOUR]="0" + ymdhm[HOUR];

    Serial.print(ymdhm[YEAR] + "/" +ymdhm[MONTH] + "/" + ymdhm[DATE] + " " + ymdhm[HOUR] + ":" + ymdhm[MIN] + "\n");
  
    // call sensors.requestTemperatures() to issue a global temperature 
    // request to all devices on the bus
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");

    // Serial output
    tempC = sensors.getTempC(insideThermometer);
    Serial.print("Temp C: ");
    Serial.print(tempC);
    Serial.print(" Temp F: ");
    Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit

    aws_url += ("log?d=" + ymdhm[YEAR] + ymdhm[MONTH] + ymdhm[DATE]);
    aws_url += ("&h=" + ymdhm[HOUR] + "&m=" + ymdhm[MIN] + "&t=" + String(tempC)); 

    // Write to my channel
    Serial.print("*** Thinkspeak ***\n");
    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, "http://api.thingspeak.com/update?api_key=KK59HDS3Q4NGWZ7P&field1=" + String(tempC))) {

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    }
    else {
      Serial.printf("[HTTP} Unable to connect\n");
    }

    // Write to my AWS server
    Serial.print("*** AWS Server ***\n");
    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, aws_url)) {

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    }
    else {
      Serial.printf("[HTTP} Unable to connect\n");
    }

    // Period: 1 minute
    delay(60000);
  }  
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
