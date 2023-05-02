 /*
 * Library to gather all environmental data like real time and temperature
 * TODO:
 *  - Add a location search engine from https://locationiq.com/ for web server
 *  - Store all parameters in mqtt messages
 *  - Get sunrise ans sunset time
 * (c) Cedric Millard 2020
 */

#ifndef WeatherService_h
#define WeatherService_h

#include <MyMQTTClient.h>
#include <Environment.h>
#include <Logging.h>

class WeatherService: public MyMQTTClient
{
  public:
    WeatherService(PubSubClient *iMqttClientEnv, short iId);
    void setWeatherLocation(float iLat, float iLong);
    bool update(bool iForce = false);
    void handleMqttCallback(char* topic, byte* payload, unsigned int length){}
    void setEnv(Environment *iEnv) {pEnv = iEnv;}
    void setLog(Logging *iLog) {pLog = iLog;}

  protected:
    bool publishParams();
  private:
    bool updateWeather();
    unsigned long lastUpdatedWeather = 0;
    float weatherLat = 59.449062;
    float weatherLong = 16.332060;
    int Id;
    Environment *pEnv = NULL;
    Logging *pLog = NULL;
    float lastTempFromWeather = -255;
};

#endif
