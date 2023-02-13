/*
 * Library to gather all environmental data like real time and temperature
 * (c) Cedric Millard 2020
 */

#include "WebMQTTPublisher.h"
#include <Config.h>

void WebMQTTPublisher::handleMqttCallback(char* iTopic, byte* payload, unsigned int iLength)
{
#ifdef _DEBUG_  
  Serial.println(ESP.getFreeHeap());
  Serial.print("Message arrived [");
  Serial.print(iTopic);
  Serial.print("] ");
  for (int i = 0; i < iLength; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif  
  String topic = iTopic;
  char *tempString = (char *) malloc(sizeof(char)*(iLength+1));
  memcpy(tempString, payload, iLength);
  tempString[iLength] = '\0';
  String sPayload(tempString);
  free(tempString);

  if(sPayload.length()>0)
  {
    if(topic.startsWith("/net/prise/"))
    {
      //retrieve ID of the prise
      String sID = topic.substring(11, topic.indexOf("/", 11));
      short iID = sID.toInt();
  
      if(!aPrises.exists(iID))
      {
  #ifdef _DEBUG_  
    Serial.println("Create new Prise");
  #endif        
        Prise *newPrise = new Prise(iID);
        newPrise->setLog(pLog);
        aPrises.add(newPrise, iID);
        *needRefresh = true;
      }
      Prise *currentPrise = aPrises.getItem(iID);
      if(topic.endsWith("/name")) 
      {
        currentPrise->name = sPayload;
      }
      if(topic.endsWith("/status")) 
      {
        currentPrise->status= sPayload.toInt();
        *needRefresh = true;
      }
      if(topic.endsWith("/rules")) 
      {
        currentPrise->jsonToRules(sPayload);
        *needRefresh = true;
      }
      if(topic.endsWith("/type"))
      {
        switch(sPayload.toInt())
        {
          case 1:currentPrise->type = objType::priseRF;break;
          case 2:currentPrise->type = objType::priseWifi;break;
          default: currentPrise->type = objType::prise;break;
        }
      }
      currentPrise->_lastUpdate = time(nullptr);
    }
    else if(topic.startsWith("/net/sensor/"))
    {
      //retrieve ID of the sensor
      String sID = topic.substring(12, topic.indexOf("/", 12));
      short iID = sID.toInt();
  #ifdef _DEBUG_  
    Serial.print("ID of Sensor = ");
    Serial.println(iID);
  #endif  
      if(!aSensors.exists(iID))
      {
        Sensor *newSensor = new Sensor(iID);
        aSensors.add(newSensor, iID);
        *needRefresh = true;
      }
      Sensor *pCurrentSensor = aSensors.getItem(iID);
      if(topic.endsWith("/name"))pCurrentSensor->name = sPayload;
      if(topic.endsWith("/type")) 
      {
        pCurrentSensor->sType= static_cast<sensorType>(sPayload.toInt());
      }
      pCurrentSensor->_lastUpdate = time(nullptr);
    }
    else if(topic.startsWith("/net/display/"))
    {
      //retrieve ID of the display
      String sID = topic.substring(13, topic.indexOf("/", 13));
      short iID = sID.toInt();
  #ifdef _DEBUG_  
    Serial.print("ID of Display = ");
    Serial.println(iID);
  #endif  
      if(!aDisplays.exists(iID))
      {
        WeatherDisplayObj *newDisplay = new WeatherDisplayObj(iID);
        aDisplays.add(newDisplay, iID);
      }
      WeatherDisplayObj *pCurrDisplay = aDisplays.getItem(iID);
      if(topic.endsWith("/name")) pCurrDisplay->name = sPayload;
      if(topic.endsWith("/leftInfo")) pCurrDisplay->leftInfo = sPayload;
      if(topic.endsWith("/rightInfo")) pCurrDisplay->rightInfo = sPayload;
      if(topic.endsWith("/layout")) pCurrDisplay->layout = sPayload.toInt();
      pCurrDisplay->_lastUpdate = time(nullptr);
    }
    else if(topic.startsWith("/sensor/"))
    {
      //retrieve ID of the sensor
      String sID = topic.substring(topic.indexOf("/sensor/")+8, topic.indexOf("/", topic.indexOf("/sensor/")+8));
      short iID = sID.toInt();
      if(aSensors.exists(iID))
          aSensors.getItem(iID)->_lastUpdate = time(nullptr);
    }
  }

// dump(); 
}

WebMQTTPublisher::WebMQTTPublisher(PubSubClient *iMqttClientEnv, bool *iNeedRefresh, short iId) : MyMQTTClient(iMqttClientEnv, String(iId) + "_WebPub"), WebPage()
{
  addTopic(NET_TOPIC);
  addTopic(SENSOR_TOPIC);
  needRefresh = iNeedRefresh;
}

void WebMQTTPublisher::dump()
{
  Serial.println("--------");
  Serial.println("Prises:");
  for(int i=0; i<aPrises.size();i++) Serial.printf("ID: %d Name: %s \n", aPrises[i]->Id, aPrises[i]->name.c_str());
  Serial.println("Sensors:");
  for(int i=0; i<aSensors.size();i++) Serial.printf("ID: %d Name: %s \n", aSensors[i]->Id, aSensors[i]->name.c_str());
  Serial.println("--------");
}

bool WebMQTTPublisher::update(bool iForce)
{
  MyMQTTClient::update(iForce);
  if(millis()-lastUpdateTime > SENSOR_UPDATE_FREQ*10000 || iForce)
  {
    lastUpdateTime = millis();
#ifdef _DEBUG_
    Serial.println("Clean Sensor list");
#endif
    for(int i=0; i<aSensors.size();i++)
    {
      long deltaT = difftime(time(nullptr), aSensors[i]->_lastUpdate);
      int iID = aSensors[i]->Id;
      if(deltaT>SENSOR_UPDATE_FREQ*100 && aSensors[i]->sType!=temp_ro)
      {
        String pubTopic = "/sensor/" + String( iID) + "/value";
        publishMsg(pubTopic, String(""), false);
#ifdef _DEBUG_
  Serial.printf("Sensor %d do not refresh => publish void value\n", iID);
#endif  
      }
      if(deltaT>PUBLISH_PARAMS_FREQ*25 && aSensors[i]->sType!=temp_ro)
      {
#ifdef _DEBUG_
  Serial.printf("Sensor %d do not refresh net => publish void value and remove\n", iID);
#endif        
          String pubTopic = "/net/sensor/" + String( iID) + "/type";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/sensor/" + String(iID) + "/name";
          publishMsg(pubTopic, String(""), true);

          aSensors.removeId(iID);
      }
    }
    for(int i=0; i<aPrises.size();i++)
    {
      long deltaT = difftime(time(nullptr), aPrises[i]->_lastUpdate);
      if(deltaT>PUBLISH_PARAMS_FREQ*25)
      {
        int iID = aPrises[i]->Id;
#ifdef _DEBUG_
  Serial.printf("Prise %d do not refresh net => publish void value and remove\n", iID);
#endif 
          String pubTopic = "/net/prise/" + String( iID) + "/type";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/prise/" + String( iID) + "/name";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/prise/" + String( iID) + "/status";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/prise/" + String( iID) + "/rules";
          publishMsg(pubTopic, String(""), true);

          aPrises.removeId(iID);
      }
    }
    for(int i=0; i<aDisplays.size();i++)
    {
      long deltaT = difftime(time(nullptr), aDisplays[i]->_lastUpdate);
      if(deltaT>PUBLISH_PARAMS_FREQ*25)
      {
        int iID = aDisplays[i]->Id;
#ifdef _DEBUG_
  Serial.printf("Display %d do not refresh net => publish void value and remove\n", iID);
#endif 

          String pubTopic = "/net/display/" + String( iID) + "/name";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/display/" + String( iID) + "/leftInfo";
          publishMsg(pubTopic, String(""), true);

          pubTopic = "/net/display/" + String( iID) + "/rightInfo";
          publishMsg(pubTopic, String(""), true);

          aDisplays.removeId(iID);
      }
    }
  }
}
