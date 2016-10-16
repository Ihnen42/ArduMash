/*
Easy Webserver: Listens on Port 80
Change yourSSID and your PASSWORD
Open your Browser and type in the IP shown in the SerialMonitor
 */
#include <NanoESP.h>
#include <SoftwareSerial.h>

#define SSID "[SSID]"
#define PASSWORD "[PASSWORD]"

#define DEBUG true

NanoESP esp8266 = NanoESP();

void setup()
{
  Serial.begin(19200);

  esp8266.init();
  esp8266.configWifi(STATION, SSID, PASSWORD);
  esp8266.startTcpServer(80);

  digitalWrite(13, HIGH);
  debug(esp8266.getIp());
}

void loop() // run over and over
{
  int clientId = esp8266.getId();
  if (clientId >= 0) {
    String webpage = "<h1>Hello World!";
    esp8266.sendDataClose(clientId, webpage);
  }
}

void debug(String Msg)
{
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
