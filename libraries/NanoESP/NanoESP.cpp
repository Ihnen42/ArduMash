
/*
  NanoESP.c - Library for the IoT WiFi-Board NanoESP (aka Pretzelboard)
  Created by Fabian Kainka, f.kainka@gmx.de, 29.09.2015
  www.iot.fkainka.de
*/

#include "NanoESP.h"

NanoESP::NanoESP():
  SoftwareSerial(11, 12)
{
}


//----------------------------------Basics-----------------------------------------

boolean NanoESP::init()
{
  boolean success = true;
  this->begin(19200);
  success &= reset();
  success &= setTransferMode() ;
  success &= setMultipleConnections();
  return success;
}

boolean NanoESP::setMultipleConnections() {
  boolean success = true;
  success &= sendCom("AT+CIPMUX=1", "OK"); //Allways multiple connections
  return success;
}

boolean NanoESP::setTransferMode() {
  boolean success = true;
  success &= sendCom("AT+CIPMODE=0", "OK"); //Normal trasfer mode
  return success;
}

boolean NanoESP::reset() {
  boolean success = true;
  this->setTimeout(5000);
  success &= sendCom("AT+RST", "ready");
  this->setTimeout(1000);
  return success;
}

boolean NanoESP::sendCom(String command, char respond[])
{
  this->println(command);
  if (this->findUntil(respond, "ERROR"))
  {
    return true;
  }
  else
  {
    //debug("ESP SEND ERROR: " + command);
    return false;
  }
}

String NanoESP::sendCom(String command)
{
  this->println(command);
  return this->readString();
}

//----------------------------------WiFi-Functions-----------------------------------------

boolean NanoESP::configWifi(int modus, String ssid, String password) 
{
  boolean success = true;
  switch (modus)
  {
    case 1:
      success &= configWifiMode(1);
      success &= configWifiStation(ssid, password);
      return success;
      break;

    case 2:
      success &= configWifiMode(2);
      success &= configWifiAP(ssid, password);
      return success;
      break;

    case 3:
      success &= configWifiMode(3);
      success &= configWifiStation(ssid, password);
      return success;
      break;
  }
}

boolean NanoESP::configWifiMode(int modus)
{
  return (sendCom("AT+CWMODE=" + String(modus), "OK"));
}


boolean NanoESP::configWifiStation(String ssid, String password)
{
  boolean success = true;
  this->setTimeout(20000);
  success &= (sendCom("AT+CWJAP=\"" + String(ssid) + "\",\"" + String(password) + "\"", "OK"));
  this->setTimeout(1000);
  return success;
}

boolean NanoESP::configWifiAP( String ssid, String password)
{
  boolean success = true;
  this->setTimeout(20000);
  if (password == "") success &= (sendCom("AT+CWSAP=\"" + String(ssid) + "\",\"\",5,0", "OK"));
  else  success &= (sendCom("AT+CWSAP=\"" + String(ssid) + "\",\"" + String(password) + "\",5,4", "OK"));
  this->setTimeout(1000);
  return success;
}


boolean NanoESP::configWifiAP( String ssid, String password, int channel, int crypt)
{
  boolean success = true;
  this->setTimeout(20000);
  success &= (sendCom("AT+CWSAP=\"" + String(ssid) + "\",\"" + String(password) + "\"," + String(channel) + "," + String(crypt), "OK"));
  this->setTimeout(1000);
  return success;
}

boolean NanoESP::disconnectWifi()
{
  boolean success = true;
  success &= (sendCom("AT+CWWAP", "OK"));
  return success;
}

String NanoESP::getIp()
{
  return sendCom("AT+CIFSR");
}


//----------------------------------Connection-Functions-----------------------------------------

boolean NanoESP::newConnection(int id, String type, String ip , int port) {  //single direction
  boolean success = true;
  success &= sendCom("AT+CIPSTART=" + String(id) + ",\"" + String(type) + "\",\"" + String(ip) + "\"," + String(port), "OK"); 
  return success;
}

boolean NanoESP::closeConnection(int id) {
  boolean success = true;
  success &= sendCom("AT+CIPCLOSE=" + String(id), "OK");
  return success;
}

boolean NanoESP::startUdpServer(int id, String ip , int port, int recvport, int mode) { //dual direction
  boolean success = true;
  success &= sendCom("AT+CIPSTART=" + String(id) + ",\"UDP\",\"" + String(ip) + "\"," + String(port) + "," + String(recvport) + "," + String(mode), "OK");
  return success;
}

boolean NanoESP::endUdpServer(int id) {
  return closeConnection(id);
}

boolean NanoESP::startTcpServer(int port) {
  boolean success = true;
  success &= sendCom("AT+CIPSERVER=1," + String(port), "OK");
  return success;
}

boolean NanoESP::endTcpServer() {
  boolean success = true;
  success &= sendCom("AT+CIPSERVER=0", "OK");
  success &= init(); //Restart?!?
  return success;
}

boolean NanoESP::sendData(int id, String msg) {
  boolean success = true;

  success &= sendCom("AT+CIPSEND=" + String(id) + "," + String(msg.length() + 2), ">");
  if (success)
  {
    success &= sendCom(msg, "OK");
  }
  return success;
}

boolean NanoESP::sendDataClose(int id, String msg) {
  boolean success = true;

  success &= sendCom("AT+CIPSEND=" + String(id) + "," + String(msg.length() + 2), ">");
  if (success)
  {
    success &= sendCom(msg, "OK");
    success &= closeConnection(id);
  }
  return success;
}


int NanoESP::getId() { 
  if (this->available()) // check if the esp is sending a message
  {
    if (this->find("+IPD,"))
    {
      return this->parseInt();
    }
  }
  else return -1;
}

boolean NanoESP::ping(String address) {
  boolean success = true;
  success &= sendCom("AT+PING=" + String(address), "OK");
  return success;
}

//-------------------------------------------------Debug Functions------------------------------------------------------
void NanoESP::serialDebug() {
  while (true)
  {
    if (this->available())
      Serial.write(this->read());
    if (Serial.available())
      this->write(Serial.read());
  }
}
