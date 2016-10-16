#include <NanoESP.h>
#include <SoftwareSerial.h>
/*
TCP-Server for RGB-Control
Change SSID and PASSWORD.
RGB-Led (common cathode) on Pins 3,5,6 
*/

#define SSID "[Your SSID]"
#define PASSWORD "[Your Password]"

#define LED_WLAN 13

#define RED 3
#define GREEN 5
#define BLUE 6
#define GND 4

#define DEBUG true

//Website
const char site[] PROGMEM = {
  "<HTML><HEAD>\n<link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=2.0, user-scalable=yes\">\n<title>\nRGB LED\n</title>\n</HEAD>\n\n<BODY bgcolor=\"#FFFF99\" text=\"#000000\">\n<FONT size=\"6\" FACE=\"Verdana\">\nSelect Color\n</FONT>\n\n<HR>\n<BR>\n<FONT size=\"3\" FACE=\"Verdana\">\nChange the Color<BR>\nof the RGB-LED\n<BR>\n<BR>\n<form method=\"GET\">\n\t<input type=\"color\" name=\"rgb\" onchange=\"this.form.submit()\"><BR>\n</form>\n<BR>\n<HR>\n\n</font>\n</HTML>\0"
};


NanoESP esp8266 = NanoESP();

void setup() {
  Serial.begin(19200);
  esp8266.init();

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  pinMode(GND, OUTPUT);
  digitalWrite(GND, LOW);

  //Dual Wifi Mode
  esp8266.configWifi(STATION, SSID, PASSWORD);
  esp8266.configWifi(ACCESSPOINT, "NanoESP", "");

  //Start UDP and TCP Server
  if ( esp8266.startUdpServer(0, "192.168.255.255", 90, 90))  debug("UDP Server Aktiv"); else debug("UDP Server Error"); 
  if ( esp8266.startTcpServer(80))  debug("TCP Server Aktiv"); else debug("TCP Server Error");

  debug(esp8266.getIp());
  digitalWrite(LED_WLAN, HIGH);
}

void loop() {
  int client = esp8266.getId();

  if ( client  >= 0 )
  {
    debug("Request id:" + String(client));

    if (esp8266.findUntil("?rgb=", "\n"))
    {
      debug("RGB Data");
      String hexstring = esp8266.readStringUntil(' ');
      long number = (long) strtol( &hexstring[3], NULL, 16); //Convert String to Hex http://stackoverflow.com/questions/23576827/arduino-convert-a-sting-hex-ffffff-into-3-int

      // Split them up into r, g, b values
      int r = number >> 16;
      int g = number >> 8 & 0xFF;
      int b = number & 0xFF;

      analogWrite(RED, r);
      analogWrite(GREEN, g);
      analogWrite(BLUE, b);
    }

    if (client > 0) // if client > 0 must be TCP-Request (because client 0 = UDP Server)
    {
      if (esp8266.sendDataClose(client, createWebsite())) debug("Website send OK"); else debug("Website send Error");
    }
  }
}


String createWebsite()
{
  String xBuffer;

  for (int i = 0; i <= sizeof(site); i++)
  {
    char myChar = pgm_read_byte_near(site + i);
    xBuffer += myChar;
  }

  return xBuffer;
}

//-------------------------------------------------Debug Functions------------------------------------------------------

void debug(String Msg)
{
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
