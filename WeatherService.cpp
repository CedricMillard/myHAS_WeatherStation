 /*
 * Library to gather all environmental data like real time and temperature
 * (c) Cedric Millard 2020
 */

#include "WeatherService.h"
#include "Config_WeatherStation.h"
#include <Config.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherService::WeatherService(PubSubClient *iMqttClient, short iId) : MyMQTTClient(iMqttClient, String(iId) + "_Weather")
{
  Id = iId;
}

bool WeatherService::publishParams()
{
#ifdef _DEBUG_
  Serial.println("WeatherService::PublishParams");
#endif
  bool bResult = true;
  String pubTopic = "/net/sensor/" + String(Id) + "/type";
  bResult &= publishMsg(pubTopic, "3", true);
  
  pubTopic = "/net/sensor/" + String(Id) + "/name";
  bResult &= publishMsg(pubTopic, "T_night", true);

  pubTopic = "/net/sensor/" + String(Id+9) + "/type";
  bResult &= publishMsg(pubTopic, "3", true);

  pubTopic = "/net/sensor/" + String(Id+9) + "/name";
  bResult &= publishMsg(pubTopic, "T_weather", true);

  return bResult;
}

void WeatherService::setWeatherLocation(float iLat, float iLong)
{
  weatherLat = iLat;
  weatherLong = iLong;
}

/*
*   https://api.darksky.net/forecast/751c1dc1b7b46ffa19ba9041a818f53e/59.449062,16.332060?exclude=currently,minutely,alerts,flag&units=ca&lang=fr
*/
bool WeatherService::update(bool iForce)
{
  bool bResult = true;

  MyMQTTClient::update(iForce);
  
  float currTempFromWeather = pEnv->getTemperatureExtFromWeather();
  if(lastTempFromWeather!=currTempFromWeather)
  {
    String sTopic = "/sensor/"+String(Id+9)+"/value";
    publishMsg(sTopic, String(currTempFromWeather), true); 
    lastTempFromWeather = currTempFromWeather;
  }

  if(lastUpdatedWeather==0 || (unsigned long)(millis() - lastUpdatedWeather)> WEATHER_UPDATE_FREQ*1000 || iForce )
  {
#ifdef _DEBUG_  
    Serial.println("Update Weather");
#endif
    HTTPClient http;
    String weatherURI = WEATHER_URI;
    weatherURI.replace("#KEY", WEATHER_KEY);
    weatherURI.replace("#OPTIONS", WEATHER_OPTIONS);
    weatherURI.replace("#LAT", String(weatherLat,6));
    weatherURI.replace("#LONG", String(weatherLong,6));
#ifdef _DEBUG_  
    Serial.println(weatherURI);
#endif    
    http.begin(weatherURI);
      
    int httpCode = http.GET();
    if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) 
      {
        String json = http.getString();

        //const size_t capacity = JSON_ARRAY_SIZE(5) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(18) + 6*JSON_OBJECT_SIZE(39) + 2*JSON_OBJECT_SIZE(40) + 6240;
        const size_t capacity = JSON_ARRAY_SIZE(5) + JSON_ARRAY_SIZE(8) + JSON_ARRAY_SIZE(49) + 3*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(8) + 43*JSON_OBJECT_SIZE(17) + 7*JSON_OBJECT_SIZE(18) + JSON_OBJECT_SIZE(38) + 7*JSON_OBJECT_SIZE(39)+5000;
        DynamicJsonDocument doc(capacity);
        StaticJsonDocument<1024> filter;
        filter["daily"]["data"][0]["icon"] = true;
        filter["daily"]["data"][0]["temperatureHigh"] = true;
        filter["daily"]["data"][0]["temperatureLow"] = true;
        filter["daily"]["data"][0]["windSpeed"] = true;
        filter["daily"]["data"][0]["moonPhase"] = true;
        filter["daily"]["data"][0]["sunriseTime"] = true;
        filter["daily"]["data"][0]["sunsetTime"] = true;
        
        filter["hourly"]["data"][0]["icon"] = true;
        filter["hourly"]["data"][0]["temperature"] = true;
        filter["hourly"]["data"][0]["windSpeed"] = true;
        filter["hourly"]["data"][0]["time"] = true;
        filter["hourly"]["icon"] = true;

        
        const size_t capacity2 = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(2) + 4*JSON_OBJECT_SIZE(5) + 160;
        DynamicJsonDocument docToRepublish(capacity2);
        JsonArray new_daily_array = docToRepublish.createNestedArray("daily");
        
        const size_t capacity3 = JSON_ARRAY_SIZE(14) + JSON_OBJECT_SIZE(2) + 14*JSON_OBJECT_SIZE(5) + 512;
        DynamicJsonDocument docToRepublish2(capacity3);
        JsonArray new_hourly_array = docToRepublish2.createNestedArray("hourly");

        deserializeJson(doc, json, DeserializationOption::Filter(filter));
        
        JsonObject daily = doc["daily"];
        JsonObject hourly = doc["hourly"];
        JsonArray daily_data_array = daily["data"];
        for(int i=0;i<4;i++)
        {
          JsonObject daily_data = daily_data_array[i];
          JsonObject currentDay = new_daily_array.createNestedObject();
          
          int iPayload = 0;
          String icon = daily_data["icon"];
          if(i==0) {
            String icon2 = hourly["icon"];
            icon = icon2;
          }

          if(icon=="clear-day" || icon=="clear-night") iPayload = W_SUN;
          else if(icon=="rain") iPayload = W_RAIN;
          else if(icon=="snow") iPayload = W_SNOW;
          else if(icon=="sleet") iPayload = W_SLEET;
          else if(icon=="wind") iPayload = W_WIND;
          else if(icon=="fog") iPayload = W_FOG;
          else if(icon=="cloudy") iPayload = W_CLOUD;
          else if(icon=="partly-cloudy-day" || icon=="partly-cloudy-night") iPayload = W_PARTCLOUD;
          else if(icon=="thunderstorm") iPayload = W_THUNDER;
          else iPayload=0;
          currentDay["weather"] = iPayload;

          currentDay["Tmax"]= (float)daily_data["temperatureHigh"];
          float Tm = (float)daily_data["temperatureLow"];
          currentDay["Tmin"]= Tm;
          if(i==0)
          {
            String sTopic = "/sensor/"+String(Id)+"/value";
            publishMsg(sTopic, String(Tm), true); 
          }
          currentDay["wind"]= (float)daily_data["windSpeed"];
          currentDay["moon"]= (float)daily_data["moonPhase"];
          currentDay["sunrise"]= daily_data["sunriseTime"];
          currentDay["sunset"]= daily_data["sunsetTime"];
        }
        
        //Publish same data to MQTT
        docToRepublish["updated"] = (long)time(nullptr);
        
        JsonArray hourly_data_array = hourly["data"];
        for(int i=0;i<14;i++)
        {
          JsonObject hourly_data = hourly_data_array[i];
          JsonObject currentHour = new_hourly_array.createNestedObject();
          
          currentHour["time"]= hourly_data["time"];

          int iPayload = 0;
          String icon = hourly_data["icon"];
          if(icon=="clear-day" || icon=="clear-night") iPayload = W_SUN;
          else if(icon=="rain") iPayload = W_RAIN;
          else if(icon=="snow") iPayload = W_SNOW;
          else if(icon=="sleet") iPayload = W_SLEET;
          else if(icon=="wind") iPayload = W_WIND;
          else if(icon=="fog") iPayload = W_FOG;
          else if(icon=="cloudy") iPayload = W_CLOUD;
          else if(icon=="partly-cloudy-day" || icon=="partly-cloudy-night") iPayload = W_PARTCLOUD;
          else if(icon=="thunderstorm") iPayload = W_THUNDER;
          else iPayload=0;
          currentHour["weather"] = iPayload;

          currentHour["Temp"]= (float)hourly_data["temperature"];
          
          currentHour["wind"]= (float)hourly_data["windSpeed"];
        }

        String output_d;
        serializeJson(docToRepublish, output_d);
#ifdef _DEBUG_  
        Serial.printf("Serialized weather daily: %s\n", output_d.c_str());
#endif            
        if(pEnv) pEnv->setWeatherDaily(output_d); 
        publishMsg(WEATHER_TOPIC_DAILY, output_d.c_str(), true); 

        String output_h;
        serializeJson(docToRepublish2, output_h);
#ifdef _DEBUG_  
        Serial.printf("Serialized weather hourly: %s\n", output_h.c_str());
#endif            
        if(pEnv) pEnv->setWeatherHourly(output_h); 
        publishMsg(WEATHER_TOPIC_HOURLY, output_h.c_str(), true); 
      }
      else {
        bResult = false;
        pLog->addLogEntry("ERROR Weather Update failed, [HTTPS] GET... not OK code: " + http.errorToString(httpCode));
#ifdef _DEBUG_  
        Serial.printf("[HTTPS] GET... not OK code: %s\n", http.errorToString(httpCode).c_str());
#endif          
       }
    }
    else {
      bResult = false;
      pLog->addLogEntry("ERROR Weather Update failed, [HTTPS] GET... failed, error:" + http.errorToString(httpCode));
#ifdef _DEBUG_  
      Serial.println("Weather Update failed 2");
            Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        Serial.println(httpCode);
#endif              
    }
    http.end();
    lastUpdatedWeather = millis();
    pLog->addLogEntry("Weather updated successfully");
  }
}
