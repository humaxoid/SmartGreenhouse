#include "time.h"

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800; // GMT+3
const int daylightOffset_sec = 3600;

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Не удалось получить время");
    return;
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println(&timeinfo, "%H:%M:%S");
}

void setup_1()
{
  // Инициализация и получение времени
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

// void loopntp()
// {
//   delay(1000);
// printLocalTime(); // Вывод реального времени
// }