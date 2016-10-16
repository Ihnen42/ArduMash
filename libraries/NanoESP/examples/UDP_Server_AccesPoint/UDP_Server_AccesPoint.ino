/*
UDP Sever AccessPoint Example
Creates a connectable AccessPoint
Send Msg to 192.168.4.255 (Broadcast) and Port 90 and recieve Msg over Port 90
 */
#include <NanoESP.h>
#include <SoftwareSerial.h>

#define DEBUG true

NanoESP esp8266 = NanoESP();

void setup()
{
  Serial.begin(19200);

  esp8266.init();
  esp8266.configWifi(ACCESSPOINT, "NanoESP", "");

  esp8266.startUdpServer(0, "192.168.4.255", 90, 90);

  digitalWrite(13, HIGH);
  debug(esp8266.getIp());

}

void loop() // run over and over
{
  int client = esp8266.getId();
  if (client >= 0) {
    esp8266.sendData(client, "Hello UDP Client");
  }
}


void debug(String Msg)
{
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
