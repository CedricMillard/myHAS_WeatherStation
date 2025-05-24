#ifndef ConfigWeatherStation_h
#define ConfigWeatherStation_h

//Comment for not having serial trace (prod)
//#define _DEBUG_

//Uncomment to broadcast weather forecast from the web
//#define WEATHER_SERVICE 

//Frequency to update the real time 
#define WEATHER_UPDATE_FREQ 28800 //s = 8H


#define OTA_NAME "WeatherStation"

#define PRISE1_ID 10010
#define PRISE2_ID 10011
#define VCC_ID 10012
#define TEMP_ID 10013
#define NEXA_CODE 64374781
#define TX_PIN 26
#define TEMP_PIN 27
//#define ADDRESS_SENSOR_VCC 480
#define ADDRESS_SENSOR_TEMP 480

#define WEATHER_LOCATION "Kvicksund"
#define WEATHER_LAT 59.449062
#define WEATHER_LONG 16.332060

//WEATHER
//#define WEATHER_URI "https://api.darksky.net/forecast/#KEY/#LAT,#LONG?#OPTIONS"
//#define WEATHER_KEY "751c1dc1b7b46ffa19ba9041a818f53e"
//#define WEATHER_OPTIONS "exclude=currently,minutely,alerts,flag&units=ca&lang=fr"
#define WEATHER_URI "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=#LAT&lon=#LONG"

#define W_SUN 1
#define W_PARTCLOUD 2
#define W_CLOUD 3
#define W_RAIN 4
#define W_SLEET 5
#define W_SNOW 6
#define W_WIND 7
#define W_FOG 8
#define W_THUNDER 9

#endif
