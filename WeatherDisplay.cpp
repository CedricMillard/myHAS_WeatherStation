 /*
 * Library to gather all environmental data like real time and temperature
 * (c) Cedric Millard 2020
 */

#include "WeatherDisplay.h"
#include "WeatherIcons.h"
#include "Config_WeatherStation.h"

#include <Fonts/FreeSansBold18pt7bCed.h>
#include <Fonts/FreeSans12pt7bCed.h>
#include <Fonts/FreeMonoBold9pt7bCed.h>

WeatherDisplay::WeatherDisplay(PubSubClient *iMqttClient, short iId, Environment *iEnv, int iEepromStartAddr, bool iPartialUpdate):GxEPD2_BW(GxEPD2_154_D67(/*CS=5*/ 15, /*DC=*/ 33, /*RST=*/ 14, /*BUSY=*/ 32)), WeatherDisplayObj(iId, iEepromStartAddr), MyMQTTClient(iMqttClient, String(iId)+"_display") 
{
  pEnv = iEnv;
  partialUpdate = iPartialUpdate;
//  display = new GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>(GxEPD2_154_D67(/*CS=5*/ 15, /*DC=*/ 33, /*RST=*/ 27, /*BUSY=*/ 32)); // GDEH0154D67
  GxEPD2_BW::init(115200);
  mirror(false);
  topicList.add("/display/"+String(Id)+"/leftInfo"); 
  topicList.add("/display/"+String(Id)+"/rightInfo"); 
  topicList.add("/display/"+String(Id)+"/name"); 
  topicList.add("/display/"+String(Id)+"/layout");
}

void WeatherDisplay::handleMqttCallback(char* iTopic, byte* payload, unsigned int iLength)
{
#ifdef _DEBUG_
  Serial.print("Message arrived [");
  Serial.print(iTopic);
  Serial.print("] ");
  for (int i = 0; i < iLength; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif
  String sTopic = iTopic;
  if(sTopic.endsWith("/name"))
  {
    char *tempString = (char *) malloc(sizeof(char)*(iLength+1));
    memcpy(tempString, payload, iLength);
    tempString[iLength] = '\0';
    String sTmpName (tempString);
    free(tempString);
    if(name!=sTmpName)
    {
      if(sTmpName.length()==0) name = String(Id);
      else name = sTmpName;
      String pubTopic = "/net/display/" + String(Id) + "/name";
      publishMsg(pubTopic, name, true);
    }
  }
  if(sTopic.endsWith("/leftInfo"))
  {
    char *tempString = (char *) malloc(sizeof(char)*(iLength+1));
    memcpy(tempString, payload, iLength);
    tempString[iLength] = '\0';
    String sTmpInfo (tempString);
    free(tempString);
    if(leftInfo!=sTmpInfo)
    {
      leftInfo = sTmpInfo;
      String pubTopic = "/net/display/" + String(Id) + "/leftInfo";
      publishMsg(pubTopic, leftInfo, true);
      updateLInfo();
      //refreshFullScreen();
    }
  }
  if(sTopic.endsWith("/rightInfo"))
  {
    char *tempString = (char *) malloc(sizeof(char)*(iLength+1));
    memcpy(tempString, payload, iLength);
    tempString[iLength] = '\0';
    String sTmpInfo (tempString);
    free(tempString);
    if(rightInfo!=sTmpInfo)
    {
      rightInfo = sTmpInfo;
      String pubTopic = "/net/display/" + String(Id) + "/rightInfo";
      publishMsg(pubTopic, rightInfo, true);
      updateRInfo();
    }
  }
  if(sTopic.endsWith("/layout"))
  {
    char *tempString = (char *) malloc(sizeof(char)*(iLength+1));
    memcpy(tempString, payload, iLength);
    tempString[iLength] = '\0';
    String sTmpLayout (tempString);
    free(tempString);
    int newLayout = sTmpLayout.toInt();
    if(newLayout!=layout)
    {
      layout = newLayout;
      if(layout==1) initLayout1();
      else if(layout==2) initLayout2();
      else if(layout==3) initLayout3();
      String pubTopic = "/net/display/" + String(Id) + "/layout";
      publishMsg(pubTopic, String(layout), true);
      refreshFullScreen();
    }
  }
  saveInEeprom();
}

void WeatherDisplay::init()
{
  WeatherDisplayObj::init();
  if(layout==1) initLayout1();
  else if(layout==2) initLayout2();
  else if(layout==3) initLayout3();
}

bool WeatherDisplay::publishParams()
{
#ifdef _DEBUG_
  Serial.println("WeatherDisplay::PublishParams");
#endif  
  bool bResult = true;
  String pubTopic = "/net/display/" + String(Id) + "/name";
  bResult &= publishMsg(pubTopic, name, true);

  pubTopic = "/net/display/" + String(Id) + "/leftInfo";
  bResult &= publishMsg(pubTopic, leftInfo, true);

  pubTopic = "/net/display/" + String(Id) + "/rightInfo";
  bResult &= publishMsg(pubTopic, rightInfo, true);

  return bResult;
}

bool WeatherDisplay::update(bool iForce)
{
  MyMQTTClient::update(iForce);
  long deltaSunrise = difftime(time(nullptr), pEnv->getSunriseTime());
  long deltaSunset = difftime(time(nullptr), pEnv->getSunsetTime());
  bool bfullRefresh = false;
  if( (bInverted && deltaSunrise>0 && deltaSunset<0) || (!bInverted && deltaSunset>0) || (!bInverted && deltaSunrise<0) )
  {
    bInverted = !bInverted ;
    bfullRefresh = true;
  }
    
  int heure = getTimeFr().substring(0,2).toInt();
  if(lastTime.length()==0 || bfullRefresh || iForce)
  {
    lastTime = getTimeFr();
    refreshFullScreen();
  }
  else if(partialUpdate)
  {
    if(lastTime!=getTimeFr())
    {
#ifdef _DEBUG_  
      Serial.println("Refresh Time");
#endif
      lastTime = getTimeFr();
      if(lTime.type==bigTime)
      {
        updateTime();

        if(lastDate!=getDateFr())
        {
    #ifdef _DEBUG_  
          Serial.println("Refresh Date");
    #endif
          lastDate = getDateFr();
          updateDate();
        }
      }
      else
        updateDate();
      
      for(int i = 0; i<4; i++)
      {
        if(lastWeather_daily[i]!=pEnv->getWeatherDay(i))
        {
  #ifdef _DEBUG_  
        Serial.print("Refresh Daily Weather ");
  #endif        
          lastWeather_daily[i] = pEnv->getWeatherDay(i);
  #ifdef _DEBUG_  
        Serial.println(i);
  #endif           
          if(i==0) updateTodayWeather();
          else if(i==3)
          {
            if(leftInfo=="Weather3") updateLInfo();
            if(rightInfo=="Weather3") updateRInfo();
          }
          else if(i<3) updateWeatherForecast(i);
        }
      }

      if(lHourlyW.type==HourlyWeather)
      {
        for(int i = 0; i<4; i++)
        {
          long newTime = getTimeFromPoz(i+1);
          if(lastWeather_hourly[i]!=pEnv->getWeatherHour(newTime))
          {
    #ifdef _DEBUG_  
          Serial.print("Refresh hourly Weather ");
    #endif        
            lastWeather_hourly[i] = pEnv->getWeatherDay(newTime);
    #ifdef _DEBUG_  
          Serial.println(i);
    #endif           
            updateTimeForecast(i+1);
          }
        }
      }
  
      if(rightInfo!="Weather3" && rightInfo.length()>0)
      {
        String sRInfo = roundTemp(pEnv->getSensorValue(rightInfo.toInt()), 3, false)+pEnv->getSensorUnit(rightInfo.toInt());
        sRInfo.replace("°", "*");
        if(lastRInfo!=sRInfo)
        {
  #ifdef _DEBUG_  
          Serial.println("Refresh RInfo ");
  #endif
          lastRInfo = sRInfo;
          updateRInfo();
        }
      }
  
      if(leftInfo!="Weather3" && leftInfo.length()>0)
      {
        String sLInfo = roundTemp(pEnv->getSensorValue(leftInfo.toInt()), 3, false)+pEnv->getSensorUnit(leftInfo.toInt());
        sLInfo.replace("°", "*");
        if(lastLInfo!=sLInfo)
        {
  #ifdef _DEBUG_  
          Serial.println("Refresh LInfo ");
  #endif
          lastLInfo = sLInfo;
          updateLInfo();
        }
      }
    powerOff();
    }
  }

  else if(lastTime!=getTimeFr())
  {
#ifdef _DEBUG_  
    Serial.println("Refresh Time");
#endif
    lastTime = getTimeFr();
    refreshFullScreen();
  }
}

void WeatherDisplay::updateDate()
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(0, 0, 200, 20);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawDate(lDate.type, lDate.x, lDate.y);
  }
  while (nextPage());
}

void WeatherDisplay::drawDate(layoutType iType, int iX, int iY)
{
  setFont(&FreeSans12pt7b);
  setTextSize(1);
  setCursor(iX+1, iY+17);
  
  if(iType==dateOnly) print(getDateFr());
  else {
    print(getDateShort());
    int16_t tbx, tby; uint16_t tbw, tbh; // boundary box window
    //Time aligned center
    getTextBounds(getTimeFr(), 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    setCursor(iX+((200 - tbw) / 2) - tbx, iY+17);
    print(getTimeFr());
  }
  int16_t tbx, tby; uint16_t tbw, tbh; // boundary box window
  getTextBounds(getWeekNumber(), 0, 0, &tbx, &tby, &tbw, &tbh);
  setCursor(iX + 199-tbw, iY+17);
  print(getWeekNumber());
}

void WeatherDisplay::updateTime()
{
  if(lTime.type == none) return;

  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(lTime.x, lTime.y, 200, 60);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawTime(lTime.x, lTime.y);
  }
  while (nextPage());
}

void WeatherDisplay::drawTime(int iX, int iY)
{
  String sTime = getTimeFr();
  setFont(&FreeSansBold18pt7b);
  setTextSize(2);
  int16_t tbx, tby; uint16_t tbw, tbh;
  getTextBounds(sTime, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  setCursor(iX + ((200 - tbw) / 2) - tbx, iY + 53);
  print(sTime);
  setTextSize(1);
}

void WeatherDisplay::updateWeatherForecast(int iDay)
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(lDailyW.x, lDailyW.y + (iDay-1)*40, 100, 40);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawWeatherForecast(iDay, lDailyW.x, lDailyW.y + (iDay-1)*40, fcolor);
  }
  while (nextPage());
}

void WeatherDisplay::drawWeatherForecast(int iDay, int iX, int iY, uint16_t iColor)
{
  int16_t tbx, tby; uint16_t tbw, tbh;
  Weather weather = pEnv->getWeatherDay(iDay);

  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  rawtime += iDay*86400;
  timeinfo = localtime (&rawtime);
  char sTime[5];
  strftime(sTime, 5, "%a", timeinfo);
  
  setFont(NULL);
  setTextSize(1);
  setCursor(iX+1, iY + 31);
  print(sTime);

  if(weather.Weather>0)
  {
    setFont(&FreeMonoBold9pt7b);
    String Tmax = roundTemp(weather.Tmax);
    getTextBounds(Tmax, 0, 0, &tbx, &tby, &tbw, &tbh);
    setCursor(iX+96-tbw, iY + 15);
    print(Tmax);
  
    String Tmin = roundTemp(weather.Tmin);
    getTextBounds(Tmin, 0, 0, &tbx, &tby, &tbw, &tbh);
    setCursor(iX+96-tbw, iY + 35);
    print(Tmin);

    drawWind(iX+2, iY + 2, iColor, weather.Wind);
    
    drawInvertedBitmap(iX+25, iY + 2, weatherIcons35x35[weather.Weather], 35, 35, iColor);
  }
  drawFrame(iColor);
}

void WeatherDisplay::updateTodayWeather()
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(lTodayW.x, lTodayW.y, 100, 80);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawTodayWeather(lTodayW.x, lTodayW.y, fcolor);
  }
  while (nextPage());
}

void WeatherDisplay::drawTodayWeather(int iX, int iY, uint16_t iColor)
{
  int16_t tbx, tby; uint16_t tbw, tbh;
  setFont(&FreeSans12pt7b);
  Weather weather = pEnv->getWeatherDay(0);
  if(weather.Weather>0)
  {
    setCursor(iX+5, iY+75);
    String Tmax = roundTemp(weather.Tmax)+"*";
    print(Tmax);
    String Tmin = roundTemp(weather.Tmin)+"*";
    getTextBounds(Tmin, 0, 0, &tbx, &tby, &tbw, &tbh);
    setCursor(iX+95-tbw, iY+75);
    print(Tmin);

    drawWind(iX+4, iY+4, iColor, weather.Wind, true);
    
    drawInvertedBitmap(iX+25, iY+5, weatherIcons50x50[weather.Weather], 50, 50, iColor);
  }
  drawFrame(iColor);
}

long WeatherDisplay::getTimeFromPoz(int iPoz)
{
  long currentTime = time(nullptr);

  long roundedTime = (long)(currentTime/3600)*3600;
  int timeShift = 0;
  switch(iPoz)
  {
    case 1: timeShift = 2; break;
    case 2: timeShift = 4; break;
    case 3: timeShift = 7; break;
    case 4: timeShift = 10; break;
    default: break;
  }

  roundedTime += timeShift*3600;

  return roundedTime;
}

void WeatherDisplay::updateTimeForecast(int iPoz)
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(lHourlyW.x + 50*(iPoz-1), lHourlyW.y, 50, 60);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawTimeForecast(iPoz, lHourlyW.x + 50*(iPoz-1), lHourlyW.y, fcolor);
  }
  while (nextPage());
}

void WeatherDisplay::drawTimeForecast(int iPoz, int iX, int iY, uint16_t iColor)
{
  int16_t tbx, tby; uint16_t tbw, tbh;

  long lTime = getTimeFromPoz(iPoz);

  Weather weather = pEnv->getWeatherHour(lTime);
  setFont(NULL);
  setTextSize(1);
  
  long currTime =  getTimeSec();
  long deltaTime = difftime(lTime, time(nullptr));
  long dispTime = (currTime + deltaTime)%86400;
  String sTime = String((int)(dispTime/3600))+"H";
  getTextBounds(sTime, 0, 0, &tbx, &tby, &tbw, &tbh);
  setCursor(iX+45-tbw, iY+3);
  print(sTime);

  if(weather.Weather>0)
  {
    setFont(&FreeMonoBold9pt7b);
    String Tmax = roundTemp(weather.Tmax)+"*";
    getTextBounds(Tmax, 0, 0, &tbx, &tby, &tbw, &tbh);
    //setCursor(96-tbw, 80 + (iDay-1)*40 + 15);
    setCursor(iX+25-tbw/2, iY+57);
    print(Tmax);
  
    drawInvertedBitmap(iX+7, iY+7, weatherIcons35x35[weather.Weather], 35, 35, iColor);
  }
  
  drawFrame(iColor);
}

void WeatherDisplay::drawMoon(uint16_t iColor)
{
  uint16_t fColor = iColor;
  uint16_t bColor = GxEPD_WHITE;
  if(fColor == GxEPD_WHITE) bColor = GxEPD_BLACK;
  
  fillCircle(185, 182, 10, fColor);
  fillCircle(175, 182, 10, bColor);
  drawCircle(185, 182, 10, fColor);
}


void WeatherDisplay::updateRInfo()
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  
  setRotation(2);
  setTextColor(fcolor);
  setPartialWindow(lRInfo.x, lRInfo.y, 100, 40);
  firstPage();
  do
  {
    fillScreen(bcolor);
    drawRInfo(lRInfo.x, lRInfo.y, fcolor);
  }
  while (nextPage());
}

void WeatherDisplay::drawRInfo(int iX, int iY, uint16_t iColor)
{
  if(rightInfo=="Weather3") drawWeatherForecast(3, iX, iY, iColor);
  else if(rightInfo.toInt()==0) return;
  else
  {
    float rInfo = pEnv->getSensorValue(rightInfo.toInt());
      
    if(rInfo>-100)
    {
      int16_t tbx, tby; uint16_t tbw, tbh;
      setFont(&FreeSansBold18pt7b);
      
      String Text = roundTemp(rInfo, 3, false)+pEnv->getSensorUnit(rightInfo.toInt()).substring(0,1);
      getTextBounds(Text, 0, 0, &tbx, &tby, &tbw, &tbh);
      setCursor(iX+(100-tbw)/2, iY+35);
      print(Text);
    }
    drawFrame(iColor);
  }
}

void WeatherDisplay::updateLInfo()
{
    uint16_t fcolor = GxEPD_BLACK;
    uint16_t bcolor = GxEPD_WHITE;
    if(bInverted)
    {
      fcolor = GxEPD_WHITE;
      bcolor = GxEPD_BLACK;
    }
    
    setRotation(2);
    setTextColor(fcolor);
    setPartialWindow(lLInfo.x, lLInfo.y, 96, 40);
    firstPage();
    do
    {
      fillScreen(bcolor);
      drawLInfo(lLInfo.x, lLInfo.y, fcolor);
    }
    while (nextPage());
}

void WeatherDisplay::drawLInfo(int iX, int iY, uint16_t iColor)
{
  if(leftInfo=="Weather3") drawWeatherForecast(3, iX, iY, iColor);
  else if(leftInfo.toInt()==0) return;
  else
  {
    float lInfo = pEnv->getSensorValue(leftInfo.toInt());
      
    if(lInfo>-100)
    {
      int16_t tbx, tby; uint16_t tbw, tbh;
      setFont(&FreeSansBold18pt7b);
      
      String Text = roundTemp(lInfo, 3, false)+pEnv->getSensorUnit(leftInfo.toInt()).substring(0,1);
      getTextBounds(Text, 0, 0, &tbx, &tby, &tbw, &tbh);
      setCursor(iX + (100-tbw)/2, iY + 35);
      print(Text);
    }
    drawFrame(iColor);
  }
}

void WeatherDisplay::drawFrame(uint16_t iColor)
{

  for(int i = 0; i<aLines.size();i++)
  {
    if(aLines[i].type==HLine)
    {
      int midX = aLines[i].x;
      int deltaR = 200-midX;
      int startX;
      int length;
      if(midX<=deltaR)
      {
        startX = 0;
        length = 2*midX;
      }
      else
      {
        startX = midX-deltaR;
        length = 2*deltaR;
      }
      
      drawFastHLine(startX, aLines[i].y, length, iColor);
    }
    else if(aLines[i].type==VLine)
    {
      int midY = aLines[i].y;
      int deltaR = 200-midY;
      int startY;
      int length;
      if(midY<=deltaR)
      {
        startY = 0;
        length = 2*midY;
      }
      else
      {
        startY = midY-deltaR;
        length = 2*deltaR;
      }
      drawFastVLine(aLines[i].x, startY, length, iColor);
    }

    //Draw hourly forecast frame if needed
    if(lHourlyW.type==HourlyWeather)
    {
      drawFastVLine(lHourlyW.x + 50, lHourlyW.y, 60, iColor);
      drawFastVLine(lHourlyW.x + 150, lHourlyW.y, 60, iColor);
    }
  }


  /*drawFastHLine(0, 80, 200, iColor);
  drawFastVLine(100, 80, 120, iColor);
  
  drawFastHLine(0, 120, 100, iColor);
  drawFastHLine(0, 160, 200, iColor);*/
}

void WeatherDisplay::drawWind(uint16_t iX, uint16_t  iY, uint16_t iColor, float iWind, bool iBig)
{
  int iHeight = 4;
  int iSpace = 1;
  int iWidth = 6;
  if(iBig)
  {
    iHeight = 6;
    iSpace = 2;
    iWidth = 7;
  }
  
  //force 8
  if(iWind>=62)
    fillRect(iX+2*(int)(iWidth*4/3)-2, iY, (int)(iWidth*4/3), iHeight, iColor);
  else
    drawRect(iX+2*(int)(iWidth*4/3)-2, iY, (int)(iWidth*4/3), iHeight, iColor);
  
  //Force 7
  if(iWind>=51)
    fillRect(iX+(int)(iWidth*4/3)-1, iY, (int)(iWidth*4/3), iHeight, iColor);
  else
    drawRect(iX+(int)(iWidth*4/3)-1, iY, (int)(iWidth*4/3), iHeight, iColor);
  
  //Force 6
  if(iWind>=40)
    fillRect(iX, iY, (int)(iWidth*4/3), iHeight, iColor);
  else
    drawRect(iX, iY, (int)(iWidth*4/3), iHeight, iColor);

  //Force 5
  if(iWind>=31)
    fillRect(iX+(int)(iWidth*3/2)-1, iY + iHeight + iSpace, (int)(iWidth*3/2), iHeight, iColor);
  else
    drawRect(iX+(int)(iWidth*3/2)-1, iY + iHeight + iSpace, (int)(iWidth*3/2), iHeight, iColor);
  
  //Force 4
  if(iWind>=20)
    fillRect(iX, iY + iHeight + iSpace, (int)(iWidth*3/2), iHeight, iColor);
  else
    drawRect(iX, iY + iHeight + iSpace, (int)(iWidth*3/2), iHeight, iColor);

  //Force 3
  if(iWind>=13)
    fillRect(iX+iWidth-1, iY + 2*iHeight + 2*iSpace, iWidth, iHeight, iColor);
  else
    drawRect(iX+iWidth-1, iY + 2*iHeight + 2*iSpace, iWidth, iHeight, iColor);

  //force 2
  if(iWind>=7)
    fillRect(iX, iY + 2*iHeight + 2*iSpace, iWidth, iHeight, iColor);
  else
    drawRect(iX, iY + 2*iHeight + 2*iSpace, iWidth, iHeight, iColor);
  
  //Force 0/1
  if(iWind>=2)
    fillRect(iX, iY + 3*iHeight + 3*iSpace, iWidth, iHeight, iColor);
  else
    drawRect(iX, iY + 3*iHeight + 3*iSpace, iWidth, iHeight, iColor);
  
}

void WeatherDisplay::refreshFullScreen()
{
  uint16_t fcolor = GxEPD_BLACK;
  uint16_t bcolor = GxEPD_WHITE;
  if(bInverted)
  {
    fcolor = GxEPD_WHITE;
    bcolor = GxEPD_BLACK;
  }
  //Serial.println("helloWorld");
  setRotation(2);
  setTextColor(fcolor);
  setFullWindow();
  firstPage();
  do
  {
    fillScreen(bcolor);

    drawFrame(fcolor);
    
    drawDate(lDate.type, lDate.x, lDate.y);
    
    if(lTime.type==bigTime)
      drawTime(lTime.x, lTime.y);

    drawWeatherForecast(1, lDailyW.x, lDailyW.y, fcolor);
    drawWeatherForecast(2, lDailyW.x, lDailyW.y+40, fcolor);
    
    drawLInfo(lLInfo.x, lLInfo.y, fcolor);

    drawTodayWeather(lTodayW.x, lTodayW.y, fcolor);

    drawRInfo(lRInfo.x, lRInfo.y, fcolor);

    if(lHourlyW.type==HourlyWeather)
    {
      drawTimeForecast(1, lHourlyW.x, lHourlyW.y, fcolor);
      drawTimeForecast(2,lHourlyW.x+50, lHourlyW.y, fcolor);
      drawTimeForecast(3,lHourlyW.x+100, lHourlyW.y, fcolor);
      drawTimeForecast(4,lHourlyW.x+150, lHourlyW.y, fcolor);
    }
    
    //drawMoon(fcolor);
  }
  while (nextPage());
}


String roundTemp(float iTemp, short iNbDigit, bool iStrict)
{
  if(iTemp<0) iNbDigit--;
  if(abs(iTemp)>=10) iNbDigit--;
  //Remove one more digits to count for partie entiere except if we already removed all decimals 
  if(iNbDigit>0)iNbDigit--;
  //if(iNbDigit<0) return "Err";
  
  String out (round(iTemp*pow(10,iNbDigit))/pow(10,iNbDigit));
  //add one more to not consider the dot
  if(iNbDigit>0)iNbDigit++;
  out = out.substring(0, out.indexOf(".")+iNbDigit);
  /*if(iStrict && out.length()>3) 
  {
    out = String(round(iTemp));
    out = out.substring(0, out.indexOf("."));
  }*/
  return out;
}

void WeatherDisplay::initLayout1()
{
  layoutElement currElem;
  
  aLines.removeAll();

  lDate.type = dateOnly;
  lDate.x = 0;
  lDate.y = 0;
  
  lTime.type = bigTime;
  lTime.x = 0;
  lTime.y = 20;
  
  lTodayW.type = todayWeather;
  lTodayW.x = 100;
  lTodayW.y = 80;
  
  lDailyW.type = DailyWeather1_2;
  lDailyW.x = 0;
  lDailyW.y = 80;

  lHourlyW.type = none;
  lHourlyW.x = 0;
  lHourlyW.y = 0;
  
  lLInfo.type = LInfo;
  lLInfo.x = 0;
  lLInfo.y = 160;
  
  lRInfo.type = RInfo;
  lRInfo.x = 100;
  lRInfo.y = 160;
  
  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 80;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 50;
  currElem.y = 120;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 160;
  aLines.add(currElem);
  
  currElem.type = VLine;
  currElem.x = 100;
  currElem.y = 140;
  aLines.add(currElem);
}

void WeatherDisplay::initLayout2()
{
  layoutElement currElem;
  
  aLines.removeAll();

  lDate.type = dateOnly;
  lDate.x = 0;
  lDate.y = 0;
  
  lTime.type = bigTime;
  lTime.x = 0;
  lTime.y = 20;
  
  lTodayW.type = todayWeather;
  lTodayW.x = 0;
  lTodayW.y = 80;
  
  lDailyW.type = DailyWeather1_2;
  lDailyW.x = 100;
  lDailyW.y = 80;

  lHourlyW.type = none;
  lHourlyW.x = 0;
  lHourlyW.y = 0;
  
  lLInfo.type = LInfo;
  lLInfo.x = 0;
  lLInfo.y = 160;
  
  lRInfo.type = RInfo;
  lRInfo.x = 100;
  lRInfo.y = 160;
  
  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 80;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 150;
  currElem.y = 120;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 160;
  aLines.add(currElem);
  
  currElem.type = VLine;
  currElem.x = 100;
  currElem.y = 140;
  aLines.add(currElem);
}

void WeatherDisplay::initLayout3()
{
  layoutElement currElem;
  
  aLines.removeAll();

  lDate.type = dateTime;
  lDate.x = 0;
  lDate.y = 0;
  
  lTime.type = none;
  lTime.x = 0;
  lTime.y = 0;

  lTodayW.type = todayWeather;
  lTodayW.x = 0;
  lTodayW.y = 18;
  
  lDailyW.type = DailyWeather1_2;
  lDailyW.x = 100;
  lDailyW.y = 20;
  
  lHourlyW.type = HourlyWeather;
  lHourlyW.x = 0;
  lHourlyW.y = 100;
  
  lLInfo.type = LInfo;
  lLInfo.x = 0;
  lLInfo.y = 160;
  
  lRInfo.type = RInfo;
  lRInfo.x = 100;
  lRInfo.y = 160;
  
  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 20;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 150;
  currElem.y = 60;
  aLines.add(currElem);
  
  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 100;
  aLines.add(currElem);

  currElem.type = HLine;
  currElem.x = 100;
  currElem.y = 160;
  aLines.add(currElem);
  
  currElem.type = VLine;
  currElem.x = 100;
  currElem.y = 110;
  aLines.add(currElem);
}