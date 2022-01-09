// Импортируем необходимые библиотеки:
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncElegantOTA.h>

// Задаем сетевые настройки
const char* ssid = "Satman_WLAN";
const char* password = "9uthfim8";
IPAddress local_IP(192, 168, 11, 68);  // Задаем статический IP-адрес: аналогичный адрес прописать в файле index.html
IPAddress gateway(192, 168, 11, 102);  // Задаем IP-адрес сетевого шлюза:
IPAddress subnet(255, 255, 255, 0);    // Задаем маску сети:
IPAddress primaryDNS(8, 8, 8, 8);      // Основной ДНС (опционально)
IPAddress secondaryDNS(8, 8, 4, 4);    // Резервный ДНС (опционально)
AsyncWebServer server(80);             // Создаем сервер через 80 порт
AsyncWebSocket ws("/ws");              // Создаем объект WebSocket

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain;charset=utf-8", "Страница не найдена");
}

// DTH22 - Датчик температуры и влажности
#define DHTPIN 13                       //Указываем номер цифрового pin для чтения данных
#define DHTTYPE DHT22                   //Указываем тип датчика
DHT dht(DHTPIN, DHTTYPE);

String getDHTTemperature() {
  float IN1 = dht.readTemperature();                   // Считывание температуры и создание переменной с именем IN1
  if (isnan(IN1)) {                                    // Проверяем, не удалось ли выполнить какое-либо чтение, после чего выходим раньше (чтобы повторить попытку).
   // Serial.println("Ошибка чтения с датчика DTH22!");
    return "--";
  }
  else {
  //  Serial.print("DTH22- Температура: ");
  //  Serial.print(IN1);                                 // Вывод температуры в последовательный монитор
  //  Serial.print(" °C ");                              // Вывод текста в монитор порта
    return String(IN1);
  }
}

String getDHTHumidity() {
  float IN2 = dht.readHumidity();                      // Считывание влажности и создание переменной с именем IN2
  if (isnan(IN2)) {                                    // Проверяем, не удалось ли выполнить какое-либо чтение, после чего выходим раньше (чтобы повторить попытку).
  //  Serial.println("Ошибка чтения с датчика DTH22!");
    return "--";
  }
  else {
  //  Serial.print("DTH22- Влажность: ");
  //  Serial.print(IN2);
  //  Serial.println(" % ");
    return String(IN2);
  }
}

// BH1750 - Датчик света
BH1750 lightMeter;
String getLightLevel() {
  float IN3 = lightMeter.readLightLevel();            // Считывание данных и создание переменной с именем IN3
//  Serial.print("Освещенность: ");                     // Вывод текста в монитор порта
//  Serial.print(IN3);                                  // Вывод показаний в последовательный монитор порта
//  Serial.println(" люкс");
  return String(IN3);
}

// BME280 - Универсальный датчик
Adafruit_BME280 bme;                                  // Подключаем датчик в режиме I2C

String getTemperature2() {
  float IN4 = bme.readTemperature() - 1.04;
//  Serial.print("BME280- Температура: ");
//  Serial.print(IN4);
//  Serial.print(F(" °C "));
  return String(IN4);
}

String getPressure() {
  float IN5 = bme.readPressure() / 133.3;
// Serial.print(" Давление: ");
// Serial.print(IN5);
//  Serial.print(" мм.рт.ст ");
  return String(IN5);
}

String getHumidity() {
  float IN6 = bme.readHumidity();
//  Serial.print(" Влажность: ");
// Serial.print(IN6);
//  Serial.println(" %");
  return String(IN6);
}

// DS18B20 - Датчик температуры почвы
OneWire ds(15);                        // Указываем номер пина.

String gettemperature3() {
  byte data[2];     // Место для значения температуры
  ds.reset();       // Начинаем взаимодействие со сброса всех предыдущих команд и параметров
  ds.write(0xCC);   // Даем датчику DS18b20 команду пропустить поиск по адресу. В нашем случае только одно устройство
  ds.write(0x44);   // Даем датчику DS18b20 команду измерить температуру. Само значение температуры мы еще не получаем - датчик его положит во внутреннюю память
  delay(1000);      // Микросхема измеряет температуру, а мы ждем.
  ds.reset();       // Теперь готовимся получить значение измеренной температуры
  ds.write(0xCC);
  ds.write(0xBE);   // Просим передать нам значение регистров со значением температуры

  // Получаем и считываем ответ
  data[0] = ds.read();   // Читаем младший байт значения температуры
  data[1] = ds.read();   // А теперь старший

  // Формируем итоговое значение:
  // - сперва "склеиваем" значение,
  // - затем умножаем его на коэффициент, соответствующий разрешающей способности (для 12 бит по умолчанию - это 0,0625)
  float temperature =  ((data[1] << 8) | data[0]) * 0.0625;

  float IN7 = ((temperature) + 2.3);                      // Считывание данных и создание переменной с именем IN7, поправочный коэффициент +2,3 гр.
//  Serial.print("DS18B20- Температура почвы: ");           // Вывод текста в монитор порта
// Serial.print(IN7);                                      // Вывод показаний в последовательный монитор порта
//  Serial.println(" °C");                                  // Вывод текста в монитор порта
  return String(IN7);
}

//Датчик Soil Moisture Sensor (Датчик влажности почвы)
int moisture_pin = 36;                 // Указываем номер аналогового пина
int output_value ;

String getoutput_value() {
  output_value = analogRead(moisture_pin);
  output_value = map(output_value, 4090, 2900, 0, 100);  // Настроить, где: 4090=0% влажности, 2900=100% влажности.
  float IN8 = (output_value);                            // Считывание данных и создание переменной с именем IN8
 // Serial.print("Влажность почвы: ");
//  Serial.print(IN8);                                     // Вывод показаний в последовательный монитор порта
 // Serial.println("%");
 // Serial.println();
  //if (IN8>=50) digitalWrite(33, HIGH);                  // При понижении влажности до 50% и менее, подать 1 на 33 пин.
  //else digitalWrite(33, LOW);                           // Сброс реле в исходное состояние
  return String(IN8);
}

// На всех выводах GPIO по умолчанию устанавливаем 0. 
bool ledState1 = 0;
bool ledState2 = 0;
bool ledState3 = 0;
bool ledState4 = 0;
bool ledState5 = 0;

// Объявляем переменные для выходов.
const int ledPin1 = 32;
const int ledPin2 = 33;
const int ledPin3 = 25;
const int ledPin4 = 26;
const int ledPin5 = 27;

String defTemp1 = "29.0";              // Верхнее пороговое значение температуры по умолчанию
String defTemp1_1 = "28.0";            // Нижнее пороговое значение температуры по умолчанию
String lastTemp1;                      // Последние показания температуры, которые будут сравниваться с пороговым значением.
String Flag1 = "checked";              // Переменная Flag1 сообщит нам, установлен ли флажок для автоматического управления выводом или нет.
String Auto1 = "true";                 // Если флажок установлен, значение, сохраненное на Auto1, должно быть установлено в true.

String defTemp2 = "28.5";
String defTemp2_1 = "27.5";
String lastTemp2;
String Flag2 = "checked";
String Auto2 = "true";

//----------------------->
// Переменная флага для отслеживания того, были ли активированы триггеры или нет
bool triggerActive = false;
// Следующие переменные будут использоваться для проверки того, получили ли мы HTTP-запрос 
// GET из этих полей ввода, и сохранения значений в переменные соответственно.
const char* PARAM_INPUT_1 = "threshold_input1";
const char* PARAM_INPUT_1_1 = "threshold_input1_1";
const char* PARAM_INPUT_1_2 = "enable_arm_input";

const char* PARAM_INPUT_2 = "threshold_input2";
const char* PARAM_INPUT_2_1 = "threshold_input2_1";
const char* PARAM_INPUT_2_2 = "enable_arm_input";

// Интервал между показаниями датчиков.
unsigned long previousMillis = 0;     
const long interval = 5000;
// <---------------------

// Уведомляем клиентов о текущем состоянии светодиода
void notifyClients1() {ws.textAll(String(ledState1));}
void notifyClients2() {ws.textAll(String(ledState2 + 2));}
void notifyClients3() {ws.textAll(String(ledState3 + 4));}
void notifyClients4() {ws.textAll(String(ledState4 + 6));}
void notifyClients5() {ws.textAll(String(ledState5 + 8));}

/* функция обратного вызова, которая запускается всякий раз, когда мы получаем новые
  данные от клиентов по протоколу WebSocket. Если мы получаем сообщение “toggle”, мы
  переключаем значение переменной ledState. Кроме того, мы уведомляем всех клиентов,
  вызывая функцию notifyClients (). Таким образом, все клиенты получают уведомление об
  изменении и соответствующим образом обновляют интерфейс.*/

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle1") == 0) {ledState1 = !ledState1;notifyClients1();}
    if (strcmp((char*)data, "toggle2") == 0) {ledState2 = !ledState2;notifyClients2();}
    if (strcmp((char*)data, "toggle3") == 0) {ledState3 = !ledState3;notifyClients3();}
    if (strcmp((char*)data, "toggle4") == 0) {ledState4 = !ledState4;notifyClients4();}
    if (strcmp((char*)data, "toggle5") == 0) {ledState5 = !ledState5;notifyClients5();}
  }
}

// Настройка прослушивания событий для обработки различных асинхронных шагов протокола WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:                  // когда клиент вошел в систему
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:               // когда клиент вышел из системы
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:                     // при получении пакета данных от клиента
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:                     // в ответ на запрос ping
    case WS_EVT_ERROR:                    // при получении ошибки от клиента
      break;
  }
}

String processor(const String& var) {
  Serial.println(var);
  if (var == "STATE1") {if (ledState1) {return "ON";} else {return "OFF";}}
  if (var == "STATE2") {if (ledState2) {return "ON";} else {return "OFF";}}
  if (var == "STATE3") {if (ledState3) {return "ON";} else {return "OFF";}}
  if (var == "STATE4") {if (ledState4) {return "ON";} else {return "OFF";}}
  if (var == "STATE5") {if (ledState5) {return "ON";} else {return "OFF";}}

//----------------------->
//Заменяет placeholder значениями BME280
// String processor(const String& var) {
 // Serial.println(var);
    if(var == "DATA4"){
return lastTemp1;
  }
  else if(var == "THRESHOLD1"){
    return defTemp1;
  }
    else if(var == "THRESHOLD1_1"){
    return defTemp1_1;
  }
  else if(var == "ENABLE_ARM_INPUT"){
    return Flag1;
  }
   
   if(var == "DATA4"){
return lastTemp2;
  }
  else if(var == "THRESHOLD2"){
    return defTemp2;
  }
    else if(var == "THRESHOLD2_1"){
    return defTemp2_1;
  }
  else if(var == "ENABLE_ARM_INPUT"){
    return Flag2;
  }
  return String();
}

// Инициализация WebSocket
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(32, OUTPUT); digitalWrite(32, LOW);
  pinMode(33, OUTPUT); digitalWrite(33, LOW);
  pinMode(25, OUTPUT); digitalWrite(25, LOW);
  pinMode(26, OUTPUT); digitalWrite(26, LOW);
  pinMode(27, OUTPUT); digitalWrite(27, LOW);

  //DTH22
  Serial.println(F("запуск датчика DHT22..."));
  dht.begin();

  // Датчик BH1750
  Wire.begin();
  lightMeter.begin();
  Serial.println(F("запуск датчика BH1750..."));

  // Инициализация датчика BME280
  if (!bme.begin(0x76)) {
    Serial.println("Не обнаружен датчик BME280, проверьте соединение!");
    while (1);
  }

  //Датчик Soil_Moisture_Sensor
  Serial.println("запуск датчика влажности почвы... ");

  // Настраиваем статический IP-адрес:
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Режим клиента не удалось настроить");
  }
  
      // SPIFFS:
  if (!SPIFFS.begin()) {
    Serial.println("An error occurred while installing the SPIFFS");
    return;
  }
 
  // Connecting to WiFi:
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(1000);
Serial.println("Connecting to WiFi...");
}

 // initSPIFFS();
 // initWiFi();
  initWebSocket();

  //=============== Отправляем в браузер вэб страницу ==================

 // Маршрут до корневого каталога веб страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
	request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });

  // Маршрут для загрузки файла style.css
 server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
   request->send(SPIFFS, "/style.css", "text/css");
 });

 server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
   request->send(SPIFFS, "/script.js", "text/javascript");
 });

  // =========== Теперь отправим в браузер все наши картинки ==============
   server.on("/teplica.png", HTTP_GET, [](AsyncWebServerRequest * request) {
     request->send(SPIFFS, "/teplica.png", "image/png");
   });

  server.on("/sungif.gif", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/sungif.gif", "image/gif");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/favicon.ico", "image/ico");
  });

  server.on("/casa.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/casa.png", "image/png");
  });

  server.on("/flame.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/flame.png", "image/png");
  });

  server.on("/humid.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/humid.png", "image/png");
  });

  server.on("/sun.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/sun.png", "image/png");
  });

  server.on("/temp.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/temp.png", "image/png");
  });

  //============= Отправляем в браузер показания датчиков =============
  server.on("/IN1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getDHTTemperature().c_str());
  });

  server.on("/IN2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getDHTHumidity().c_str());
  });

  server.on("/IN3", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getLightLevel().c_str());
  });

  server.on("/IN4", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getTemperature2().c_str());
  });

  server.on("/IN5", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getPressure().c_str());
  });

  server.on("/IN6", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getHumidity().c_str());
  });

  server.on("/IN7", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", gettemperature3().c_str());
  });

  server.on("/IN8", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getoutput_value().c_str());
  });


// --------------------->
// Итак, мы проверяем, содержит ли запрос входные параметры, и сохраняем эти параметры в переменные:
  // Получите HTTP GET запрос по адресу <ESP_IP>/get?threshold_input=<inputMessage>&enable_arm_input=<inputMessage2>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
/* Это часть кода, где переменные будут заменены значениями, представленными на форме. Переменная inputMessage 
сохраняет пороговое значение температуры, а переменная inputMessage2 сохраняет, установлен ли флажок или нет 
(если мы должны контролировать GPIO или нет)*/
    // ПОЛУЧИТЬ значение threshold_input on <ESP_IP>/get?threshold_input=<inputMessage>
   if (request->hasParam(PARAM_INPUT_1) && (PARAM_INPUT_1_1)) {    
      defTemp1 = request->getParam(PARAM_INPUT_1)->value();     // Верхний порог
      defTemp1_1 = request->getParam(PARAM_INPUT_1_1)->value(); // Нижний порог
      // ПОЛУЧИТЬ значение enable_arm_input1 on<ESP_IP>/get?enable_arm_input1=<Auto1>
      if (request->hasParam(PARAM_INPUT_1_2)) {
        Auto1 = request->getParam(PARAM_INPUT_1_2)->value();
        Flag1 = "checked";
 //  digitalWrite(ledPin4, HIGH); // Если флажок установлен, выполним эту часть кода, т.е 1 на GPIO27
      }
      else {
        Auto1 = "false";
        Flag1 = "";
 //  digitalWrite(ledPin4, LOW); // Если флажок не установлен, выполним эту часть кода, т.е 0 на GPIO27
      }
    }
   //Serial.println(defTemp1);
     Serial.println(Auto1);

   if (request->hasParam(PARAM_INPUT_2) && (PARAM_INPUT_2_1)) {    
      defTemp2 = request->getParam(PARAM_INPUT_2)->value();     // Верхний порог
      defTemp2_1 = request->getParam(PARAM_INPUT_2_1)->value(); // Нижний порог
      if (request->hasParam(PARAM_INPUT_2_2)) {
        Auto2 = request->getParam(PARAM_INPUT_2_2)->value();
        Flag2 = "checked";
      }
      else {
        Auto2 = "false";
        Flag2 = "";
      }
    }
    // Serial.println(defTemp2);
     Serial.println(Auto2);
	
    request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });
  
  server.onNotFound(notFound);
// <--------------------------

  AsyncElegantOTA.begin(&server);   // Запускаем ElegantOTA
  server.begin();                   // Запускаем сервер
}

void loop() {
  AsyncElegantOTA.loop();
 // ws.cleanupClients();
 
 // Проверяем, если галка снята (ручной режим), то 
 if (Auto1 == "false") digitalWrite(ledPin1, ledState1);
 if (Auto2 == "false") digitalWrite(ledPin2, ledState2);
 //if (Auto3 == "false") digitalWrite(ledPin3, ledState3);
 //if (Auto4 == "false") digitalWrite(ledPin4, ledState4);
 //if (Auto5 == "false") digitalWrite(ledPin5, ledState5);

// -----------------------------> 
 // Считываем с датчика показания температуры каждые 5 секунд.
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
   float IN4 = bme.readTemperature()-1.04;
  // const int IN4 = bme.readTemperature()-1.04;
      Serial.print(IN4);
      Serial.println(" *C");
 //  lastTemp1 = String(IN4);  // если закоментировать, не отобразит температуру, но работать будет
    

// Верхняя форточка - порог открытия
    if (IN4 > defTemp1.toFloat() && Auto1 == "true" && !triggerActive) {
      Serial.println("Температура достигла верхний порог №1");
      triggerActive = true;
      digitalWrite(ledPin1, HIGH);}
// Порог закрытия
    if (IN4 < defTemp1_1.toFloat() && Auto1 == "true" && triggerActive) {
    Serial.println("Температура опустилась до нижнего порога №1");
    triggerActive = false;
      digitalWrite(ledPin1, LOW);}
	
// Нижняя форточка - порог открытия
    if (IN4 > defTemp2.toFloat() && Auto2 == "true" && !triggerActive) {
      Serial.println("Температура достигла верхний порог №2");
      triggerActive = true;
      digitalWrite(ledPin2, HIGH);}
// Порог закрытия
    if (IN4 < defTemp2_1.toFloat() && Auto2 == "true" && triggerActive) {
    Serial.println("Температура опустилась до нижнего порога №2");
    triggerActive = false;
      digitalWrite(ledPin2, LOW);}

// Если температура обоих порогов ниже заданных уровней, на выходе 0.
    //  else if((IN4 < (defTemp1.toFloat() && defTemp1_1.toFloat())) && Auto1 == "true") {
    //   digitalWrite(ledPin1, LOW);
    //  }
    }
  }
