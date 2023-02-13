/*
 * TODO: 
 * 
 * + Ajouter date? (trigger dans une plage de dates)
 * 
 * If MQTT server is down, sockets are stucked... Not working fully standalone...
 *    
 * + Handle battery icon in bottom for weather station 2 (when unit is V, display a battery icon? need to calibrate range)
 * *   - 3.3V = 0% / 4.24V = 100%
 * 
 * + Weather station 2
 *   + Manage sleep mode for running on battery
 *   
 *   Possible bugs: long rules, last action detected as toggle which is not possible (OK from web server, issue in PriseIOT) (not able to reproduce)
 *   Cannot delete rules (in web interface (not able to reproduce)
 *   In ESP8266 webppage, often all rules are not displayed (risk of crash if submit) => freq reduced since last update but still happens quite often
 * 
 */
#include "Config_WeatherStation.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <Environment.h>
#include "WeatherService.h"
#include "WeatherDisplay.h"
#include <ArrayCed.h>
#include "WebMQTTPublisher.h"
#include <ConnectedObjects.h>
#include <ArduinoOTA.h>
#include <Logging.h>
#include <Settings.h>
#ifndef ESP8266
#include <SPIFFS.h>
#endif

//Flag to indicate if prise is a wifi client or an access point
bool wifiAP = false;
unsigned long wifiReconnectTime = 0;

AsyncWebServer server(80);

WiFiClient wifiClientEnv;
WiFiClient wifiClientWeather;
WiFiClient wifiClientWebPublisher;
WiFiClient wifiClientPrise433_1;
WiFiClient wifiClientPrise433_2;
WiFiClient wifiClientDisplay;
WiFiClient wifiClientTemp;
//WiFiClient wifiClientVcc;
PubSubClient mqttClientEnv(wifiClientEnv);
PubSubClient mqttClientWeather(wifiClientWeather);
PubSubClient mqttClientWebPub(wifiClientWebPublisher);
PubSubClient mqttClientPrise433_1(wifiClientPrise433_1);
PubSubClient mqttClientPrise433_2(wifiClientPrise433_2);
PubSubClient mqttClientDisplay(wifiClientDisplay);
PubSubClient mqttClientTemp(wifiClientTemp);
//PubSubClient mqttClientVcc(wifiClientVcc);

//Flag that indicates web browser to refresh
bool needRefresh = false;

//Flag to handle weather not available => avoid fetching weather too often to not reach the 1000 request per day.
unsigned long _lastWeatherUpdate = 0;

Settings *mySettings = NULL;
Logging *myLog = NULL;
Environment *myEnv = new Environment(&mqttClientEnv, PRISE1_ID);
WeatherService *myWeatherService = new WeatherService(&mqttClientWeather, PRISE1_ID);
WebMQTTPublisher *myWebPublisher = new WebMQTTPublisher(&mqttClientWebPub, &needRefresh, PRISE1_ID);
//ESP_Vcc *vccSensor = new ESP_Vcc(&mqttClientVcc, VCC_ID, ADDRESS_SENSOR_VCC);

PriseIOT_RF433 *prise433_1 = new PriseIOT_RF433(&mqttClientPrise433_1, PRISE1_ID, TX_PIN, NEXA_CODE, 3);
PriseIOT_RF433 *prise433_2 = new PriseIOT_RF433(&mqttClientPrise433_2, PRISE2_ID, TX_PIN, NEXA_CODE, 2, 512);

TempSensorDS18B20 *tempSensor = new TempSensorDS18B20(&mqttClientTemp, TEMP_ID, TEMP_PIN, ADDRESS_SENSOR_TEMP);

int Objet::eepromSize{ 1100 };
bool Objet::eepromInit{ false };

WeatherDisplay *myDisplay = new WeatherDisplay(&mqttClientDisplay, PRISE1_ID, myEnv, 1024);

void callbackEnv(char* topic, byte* payload, unsigned int length) {
  myEnv->handleMqttCallback(topic, payload, length);
}

void callbackWeb(char* topic, byte* payload, unsigned int length) {
  myWebPublisher->handleMqttCallback(topic, payload, length);
}

void callbackPrise_1(char* topic, byte* payload, unsigned int length) {
  prise433_1->handleMqttCallback(topic, payload, length);
}

void callbackPrise_2(char* topic, byte* payload, unsigned int length) {
  prise433_2->handleMqttCallback(topic, payload, length);
}

void callbackDisplay(char* topic, byte* payload, unsigned int length) {
  myDisplay->handleMqttCallback(topic, payload, length);
}

/*void callbackVcc(char* topic, byte* payload, unsigned int length) {
  vccSensor->handleMqttCallback(topic, payload, length);
}*/

void callbackTemp(char* topic, byte* payload, unsigned int length) {
  tempSensor->handleMqttCallback(topic, payload, length);
}

void checkWifi()
{
  //If wifi connection lost, try to reconnect every 15 seconds (because ESP will keep initiating the connection in the background). 
  if(WiFi.status() != WL_CONNECTED && millis()-wifiReconnectTime>15000)
  {
#ifdef _DEBUG_  
      Serial.print("Wifi connection lost, trying to reconnect");
#endif 
    //To not flood the log, raise up message every hour only
    if(millis()-wifiReconnectTime>3600000)
      myLog->addLogEntry("Wifi connection lost, trying to reconnect");

    WiFi.mode(WIFI_STA);
    WiFi.begin(mySettings->getWifiSSID(), mySettings->getWifiPWD());
    wifiReconnectTime = millis();
  }
  
}

//Set the ESP as an AP to configure it
void setWifiAP()
{
  wifiAP = true;
  WiFi.mode(WIFI_AP);
  IPAddress local_ip(192,168,0,1);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP("myHAS", "12345678");
}

void connectWifi(unsigned long iTimeOut = -1)
{
  if(mySettings->isWifiSetup())
  {
     if(WiFi.status() != WL_CONNECTED)
     {
        WiFi.mode(WIFI_STA);
        WiFi.begin(mySettings->getWifiSSID(), mySettings->getWifiPWD());
#ifdef _DEBUG_  
        Serial.print("Connecting to WiFi");
#endif 
        unsigned long currentMillis = millis();
        while (WiFi.status() != WL_CONNECTED && ((unsigned long)(millis()-currentMillis)<iTimeOut || iTimeOut==-1) )
        {
          delay(500);
#ifdef _DEBUG_  
          Serial.print(".");
#endif    
        }

        if(WiFi.status() != WL_CONNECTED)
        {
          WiFi.disconnect();
          setWifiAP();
        }

#ifdef _DEBUG_  
        Serial.println("\nConnected !");
#endif     

     }
  }
  else
  {
    setWifiAP();
  }
  
  
}

void setup()
{    
  Serial.begin(115200);
  mySettings = new Settings();
  String wifiList = "";
  int nbNetworks = WiFi.scanNetworks();
 
  for(int i =0; i<nbNetworks; i++){
      if(wifiList.indexOf(WiFi.SSID(i))==-1)
        wifiList += WiFi.SSID(i)+";";
  }
  mySettings->setWifiList(wifiList);
  
  connectWifi(30000);

  // Print ESP32 Local IP Address
#ifdef _DEBUG_  
Serial.println(WiFi.localIP());
#endif

  initiatlizeWebServer();
  if(!wifiAP)
  {
    ArduinoOTA
      .onStart([]() {
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
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(OTA_NAME);
    ArduinoOTA.setPassword(mySettings->getOTAPWD());
    ArduinoOTA.begin();
    
    //initialize MQTT clients
    //mqttClientEnv.setServer(MQTT_SERVER, 1885);
    myEnv->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientEnv.setCallback(callbackEnv);

    myLog = new Logging(PRISE1_ID);
    myEnv->setLog(myLog);

    //mqttClientWeather.setServer(MQTT_SERVER, 1885);
    myWeatherService->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientWeather.setCallback(NULL);
    myWeatherService->setLog(myLog);
    myWeatherService->setEnv(myEnv);
    
    //mqttClientDisplay.setServer(MQTT_SERVER, 1885);
    myDisplay->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientDisplay.setCallback(callbackDisplay);
    myDisplay->setLog(myLog);
    myDisplay->init();

    //mqttClientWebPub.setServer(MQTT_SERVER, 1885);
    myWebPublisher->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientWebPub.setCallback(callbackWeb);
    myWebPublisher->setLog(myLog);
    myWebPublisher->setEnv(myEnv);

    //mqttClientPrise433_1.setServer(MQTT_SERVER, 1885);
    prise433_1->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientPrise433_1.setCallback(callbackPrise_1);
    prise433_1->setEnv(myEnv);
    prise433_1->setLog(myLog);
    prise433_1->init();
    

    //mqttClientPrise433_2.setServer(MQTT_SERVER, 1885);
    prise433_2->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    prise433_2->setEnv(myEnv);
    prise433_2->setLog(myLog);
    prise433_2->init();

    //mqttClientTemp.setServer(MQTT_SERVER, 1885);
    tempSensor->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientTemp.setCallback(callbackTemp);
    tempSensor->setEnv(myEnv);
    tempSensor->setLog(myLog);
    tempSensor->init();
    
    /*
    vccSensor->setMqttServer(mySettings->getMqttServer(), mySettings->getMqttPort(), mySettings->getMqttLogin(), mySettings->getMqttPWD());
    mqttClientVcc.setCallback(callbackVcc);
    vccSensor->setEnv(myEnv);
    vccSensor->init();*/
  }
  
}

void loop()
{
  if(!wifiAP)
  {
    checkWifi();
    
    ArduinoOTA.handle();
    
    tempSensor->update();
    myEnv->update();
    bool force = false;
    if(myEnv->getWeatherDay(3).Weather == 0 && (unsigned long)(millis()-_lastWeatherUpdate)>90000) 
    {
      force = true;
      _lastWeatherUpdate = millis();
    }
    myWeatherService->update(force);
    myDisplay->update();
    prise433_1->update();
    prise433_2->update();
    myWebPublisher->update();
    //vccSensor->update();
    //delay(100);
  }
}

void initiatlizeWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(wifiAP) request->redirect("/settings");
    else
    {
      request->send_P(200, "text/html", myWebPublisher->getIndexHTML().c_str());
    }
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, LOG_FILE_PATH, "text/plain");
  });

  server.on("/refresh", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(needRefresh).c_str());
    needRefresh = false;
  });

  //Click on edit rules
  server.on("/rules", HTTP_GET, [](AsyncWebServerRequest *request){
    int iID = request->getParam("ID")->value().toInt();
    request->send_P(200, "text/html", myWebPublisher->getRulesHTML(iID).c_str());
  });

  server.on("/saveRules", HTTP_POST, [](AsyncWebServerRequest *request){
    //Publish rules
    int iNbRule = request->getParam("NbRule", true)->value().toInt();
    int iID = request->getParam("ID", true)->value().toInt();
#ifdef _DEBUG_      
    Serial.printf("NbRule %d ID = %d\n", iNbRule, iID);
#endif    
    myWebPublisher->aPrises.getItem(iID)->aRules.removeAll();
    
    for(int i=1; i<=iNbRule; i++)
    {
      if(request->getParam("status" + String(i), true)->value()!="deleted")
      {
        Rule currentRule;
        currentRule.frequency = request->getParam("Freq" + String(i), true)->value().toInt();
        currentRule.condition = request->getParam("rule" + String(i), true)->value();
        String sAction = request->getParam("action" + String(i), true)->value();
        if (sAction=="turnON") currentRule.action = turnOn;
        //else if (sAction=="turnOFF") currentRule.action = turnOff;
        else if (sAction=="blink") currentRule.action = Action::blink;
        else currentRule.action = toggle;
        if(request->getParam("status" + String(i), true)->value()=="active")
          currentRule.active = true;
        else
          currentRule.active = false;
        myWebPublisher->aPrises.getItem(iID)->aRules.add(currentRule);
#ifdef _DEBUG_        
      Serial.printf("Rule Added: freq=%d rule=%s action=%s\n",currentRule.frequency, currentRule.condition.c_str(), sAction.c_str());
#endif 
      }
#ifdef _DEBUG_        
      else Serial.println("1 Rule Deleted");
#endif      
    }
    
    String topic = "/prise/" + String(iID) + "/rules";
    myWebPublisher->publishMsg(topic, myWebPublisher->aPrises.getItem(iID)->rulesToJson(), false);
    
    request->redirect("/");
  });

  //Change a button status in Webpage
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
#ifdef _DEBUG_      
    Serial.printf("Number of GET param = %d\n", (int)(request->params()));
#endif
    for(int i=0; i<request->params(); i++)
    {
      String paramName = request->getParam(i)->name();
      String paramValue = request->getParam(i)->value();
Serial.printf("Param %d name = %s value = %s\n", i, paramName.c_str(), paramValue.c_str());
      String objID = paramName.substring(paramName.indexOf('_')+1, paramName.lastIndexOf('_'));
      if(paramName.startsWith("prise_"))
      {
        if(paramName.endsWith("_name"))
        {
          if(myWebPublisher->aPrises.getItem(objID.toInt())->name != paramValue) 
          {
            String topic = "/prise/" + objID + "/name";
            myWebPublisher->publishMsg(topic, paramValue, false);
            myWebPublisher->aPrises.getItem(objID.toInt())->name = paramValue;
          }
        }
        else if(paramName.endsWith("_status"))
        {
          if(myWebPublisher->aPrises.getItem(objID.toInt())->status ^ paramValue.toInt()) {
            String topic = "/prise/" + objID + "/status";
            myWebPublisher->publishMsg(topic, paramValue, false);
            myWebPublisher->aPrises.getItem(objID.toInt())->status = paramValue.toInt();
          }
        }
      }
      else if(paramName.startsWith("sensor_")) 
      {
        if(paramName.endsWith("_name"))
        {
          if(myWebPublisher->aSensors.getItem(objID.toInt())->name != paramValue) 
          {
            String topic = "/sensor/" + objID + "/name";
            myWebPublisher->publishMsg(topic, paramValue, false);
            myWebPublisher->aSensors.getItem(objID.toInt())->name = paramValue;
          }
        }
      }
      else if(paramName.startsWith("display_")) 
      {
        if(paramName.endsWith("_name"))
        {
          if(myWebPublisher->aDisplays.getItem(objID.toInt())->name != paramValue) 
          {
            String topic = "/display/" + objID + "/name";
            myWebPublisher->publishMsg(topic, paramValue, true);
            myWebPublisher->aDisplays.getItem(objID.toInt())->name = paramValue;
          }
        }
        else if(paramName.endsWith("_Left"))
        {
          if(myWebPublisher->aDisplays.getItem(objID.toInt())->leftInfo != paramValue) 
          {
            String topic = "/display/" + objID + "/leftInfo";
            myWebPublisher->publishMsg(topic, paramValue, true);
            myWebPublisher->aDisplays.getItem(objID.toInt())->leftInfo = paramValue;
          }
        }
        if(paramName.endsWith("_Right"))
        {
          if(myWebPublisher->aDisplays.getItem(objID.toInt())->rightInfo != paramValue) 
          {
            String topic = "/display/" + objID + "/rightInfo";
            myWebPublisher->publishMsg(topic, paramValue, true);
            myWebPublisher->aDisplays.getItem(objID.toInt())->rightInfo = paramValue;
          }
        }
        if(paramName.endsWith("_Layout"))
        {
          if(myWebPublisher->aDisplays.getItem(objID.toInt())->layout != paramValue.toInt()) 
          {
            String topic = "/display/" + objID + "/layout";
            myWebPublisher->publishMsg(topic, paramValue, true);
            myWebPublisher->aDisplays.getItem(objID.toInt())->layout = paramValue.toInt();
          }
        }
      }
    }
    //request->send_P(200, "text/html", getIndexHTML().c_str());
    request->redirect("/");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String sIndexPage = mySettings->getSettingsHtml();
    request->send_P(200, "text/html", sIndexPage.c_str());
  });

  server.on("/updateSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    //Publish rules
    mySettings->setWifiLogin(request->getParam("wifiSSID", true)->value(), request->getParam("wifiPWD", true)->value());
    mySettings->setmqttServer(request->getParam("mqttServer", true)->value(), request->getParam("mqttPort", true)->value().toInt(),
    request->getParam("mqttLogin", true)->value(), request->getParam("mqttPWD", true)->value());
    mySettings->setOTA(request->getParam("otaPWD", true)->value());
    mySettings->saveSettings();
    request->send_P(200, "text/html", "Restarting...");
    delay(2000);
    request->redirect("/");
    delay(2000);
    ESP.restart();
  });

  server.begin();
}
