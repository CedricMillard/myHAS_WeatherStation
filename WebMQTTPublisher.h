 /*
 * Library to interface web page and MQTT server. Send message to server and keep list of device up-to-date
 * TODO:
 * (c) Cedric Millard 2020
 */

#ifndef WebMQTTPublisher_h
#define WebMQTTPublisher_h

#include <ArrayCed.h>
#include <MyMQTTClient.h>
#include <ConnectedObjects.h>
#include <WebPage.h>
#include <Logging.h>

class WebMQTTPublisher: public MyMQTTClient, public WebPage
{
  public:
    WebMQTTPublisher(PubSubClient *iMqttClientEnv, bool *iNeedRefresh, short iId);
    void handleMqttCallback(char* iTopic, byte* payload, unsigned int iLength);
    bool update(bool iForce = false);
    void dump();

   bool *needRefresh;
   long lastUpdateTime = 0;
};

#endif
