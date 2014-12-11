/*

Arduino Plant Monitor
By: Jenny Lindberg

---------
Based on:

Phant_CC3000.ino
Post data to SparkFun's data stream server system (phant) using
an Arduino and the CC3000 Shield.
Jim Lindblom @ SparkFun Electronics
Original Creation Date: July 3, 2014

This code is beerware; if you see me (or any other SparkFun
employee) at the local, and you've found our code helpful, please
buy us a round!

Much of this code is largely based on Shawn Hymel's WebClient
example in the SFE_CC3000 library.

Weather Shield Example
By: Nathan Seidle
SparkFun Electronics
Date: November 16th, 2013
License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

Much of this is based on Mike Grusin's USB Weather Board code: https://www.sparkfun.com/products/10586

*/

// SPI and the pair of SFE_CC3000 include statements are required
// for using the CC300 shield as a client device.
#include <SPI.h>
#include <SFE_CC3000.h>
#include <SFE_CC3000_Client.h>
// Progmem allows us to store big strings in flash using F().
// We'll sacrifice some flash for extra DRAM space.
#include <Progmem.h>

#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor

MPL3115A2 myPressure; //Create an instance of the pressure sensor
HTU21D myHumidity; //Create an instance of the humidity sensor

////////////////////////////////////
// CC3000 Shield Pins & Variables //
////////////////////////////////////
// Don't change these unless you're using a breakout board.
#define CC3000_INT      2   // Needs to be an interrupt pin (D2/D3)
#define CC3000_EN       7   // Can be any digital pin
#define CC3000_CS       10  // Preferred is pin 10 on Uno
#define IP_ADDR_LEN     4   // Length of IP address in bytes

////////////////////
// WiFi Constants //
////////////////////
char ap_ssid[] = "oort";                // SSID of network
char ap_password[] = "sibelius";        // Password of network
unsigned int ap_security = WLAN_SEC_WPA2; // Security of network
// ap_security can be any of: WLAN_SEC_UNSEC, WLAN_SEC_WEP,
//  WLAN_SEC_WPA, or WLAN_SEC_WPA2
unsigned int timeout = 30000;             // Milliseconds
char server[] = "data.sparkfun.com";      // Remote host site

// Initialize the CC3000 objects (shield and client):
SFE_CC3000 wifi = SFE_CC3000(CC3000_INT, CC3000_EN, CC3000_CS);
SFE_CC3000_Client client = SFE_CC3000_Client(wifi);

/////////////////
// Phant Stuff //
/////////////////
const String publicKey = "RMzg4wzqx0T54QArpOZq";
const String privateKey = "lzrqN1rpEYFKgWE60mdR";
const byte NUM_FIELDS = 5;
const String fieldNames[NUM_FIELDS] = {"humidity", "light", "temp_h", "temp_p", "pressure"};
String fieldData[NUM_FIELDS];

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// digital I/O pins
const byte GREEN_LED = 8;

// analog I/O pins
const byte REFERENCE_3V3 = A3;
const byte LIGHT = A1;

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const unsigned long READ_INTERVAL = 1000; // 1 second
const unsigned long POST_INTERVAL = 300000; // 5 minutes
const unsigned int NUM_READS = 300; // 300 data points for rolling average

unsigned int counter = 0; // count of averages, to start up from empty data
unsigned long t_next_read = 0;
unsigned long t_next_post = 0;

float humidity = 0.0; // [%]
float temp_h = 0.0; // [temperature F, from humidity]
float temp_p = 0.0; // [temperature F, from pressure]
float pressure = 0.0;
float light = 0.0; //[analog value from 0 to 1023]

float av_humidity = 0.0;
float av_temp_h = 0.0;
float av_temp_p = 0.0;
float av_pressure = 0.0;
float av_light = 0.0;

void setup()
{
  Serial.begin(9600);
  Serial.println("Jenny's Plant Monitor");

  // set up LEDs
  pinMode(GREEN_LED, OUTPUT); //Status LED Green
  digitalWrite(GREEN_LED, LOW); // green LED off

  pinMode(REFERENCE_3V3, INPUT);
  pinMode(LIGHT, INPUT);
  
  // configure the pressure sensor
  myPressure.begin(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags

  // configure the humidity sensor
  myHumidity.begin();

  // set up wifi
  setupWiFi();
  
  // set up read/write intervals
  t_next_read = millis();
  t_next_post = millis() + POST_INTERVAL;

  Serial.println("Plant monitor online!");
}

void loop()
{
  // read sensors
  if (millis() >= t_next_read)
  {
    t_next_read += READ_INTERVAL;
    
    digitalWrite(GREEN_LED, HIGH); // green LED on
    
    Serial.println("Reading sensors");
  
    // read all the various sensors
    readWeather();
    
    // calculate rolling average
    averageWeather();
  
    // report all readings to serial
    printWeather();
    
    digitalWrite(GREEN_LED, LOW); // green LED off
  }
 
  // to the web!
  if (millis() >= t_next_post)
  {
    t_next_post += POST_INTERVAL;
    
    digitalWrite(GREEN_LED, HIGH); // green LED on
    
    Serial.println("==========Posting to phant!=============");
    
    // gather data
    fieldData[0] = String(av_humidity);
    fieldData[1] = String(av_light);
    fieldData[2] = String(av_temp_h);
    fieldData[3] = String(av_temp_p);
    fieldData[4] = String(av_pressure);
  
    // post data
    postData();
    
    digitalWrite(GREEN_LED, LOW); // green LED off
  }
}

// read each of the weather sensors
void readWeather()
{
  // humidity
  humidity = myHumidity.readHumidity();

  // temp from humidity sensor
  temp_h = (myHumidity.readTemperature() * 1.8) + 32;
  
  // must be first to fix temp reading from pressure sensor!
  // much dumb, TODO
  pressure = myPressure.readPressure();

  // temp from pressure sensor
  temp_p = myPressure.readTempF();

  // light level
  light = get_light_level();
}

//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
  float operatingVoltage = analogRead(REFERENCE_3V3);

  float lightSensor = analogRead(LIGHT);

  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V

  lightSensor = operatingVoltage * lightSensor;
  
  // voltage / 10000 resistor (10k) = current * 1000000 (to microamps) * 7 = lux * 0.0929030436 = footcandles
  return(lightSensor * 100 * 7 * 0.0929030436);
}

void averageWeather()
{ 
  if (counter < NUM_READS)
  {
    counter++;
  }
  
  float fraction = 1.0 / counter;
  float fraction_diff = 1.0 - fraction;
  
  av_humidity = (fraction * humidity) + (fraction_diff * av_humidity);
  av_temp_h = (fraction * temp_h) + (fraction_diff * av_temp_h);
  av_temp_p = (fraction * temp_p) + (fraction_diff * av_temp_p);
  av_pressure = (fraction * pressure) + (fraction_diff * av_pressure);
  av_light = (fraction * light) + (fraction_diff * av_light);
}

// prints last reading directly to the serial port
void printWeather()
{
  Serial.print("humidity=");
  Serial.print(humidity, 1);
  Serial.print(", temp_h=");
  Serial.print(temp_h, 1);
  Serial.print(", temp_p=");
  Serial.print(temp_p, 1);
  Serial.print(", pressure=");
  Serial.print(pressure, 1);
  Serial.print(", light=");
  Serial.print(light, 2);
  Serial.println();
  
  Serial.print("av_humidity=");
  Serial.print(av_humidity, 1);
  Serial.print(", av_temp_h=");
  Serial.print(av_temp_h, 1);
  Serial.print(", av_temp_p=");
  Serial.print(av_temp_p, 1);
  Serial.print(", av_pressure=");
  Serial.print(av_pressure, 1);
  Serial.print(", av_light=");
  Serial.print(av_light, 2);
  Serial.println();
}

void postData()
{
  // re-connect to wifi if not connected
  // randomly drops out
  connectWiFi();
  
  // Make a TCP connection to remote host
  if ( !client.connect(server, 80) )
  {
    // Error: 4 - Could not make a TCP connection
    Serial.println(F("Error: 4"));
  }

  // Post the data! Request should look a little something like:
  // GET /input/publicKey?private_key=privateKey&light=1024&switch=0&time=5201 HTTP/1.1\n
  // Host: data.sparkfun.com\n
  // Connection: close\n
  // \n
  client.print("GET /input/");
  client.print(publicKey);
  client.print("?private_key=");
  client.print(privateKey);
  for (int i=0; i<NUM_FIELDS; i++)
  {
    client.print("&");
    client.print(fieldNames[i]);
    client.print("=");
    client.print(fieldData[i]);
  }
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(server);
  client.println("Connection: close");
  client.println();
  
  while (client.connected())
  {
    if ( client.available() )
    {
      char c = client.read();
      Serial.print(c);
    }
  }
  Serial.println();
}

void setupWiFi()
{
  ConnectionInfo connection_info;
  int i;

  // Initialize CC3000 (configure SPI communications)
  if ( wifi.init() )
  {
    Serial.println(F("CC3000 Ready!"));
  }
  else
  {
    // Error: 0 - Something went wrong during CC3000 init!
    Serial.println(F("Error: 0"));
  }

  connectWiFi();  

  // Gather connection details and print IP address
  if ( !wifi.getConnectionInfo(connection_info) )
  {
    // Error: 2 - Could not obtain connection details
    Serial.println(F("Error: 2"));
  }
  else
  {
    Serial.print(F("My IP: "));
    for (i = 0; i < IP_ADDR_LEN; i++)
    {
      Serial.print(connection_info.ip_address[i]);
      if ( i < IP_ADDR_LEN - 1 )
      {
        Serial.print(".");
      }
    }
    Serial.println();
  }
}

void connectWiFi()
{
  // Connect using DHCP
  Serial.print(F("Connecting to: "));
  Serial.println(ap_ssid);
  if(!wifi.connect(ap_ssid, ap_security, ap_password, timeout))
  {
    // Error: 1 - Could not connect to AP
    Serial.println(F("Error: 1"));
  }
}
