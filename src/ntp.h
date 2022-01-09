#include <WiFi.h>
#include "time.h"

const char *ssid = "Satman_WLAN";
const char *password = "9uthfim8";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 12600;
const int daylightOffset_sec = 3600;

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Не удалось получить время");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup()
{
  Serial.begin(115200);

  // Подключение к WiFi
  Serial.printf("подключение к %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ПОДКЛЮЧИЛСЯ");

  // Инициализация и получение времени
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  // Отключение от Wi-Fi, так как он больше не нужен
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void loop()
{
  delay(1000);
  printLocalTime();
}