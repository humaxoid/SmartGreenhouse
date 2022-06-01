// Файл конфигурации датчиков
#include <Wire.h> // Библиотека взаимодействия с DS18B20, BH1750
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DS18B20.h>

// DTH22 - Датчик температуры и влажности
#define DHTPIN 13     //Указываем номер цифрового pin для чтения данных
#define DHTTYPE DHT22 //Указываем тип датчика
DHT dht(DHTPIN, DHTTYPE);

String getDHTTemperature()
{
  float IN1 = dht.readTemperature(); // Считывание температуры и создание переменной с именем IN1
  if (isnan(IN1))
  {
    return "--"; // Проверяем, не удалось ли выполнить какое-либо чтение, после чего выходим раньше (чтобы повторить попытку).
  }
  else
  {
    // Serial.print("DTH22- Температура: ");
    // Serial.print(IN1);                    // Вывод температуры в последовательный монитор
    // Serial.print(" °C ");                 // Вывод текста в монитор порта
    return String(IN1);
  }
}

String getDHTHumidity()
{
  uint8_t IN2 = dht.readHumidity(); // Считывание влажности и создание переменной с именем IN2
  if (isnan(IN2))
  { // Проверяем, не удалось ли выполнить какое-либо чтение, после чего выходим раньше (чтобы повторить попытку).
    // Serial.println("Ошибка чтения с датчика DTH22!");
    return "--";
  }
  else
  {
    // Serial.print("DTH22- Влажность: ");
    // Serial.print(IN2);
    // Serial.println(" % ");
    return String(IN2);
  }
}

 // BH1750 - Датчик света
 BH1750 lightMeter;
 String getLightLevel()
 {
   uint16_t IN3 = lightMeter.readLightLevel(); // Считывание данных и создание переменной с именем IN3
   // Serial.print("Освещенность: ");             // Вывод текста в монитор порта
   // Serial.print(IN3);                          // Вывод показаний в последовательный монитор порта
   // Serial.println(" люкс");
   return String(IN3);
 }

// ================= BME280 - Универсальный датчик ==============

Adafruit_BME280 bme; // Подключаем датчик в режиме I2C
unsigned long delayTime;

String getTemperature2()
{
  float IN4 = bme.readTemperature();
  return String(IN4);
}

String getPressure()
{
  uint16_t IN5 = bme.readPressure() / 133.3;
  return String(IN5);
}

String getHumidity()
{
  uint8_t IN6 = bme.readHumidity();
  return String(IN6);
}

// ================ DS18B20 - Датчик температуры почвы ============
OneWire ds(15); // Указываем номер пина.

String gettemperature3()
{
  byte data[2];   // Место для значения температуры
  ds.reset();     // Начинаем взаимодействие со сброса всех предыдущих команд и параметров
  ds.write(0xCC); // Даем датчику DS18b20 команду пропустить поиск по адресу. В нашем случае только одно устройство
  ds.write(0x44); // Даем датчику DS18b20 команду измерить температуру. Само значение температуры мы еще не получаем - датчик его положит во внутреннюю память
  delay(1000);    // Микросхема измеряет температуру, а мы ждем.
  ds.reset();     // Теперь готовимся получить значение измеренной температуры
  ds.write(0xCC);
  ds.write(0xBE); // Просим передать нам значение регистров со значением температуры

  // Получаем и считываем ответ
  data[0] = ds.read(); // Читаем младший байт значения температуры
  data[1] = ds.read(); // А теперь старший

  // Формируем итоговое значение:
  // - сперва "склеиваем" значение,
  // - затем умножаем его на коэффициент, соответствующий разрешающей способности (для 12 бит по умолчанию - это 0,0625)
  float temperature = ((data[1] << 8) | data[0]) * 0.0625;

  uint8_t IN7 = temperature; // Считывание данных и создание переменной с именем IN7, поправочный коэффициент +2,3 гр.
  return String(IN7);
}

// Датчик Soil Moisture Sensor (Датчик влажности почвы)
int moisture_pin = 36; // Указываем номер аналогового пина
int output_value;

String getoutput_value()
{
  output_value = analogRead(moisture_pin);
  output_value = map(output_value, 4090, 1600, 0, 100); // Настроить, где: 4090 = 0% влажности, 2900 = 100% влажности.
  uint8_t IN8 = (output_value);                         // Считывание данных и создание переменной с именем IN8
  return String(IN8);
}

// Датчик Soil Moisture Sensor (Датчик дождя)
int moisture_pin2 = 39; // Указываем номер аналогового пина
int output_value2;

String getoutput_value2()
{
  output_value2 = analogRead(moisture_pin2);
  output_value2 = map(output_value2, 4090, 2900, 0, 100); // Настроить, где: 4090=0% влажности, 2900=100% влажности.
  uint8_t IN9 = (output_value2);
  return String(IN9);
}

void initBME280()
{
  bool status;
  status = bme.begin(0x76);
  if (!status)
  {
    Serial.println("Не обнаружен датчик BME280, проверьте соединение");
    while (1);
  }
  // delayTime = 1000; // Задержка для сериал монитора
}

void initDHT22()
{
  // Serial.println(F("запуск датчика DHT22..."));
  dht.begin();
}

 void initBH1750()
 {
   Wire.begin();
   lightMeter.begin();
   // Serial.println(F("запуск датчика BH1750..."));
 }

void loopSensors()
{
  delay(delayTime);
}