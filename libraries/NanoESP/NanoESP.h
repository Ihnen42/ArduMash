/*
  NanoESP.h - Library for the IoT WiFi-Board NanoESP (aka Pretzelboard)
  Created by Fabian Kainka, f.kainka@gmx.de, 29.09.2015
  www.iot.fkainka.de
*/


#define STATION 1
#define ACCESSPOINT 2
#define DUAL 3

#define TCP "TCP"
#define UDP "UDP"

#ifndef _NANOESP_H_
#define _NANOESP_H_

#include "Arduino.h"
#include <SoftwareSerial.h>

class NanoESP : public SoftwareSerial {
  public:
    NanoESP() ; 
    boolean init();
    boolean sendCom(String command, char respond[]);
    String sendCom(String command);
    boolean setMultipleConnections();
    boolean setTransferMode() ;
    boolean reset();

    boolean configWifi(int modus, String ssid, String password);
    boolean configWifiMode(int modus);
    boolean configWifiStation(String ssid, String password);
    boolean configWifiAP(String ssid, String password);
    boolean configWifiAP(String ssid, String password, int channel, int crypt);
    boolean disconnectWifi();
    String getIp();

    boolean newConnection(int id, String type, String ip , int port);
    boolean closeConnection(int id) ;
    boolean startUdpServer(int id, String ip , int port, int recvport, int mode=0);
    boolean endUdpServer(int id);
    boolean startTcpServer(int port) ;
    boolean endTcpServer();
    boolean sendData(int id, String msg);
    boolean sendDataClose(int id, String msg);
    int getId(); 
    boolean ping(String adress); 

    void serialDebug();
          
  private :

};

#endif
