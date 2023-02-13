 /*
 * Library to manage display of weather on ePaper
 * TODO:
 * (c) Cedric Millard 2020
 */

#ifndef WeatherDisplay_h
#define WeatherDisplay_h

#include <Environment.h>
#include <ConnectedObjects.h>
#include <ArrayCed.h>


#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

enum layoutType
{
  none,

  bigTime,
  dateOnly,
  dateTime,

  todayWeather,
  DailyWeather1_2,
  HourlyWeather,

  LInfo,
  RInfo,

  HLine,
  VLine
};

//WeatherStation(short iId, int iEepromStartAddr = 0) : Objet(iId, objType::display){eepromStartAddr = iEepromStartAddr;}
struct layoutElement
{
  layoutType type = none;
  int x;
  int y;
};

class WeatherDisplay : public GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>, public WeatherDisplayObj, public MyMQTTClient
{
  public:
    WeatherDisplay(PubSubClient *iMqttClient, short iId, Environment *iEnv, int iEepromStartAddr = 0, bool iPartialUpdate = true);
    bool update(bool iForce = false);
    void handleMqttCallback(char* iTopic, byte* payload, unsigned int iLength);
    void init();  
  protected:
    bool publishParams();
    
    void drawFrame(uint16_t iColor);
    
    void updateDate();
    void drawDate(layoutType iType, int iX, int iY);
    
    void updateTime();
    void drawTime(int iX, int iY);

    void updateWeatherForecast(int iDay);
    void drawWeatherForecast(int iDay, int iX, int iY, uint16_t iColor = GxEPD_BLACK);
    
    void updateTimeForecast(int iPoz);
    void drawTimeForecast(int iPoz, int iX, int iY, uint16_t iColor = GxEPD_BLACK);
    long getTimeFromPoz(int iPoz);

    void updateTodayWeather();
    void drawTodayWeather(int iX, int iY, uint16_t iColor = GxEPD_BLACK);    

    void updateMoon();
    void drawMoon(uint16_t iColor);

    void updateRInfo();
    void drawRInfo(int iX, int iY, uint16_t iColor = GxEPD_BLACK);

    void updateLInfo();
    void drawLInfo(int iX, int iY, uint16_t iColor = GxEPD_BLACK);

    void drawWind(uint16_t iX, uint16_t  iY, uint16_t iColor, float iWind, bool iBig = false);

    void refreshFullScreen();

    void initLayout1();
    void initLayout2();
    void initLayout3();

    Environment *pEnv = NULL;

    String lastDate;
    String lastTime;
    Weather lastWeather_daily[4];
    Weather lastWeather_hourly[4];
    String lastMoon;
    String lastRInfo;
    String lastLInfo;
    
    bool bInverted = false;
    bool partialUpdate = true;

    Array<layoutElement> aLines;
       
    layoutElement lTime;
    layoutElement lDate;
    layoutElement lTodayW;
    layoutElement lDailyW;
    layoutElement lHourlyW;
    layoutElement lLInfo;
    layoutElement lRInfo;
};

//String roundTemp(float iTemp, bool iStrict = true);

String roundTemp(float iTemp, short iNbDigit = 2, bool iStrict = true);
#endif
