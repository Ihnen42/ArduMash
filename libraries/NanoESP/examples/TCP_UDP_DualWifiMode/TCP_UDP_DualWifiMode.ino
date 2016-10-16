/*
TCP UDP DualWifiMode Example
Change SSID and PASSWORD and IPs if necessary

-Dual WiFi Mode
-Multiple UDP and TCP Connections
-TCP-Server (Webserver)
-Client handling
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

  if (!esp8266.init())debug("Error Init");

  //Typical easy Dualmode config with Error Report, First AccessPoint then Station!
  if (!esp8266.configWifi(ACCESSPOINT, "NanoESP", "")) debug("Error AP");
  if (!esp8266.configWifi(DUAL, SSID, PASSWORD)) debug("Error Station");  else digitalWrite(13, HIGH);

  debug(esp8266.getIp());

  //enable up to 5 Connections, if you want to use a TCP Server dont use all 5!
  boolean vConnect = true;
  vConnect &=  esp8266.newConnection(0, UDP, "192.168.178.25", 80);
  vConnect &=  esp8266.startUdpServer(1, "192.168.178.255", 81, 81);
  vConnect &=  esp8266.newConnection(2, TCP, "www.google.de", 80);

  //Create a TCP Server (only one possible)
  vConnect &= esp8266.startTcpServer(80);
  digitalWrite(13, vConnect);
}

void loop() // run over and over
{
  int client = esp8266.getId();
  if (client >= 0) {

    switch (client) {
      case 0:   //Udp Connection 1
        break;

      case 1:   //Udp Connection 2 - UDP Server
        esp8266.find(":");
        debug(esp8266.readString());
        esp8266.sendData(client, "Hello Yo!");
        break;

      case 3:   //TCP Connection
        break;

      default:    //Client id 2 or 4 = TCP request
        String webpage = "<h1>Hello World!";
        esp8266.sendDataClose(client, webpage);
        break;
    }
  }
}


void debug(String Msg)
{
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
