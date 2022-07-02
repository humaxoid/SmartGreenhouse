
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncElegantOTA.h>
#include <sensors.h>

// Задаем сетевые настройки
const char *ssid = "Satman_WLAN";
const char *password = "9uthfim8";
AsyncWebServer server(80); // Запускаем асинхронный веб-сервер на 80 порту
AsyncWebSocket ws("/ws");  // Создаём объект, который будет обрабатывать websocket-ы:

uint32_t timer1, timer2, timer3; // Переменные хранения времени (unsigned long)

// На всех выводах GPIO по умолчанию устанавливаем 0.
bool ledState1 = false, ledState2 = false, ledState3 = false, ledState4 = false, ledState5 = false, ledState6 = false;

// Объявляем дефайн для выходов.
#define ledPin1 32 // Верхняя форточка
#define ledPin2 33 // Нижняя форточка
#define ledPin3 25 // Таймер грядки-1
#define ledPin4 26 // Таймер грядки-2
#define ledPin5 27 // Таймер грядки-3
#define ledPin6 2  // Вывод на светодиод режима Авто/Ручной.

String defTemp1 = "26.0";   // Верхнее пороговое значение температуры по умолчанию
String defTemp1_1 = "22.0"; // Нижнее пороговое значение температуры по умолчанию
String defTemp2 = "28.0";
String defTemp2_1 = "24.0";
String lastTemp; // Последние показания температуры, которые будут сравниваться с пороговыми значениями.

String defTime3 = "72000";   // Периодичность включения таймера по умолчанию 8 часов
String defTime3_1 = "14400"; // Длительность работы таймера по умолчанию 4 часа
String defTime4 = "79200";
String defTime4_1 = "7200";
String defTime5 = "79200";
String defTime5_1 = "7200";

String Flag = "checked"; // Переменная "Flag" сообщает нам, установлен ли флажок режима "Авто" или нет.
String Auto = "true";    // Если флажок установлен, переменная "Auto" примет значение true.

// Локальные переменные для отслеживания, были ли активированы триггеры или нет.
bool triggerActive1 = false; // верхн.форточка
bool triggerActive2 = false; // нижн.форточка
bool triggerActive3 = false; // таймер 1
bool triggerActive4 = false; // таймер 1
bool triggerActive5 = false; // таймер 1
bool triggerActive6 = false; // анимация

// Интервалы между показаниями датчиков и обр.отсчета таймеров
uint32_t previousMillis1 = 0; // время, когда таймер последний раз сработал.
uint32_t previousMillis2 = 0;
uint32_t previousMillis3 = 0;
const long interval1 = 10; // 10 сек. цикличность опроса датчиков
const long interval2 = 2;  // 2 сек. цикличность обратного отсчета времени
const long interval3 = 10; // 10 сек. цикличность обновления uptime

// Следующие переменные будут использоваться для проверки того, получили ли мы HTTP-запрос
// GET из этих полей ввода, и для сохранения значений в переменные.
const char *PARAM_INPUT_1 = "threshold_input1";
const char *PARAM_INPUT_1_1 = "threshold_input1_1";
const char *PARAM_INPUT_2 = "threshold_input2";
const char *PARAM_INPUT_2_1 = "threshold_input2_1";

const char *PARAM_INPUT_3 = "time_input3";
const char *PARAM_INPUT_3_1 = "time_input3_1";
const char *PARAM_INPUT_4 = "time_input4";
const char *PARAM_INPUT_4_1 = "time_input4_1";
const char *PARAM_INPUT_5 = "time_input5";
const char *PARAM_INPUT_5_1 = "time_input5_1";

const char *PARAM_INPUT_flag = "enable_arm_input";

/* функция обратного вызова, которая запускается каждый раз, когда мы получаем новые
  данные от клиентов (js -->) по протоколу WebSocket. Если мы получаем сообщение
  “toggle”, мы переключаем значение переменной ledState. Кроме того, мы уведомляем всех
  клиентов (--> js), вызывая ws.textAll. Таким образом, все клиенты
  получают уведомление об изменении и соответствующим образом обновляют интерфейс.*/

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{

  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if ((info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) && Auto == "false")
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle1") == 0) // Слушаем сообщения js -->, если toggle1 = 0, то выполняем код ниже.
    {
      ledState1 = !ledState1;        // Установим значение переменной ledState = 1, но если кнопка не нажата тогда ledState = 0
      ws.textAll(String(ledState1)); // Уведомляем клиентов о переключении кнопки.
      // Serial.println(ledState1);
    }
    if (strcmp((char *)data, "toggle2") == 0)
    {
      ledState2 = !ledState2;
      ws.textAll(String(ledState2 + 2));
    }
    if (strcmp((char *)data, "toggle3") == 0)
    {
      ledState3 = !ledState3;
      ws.textAll(String(ledState3 + 4));
    }
    if (strcmp((char *)data, "toggle4") == 0)
    {
      ledState4 = !ledState4;
      ws.textAll(String(ledState4 + 6));
    }
    if (strcmp((char *)data, "toggle5") == 0)
    {
      ledState5 = !ledState5;
      ws.textAll(String(ledState5 + 8));
    }
  }
}

/* Настройка прослушивания событий для отслеживания для обработки различных шагов клиента таких как:
вход в систему, выход из системы, получение данных, получение ошибки, ответ на ping: */

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT: // если клиент подключился
    Serial.printf("Клиент №%u подключился с адреса %s\n", client->id(), client->remoteIP().toString().c_str());
    // при обновлении страницы (переподключение) обновляем статус кнопки.
    if (ledState1)
    {
      ws.textAll(String(1));
    }
    if (ledState2)
    {
      ws.textAll(String(3));
    }
    // ---------------------
    if (ledState3)
    {
      ws.textAll(String(5));
    }
    if (ledState4)
    {
      ws.textAll(String(7));
    }
    if (ledState5)
    {
      ws.textAll(String(9));
    }
    if (ledState6)
    {
      ws.textAll(String(11));
    }
    break;
  case WS_EVT_DISCONNECT: // если клиент отключился
    Serial.printf("Клиент №%u отключился\n", client->id());
    break;
  case WS_EVT_DATA: // обработка полученных данных
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:  // в ответ на запрос ping
  case WS_EVT_ERROR: // при получении ошибки от клиента
    break;
  }
}

// Функция processor() заменяет все заполнители между символами %---%, в HTML-тексте фактическими значениями.
String processor(const String &var)
{
  // Serial.println(var);
  if (var == "STATE1")
  {
    if (ledState1)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }

  if (var == "STATE2")
  {
    if (ledState2)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }
  if (var == "STATE3")
  {
    if (ledState3)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }
  if (var == "STATE4")
  {
    if (ledState4)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }
  if (var == "STATE5")
  {
    if (ledState5)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }

  // ############# Температура ##############
  if (var == "DATA4")
  { // Последние показания температуры, которые будут сравниваться с пороговыми значениями.
    return lastTemp;
  }
  else if (var == "THRESHOLD1")
  { // Верхн. форточка- верхний порог t°
    return defTemp1;
  }
  else if (var == "THRESHOLD1_1")
  { // Верхн. форточка- нижний порог t°
    return defTemp1_1;
  }
  else if (var == "THRESHOLD2")
  { // Нижн. форточка- верхний порог t°
    return defTemp2;
  }
  else if (var == "THRESHOLD2_1")
  { // Нижн. форточка- нижний порог t°
    return defTemp2_1;
  }
  // ############## Таймеры ################
  else if (var == "TIME3")
  {
    return defTime3;
  }
  else if (var == "TIME3_1")
  {
    return defTime3_1;
  }
  else if (var == "TIME4")
  {
    return defTime4;
  }
  else if (var == "TIME4_1")
  {
    return defTime4_1;
  }
  else if (var == "TIME5")
  {
    return defTime5;
  }
  else if (var == "TIME5_1")
  {
    return defTime5_1;
  }

  else if (var == "ENABLE_ARM_INPUT")
  {
    return Flag;
  }
  //Прекращаем вычисления в функции и возвращаем значение из прерванной функции в вызывающую.
  return String();
}

// ----------------------------------------------------------------
// Инициализация
// ----------------------------------------------------------------

// Запускаем SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("При монтировании SPIFFS, произошла ошибка");
  }
  Serial.println("SPIFFS примонтировано успешно!");
}

void initWiFi()
{
  WiFi.begin(ssid, password);
  // Serial.print("Подключаемся к WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  // Serial.println();
  Serial.printf("Подключился к сети %s\n", ssid);
  Serial.print("ESP32 IP адрес: ");
  Serial.println(WiFi.localIP());
}

void initWebServer()
{
  AsyncElegantOTA.begin(&server);
  server.begin();
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain;charset=utf-8", "Страница не найдена");
}

void setup()
{
  // Зададим скорость последовательного порта.
  Serial.begin(115200);
  delay(1000);

  initDHT22();     // Инициализация датчика HDT22, в файле sensors.h
  initBH1750();    // Инициализация датчика BH1750, в файле sensors.h
  initBME280();    // Инициализация датчика BME280, в файле sensors.h
  initSPIFFS();    // Инициализация SPIFFS
  initWiFi();      // Инициализация WiFi
  initWebServer(); // Инициализация Web сервера
  initWebSocket(); // Инициализация Websocket

  // Объявим GPIO выходы (по умолчанию LOW)
  pinMode(ledPin1, OUTPUT);
  digitalWrite(ledPin1, LOW);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, LOW);
  pinMode(ledPin3, OUTPUT);
  digitalWrite(ledPin3, LOW);
  pinMode(ledPin4, OUTPUT);
  digitalWrite(ledPin4, LOW);
  pinMode(ledPin5, OUTPUT);
  digitalWrite(ledPin5, LOW);
  pinMode(ledPin6, OUTPUT);
  digitalWrite(ledPin6, HIGH); // Вывод светодиодного индикатора режимов Авто/Ручной.

  // Маршрут до корневого каталога веб страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html", false, processor); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/script.js", "text/javascript"); });

  server.on("/seven-segment.otf", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/seven-segment.otf", "application/octet-stream"); });

  server.on("/teplica.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/teplica.png", "image/png"); });

  server.on("/up.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/up.gif", "image/gif"); });

  server.on("/down.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/down.gif", "image/gif"); });

  server.on("/pic1.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/pic1.gif", "image/gif"); });

  server.on("/pic2.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/pic2.gif", "image/gif"); });

  server.on("/baklazan.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/baklazan.gif", "image/gif"); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/favicon.ico", "image/ico"); });

  server.on("/casa.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/casa.png", "image/png"); });

  server.on("/flame.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/flame.png", "image/png"); });

  server.on("/humid.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/humid.png", "image/png"); });

  server.on("/sun.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/sun.png", "image/png"); });

  server.on("/temp.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/temp.png", "image/png"); });

  //============= Отправляем в браузер показания датчиков =============
  server.on("/IN1", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDHTTemperature().c_str()); });

  server.on("/IN2", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDHTHumidity().c_str()); });

  server.on("/IN3", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getLightLevel().c_str()); });

  server.on("/IN4", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getTemperature2().c_str()); });

  server.on("/IN5", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getPressure().c_str()); });

  server.on("/IN6", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getHumidity().c_str()); });

  server.on("/IN7", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", gettemperature3().c_str()); });

  server.on("/IN8", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getoutput_value().c_str()); });

  //===================== Реальное время с NTP сервера ========================
  //  server.on("/printLocalTime", HTTP_GET, [](AsyncWebServerRequest *request)
  //             { request->send(200, "text/plain", printLocalTime()); });

  /* Итак, мы проверяем, содержит ли запрос входные параметры, и сохраняем эти параметры в переменные:
  Получаем HTTP GET запрос по адресу url/get?threshold_input=<inputMessage>&enable_arm_input=<inputMessage2> */
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {

/* Эта часть кода, где переменные будут заменены значениями, представленными на форме. Переменная inputMessage 
сохраняет пороговое значение температуры, а переменная inputMessage2 сохраняет, установлен ли флажок или нет 
(если мы должны контролировать GPIO или нет)
ПОЛУЧИТЬ значение threshold_input on 192.168.11.68/get?threshold_input=<inputMessage> */
              if (request->hasParam(PARAM_INPUT_1) && (PARAM_INPUT_1_1) && (PARAM_INPUT_2) && (PARAM_INPUT_2_1) && (PARAM_INPUT_3) && (PARAM_INPUT_3_1) && (PARAM_INPUT_4) && (PARAM_INPUT_4_1) && (PARAM_INPUT_5) && (PARAM_INPUT_5_1))
              {
                defTemp1 = request->getParam(PARAM_INPUT_1)->value();     // Верхняя форточка, верхний порог t°.
                defTemp1_1 = request->getParam(PARAM_INPUT_1_1)->value(); // Верхняя форточка, нижний порог t°.
                defTemp2 = request->getParam(PARAM_INPUT_2)->value();     // Нижняя форточка, нижний порог t°.
                defTemp2_1 = request->getParam(PARAM_INPUT_2_1)->value(); // Нижняя форточка, нижний порог t°.
                defTime3 = request ->getParam(PARAM_INPUT_3)->value();    // Таймер 1
                defTime3_1 = request->getParam(PARAM_INPUT_3_1)->value();
                defTime4 = request->getParam(PARAM_INPUT_4)->value();     // Таймер 2
                defTime4_1 = request->getParam(PARAM_INPUT_4_1)->value();
                defTime5 = request->getParam(PARAM_INPUT_5)->value();     // Таймер 3
                defTime5_1 = request->getParam(PARAM_INPUT_5_1)->value();

                // ПОЛУЧИТЬ значение enable_arm_input1 on<ESP_IP>/get?enable_arm_input1=<Auto1>
                if (request->hasParam(PARAM_INPUT_flag))
                {
                  Auto = request->getParam(PARAM_INPUT_flag)->value();
                  Flag = "checked";
                  digitalWrite(ledPin6, HIGH); // Индикация режима "Авто".
                }
                else
                {
                  Auto = "false";
                  Flag = "";
                  digitalWrite(ledPin6, LOW); // Индикация режима "Ручное".
                }
              }
              // Serial.println(Auto);
			request->send(SPIFFS, "/index.html", "text/html", false, processor); });
}

void loop()
{
  ws.cleanupClients();
  // loopSensors(); // запуск loop в файле sensors.h

  // Проверяем, если галка снята (ручной режим), то...
  if (Auto == "false")
    digitalWrite(ledPin1, ledState1);
  if (Auto == "false")
    digitalWrite(ledPin2, ledState2);
  if (Auto == "false")
    digitalWrite(ledPin3, ledState3);
  if (Auto == "false")
    digitalWrite(ledPin4, ledState4);
  if (Auto == "false")
    digitalWrite(ledPin5, ledState5);

  uint32_t currentMillis = millis() / 1000; // объявляем переменную currentMillis в секундах

  if (currentMillis - previousMillis1 >= interval1) // Создаем локальную задержку 10 сек.
  {

// ############################# Защита от дождя ########################################

// uint16_t rain = analogRead(39) < 2700;   // переменная порога начала дождя
// uint16_t norain = analogRead(39) > 3600; // переменная порога окончания дождя
#define rain analogRead(39) < 2700
#define norain analogRead(39) > 3500
    // Serial.println(analogRead(39));

    if (rain && !triggerActive6) // если пошел дождь, то ...
    {
      triggerActive6 = true;
      ledState6 = !ledState6; // запоминать состояние
      ws.textAll(String(11));
      Serial.println("Начался дождь");
    }

    else if (norain && triggerActive6) // если дождь прекратился, то ...
    {
      triggerActive6 = false;
      ledState6 = !ledState6;
      ws.textAll(String(10));
      Serial.println("Дождь прекратился");
    }

    // #######################  Раздел открывания/закрывания форточек ############################

    // Верхняя форточка - порог открытия
    if ((bme.readTemperature() > defTemp1.toFloat() && norain) && Auto == "true" && !triggerActive1) // с датчиком дождя
    // if ((bme.readTemperature() > defTemp1.toFloat()) && Auto == "true" && !triggerActive1) // без датчика дождя
    {
      Serial.println("Верхняя форточка открылась - t° выше порога");
      triggerActive1 = true;       // опрокинуть триггер
      ledState1 = !ledState1;      // запоминать состояние
      digitalWrite(ledPin1, HIGH); // подаем на выход высокий уровень
      ws.textAll(String(1));       // отправляем статус кнопки "ON"
    }

    // Порог закрытия
    else if (((bme.readTemperature() < defTemp1_1.toFloat()) || rain) && Auto == "true" && triggerActive1)
    // else if ((bme.readTemperature() < defTemp1_1.toFloat()) && Auto == "true" && triggerActive1)
    {
      Serial.println("Верхняя форточка закрылась - t° ниже порога");
      triggerActive1 = false;     // опрокинуть триггер
      ledState1 = !ledState1;     // запоминать состояние
      digitalWrite(ledPin1, LOW); // подаем на выход низкий уровень
      ws.textAll(String(0));      // отправляем статус кнопки "OFF"
    }

    // Нижняя форточка - порог открытия
    if (bme.readTemperature() > defTemp2.toFloat() && Auto == "true" && !triggerActive2)
    {
      triggerActive2 = true;
      ledState2 = !ledState2;
      digitalWrite(ledPin2, HIGH);
      ws.textAll(String(3));
    }

    // Порог закрытия
    else if (bme.readTemperature() < defTemp2_1.toFloat() && Auto == "true" && triggerActive2)
    {
      triggerActive2 = false;
      ledState2 = !ledState2;
      digitalWrite(ledPin2, LOW);
      ws.textAll(String(2));
    }
    previousMillis1 = currentMillis;
  }

  // #########################  Дальше раздел таймеров полива ############################

  if (currentMillis - timer1 >= (ledState3 ? defTime3_1.toFloat() : defTime3.toFloat()) && Auto == "true" && !triggerActive3)
  {
    timer1 = currentMillis; //перезаписываем в переменную timer1 текущее значение millis()
    ledState3 = !ledState3;
    digitalWrite(ledPin3, ledState3);   // переключаем состояние PIN
    ws.textAll(String(!ledState3 + 4)); // отправляем состояние кнопки
    Serial.println("ON");
  }

  if (currentMillis - timer2 >= (ledState4 ? defTime4_1.toFloat() : defTime4.toFloat()) && Auto == "true" && !triggerActive4)
  {
    timer2 = currentMillis;
    ledState4 = !ledState4;
    digitalWrite(ledPin4, ledState4);
    ws.textAll(String(!ledState4 + 6));
  }

  if (currentMillis - timer3 >= (ledState5 ? defTime5_1.toFloat() : defTime5.toFloat()) && Auto == "true" && !triggerActive5)
  {
    timer3 = currentMillis;
    ledState5 = !ledState5;
    digitalWrite(ledPin5, ledState5);
    ws.textAll(String(!ledState5 + 8));
  }

  // ############# Обратный отсчет до срабатывания таймеров ##############

  if (currentMillis - previousMillis2 >= interval2) // интервал каждые 2 сек.
  {
    if (!ledState3 && Auto == "true")
    {
      uint32_t countdown1 = defTime3.toFloat() - (currentMillis - timer1);
      uint8_t H1 = countdown1 / 3600;               // остаток часов
      uint8_t M = countdown1 / 60 % 60;             // остаток минут
      uint8_t S = countdown1 % 60;                  // остаток секунд
      char buffer1[9];                              // кол-во знаков
      sprintf(buffer1, "%02d:%02d:%02d", H1, M, S); // в буфер формат чч:мм:cc
      ws.textAll("buffer1=" + String(buffer1));     // отправка на страницу
      // Serial.println(buffer1);                   // в сериал порт
    }
    else if (ledState3 && Auto == "true")
    {
      ws.textAll("buffer1=" + String("ON"));
    }

    if (!ledState4 && Auto == "true")
    {
      uint32_t countdown2 = defTime4.toFloat() - (currentMillis - timer2);
      uint8_t H2 = countdown2 / 3600;
      uint8_t M2 = countdown2 / 60 % 60;
      uint8_t S2 = countdown2 % 60;
      char buffer2[9];
      sprintf(buffer2, "%02d:%02d:%02d", H2, M2, S2);
      ws.textAll("buffer2=" + String(buffer2));
    }
    else if (ledState4 && Auto == "true")
    {
      ws.textAll("buffer2=" + String("ON"));
    }

    if (!ledState5 && Auto == "true")
    {
      uint32_t countdown3 = defTime5.toFloat() - (currentMillis - timer3);
      uint8_t H3 = countdown3 / 3600;
      uint8_t M3 = countdown3 / 60 % 60;
      uint8_t S3 = countdown3 % 60;
      char buffer3[9];
      sprintf(buffer3, "%02d:%02d:%02d", H3, M3, S3);
      ws.textAll("buffer3=" + String(buffer3));
    }
    else if (ledState5 && Auto == "true")
    {
      ws.textAll("buffer3=" + String("ON"));
    }
    previousMillis2 = currentMillis;
  }

  // uptime -----
  if (currentMillis - previousMillis3 >= interval3)
  {
    uint8_t D = currentMillis / 3600 / 24;       // дни
    uint8_t H = currentMillis / 3600 % 24;       // часы
    uint8_t M = currentMillis / 60 % 60;         // минуты
    char uptime[10];                             // кол-во знаков
    sprintf(uptime, "%02d  %02d:%02d", D, H, M); // в буфер формат чч:мм:cc
    ws.textAll("uptime=" + String(uptime));
    // Serial.println(uptime); // в сериал порт
    previousMillis3 = currentMillis;
  }
}