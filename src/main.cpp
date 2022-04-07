
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncElegantOTA.h>

#include <sensors.h>
//#include <ntp.h>

// Задаем сетевые настройки

const char *ssid = "uname";
const char *password = "pass";

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

String defTemp1 = "25.0";   // Верхнее пороговое значение температуры по умолчанию
String defTemp1_1 = "24.0"; // Нижнее пороговое значение температуры по умолчанию
String defTemp2 = "26.5";
String defTemp2_1 = "25.5";
String lastTemp; // Последние показания температуры, которые будут сравниваться с пороговыми значениеми.

String defTime3 = "8";   // Периодичность включения таймера по умолчанию
String defTime3_1 = "4"; // Длительность работы таймера по умолчанию
String defTime4 = "9";
String defTime4_1 = "3";
String defTime5 = "10";  // Периодичность включения таймера по умолчанию (в часах)
String defTime5_1 = "2";

String Flag = "checked"; // Переменная "Flag" сообщает нам, установлен ли флажок режима "Авто" или нет.
String Auto = "true";    // Если флажок установлен, переменная "Auto" примет значение true.

// Локальные переменные для отслеживания, были ли активированы триггеры или нет.
bool triggerActive1 = false;
bool triggerActive2 = false;
bool triggerActive3 = false;

// Вычисляем разницу для получения полного периода
// float difference1 = (defTime3.toFloat() - defTime3_1.toFloat());
// float difference2 = (defTime4 - defTime4_1);
// float difference3 = (defTime5 - defTime5_1);

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

// Интервал между показаниями датчиков.
uint32_t previousMillis = 0;
const long interval = 5000;

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
  case WS_EVT_CONNECT: // когда клиент вошел в систему
    Serial.printf("WebSocket клиент #%u подключен с %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT: // когда клиент вышел из системы
    Serial.printf("WebSocket клиент #%u отключен\n", client->id());
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

// =========== Конертируем время ================

String timeLeft1() // Отправляем остаток времени по таймеру №1
{
  uint32_t sec = defTime3.toFloat() * 3600ul - millis() / 1000ul; // остаток в секундах
  uint8_t H = (sec / 3600);                                       // часы
  uint8_t M = (sec % 3600) / 60;                                  // минуты
  uint8_t S = (sec % 3600) % 60;                                  // секунды

  char szStr[9];
  sprintf(szStr, "%02d:%02d:%02d", H, M, S);
  Serial.println(szStr);
  if (ledState3) // если таймер включен
  {
    return "ON"; // отправить статус "ON"
  }
  return String(szStr);
}

String timeLeft2() // Отправляем остаток времени по таймеру №2
{
  uint32_t sec = defTime4.toFloat() * 3600ul - millis() / 1000ul; // остаток в секундах
  uint8_t H = (sec / 3600);                                       // часы
  uint8_t M = (sec % 3600) / 60;                                  // минуты
  uint8_t S = (sec % 3600) % 60;                                  // секунды

  char szStr[9];
  sprintf(szStr, "%02d:%02d:%02d", H, M, S);

  if (ledState4) // если таймер включен
  {
    return "ON"; // отправить статус "ON"
  }
  return String(szStr);
}

String timeLeft3() // Отправляем остаток времени по таймеру №3
{
  uint32_t sec = defTime5.toFloat() * 3600ul - millis() / 1000ul; // остаток в секундах
  uint8_t H = (sec / 3600);                                       // часы
  uint8_t M = (sec % 3600) / 60;                                  // минуты
  uint8_t S = (sec % 3600) % 60;                                  // секунды

  char szStr[9];
  sprintf(szStr, "%02d:%02d:%02d", H, M, S);

  if (ledState5) // если таймер включен
  {
    return "ON"; // отправить статус "ON"
  }
  return String(szStr);
}

// ----------------------------------------------------------------
// Инициализация
// ----------------------------------------------------------------

void initSPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("При монтировании SPIFFS произошла ошибка");
    while (1)
      ;
  }
}

void initWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Подключаемся к WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
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

  initDHT22(); // Инициализация датчика HDT22, в файле sensors.h
  // initBH1750();     // Инициализация датчика BH1750, в файле sensors.h
  initBME280();    // Инициализация датчика BME280, в файле sensors.h
  initSPIFFS();    // Инициализация SPIFFS
  initWiFi();      // Инициализация WiFi
  initWebServer(); // Инициализация Web сервера
  initWebSocket(); // Инициализация Websocket
  // printLocalTime(); // Инициализация ntp клиента

  // Объявим GPIO выходы (по умолчанию LOW)
  pinMode(ledPin1, OUTPUT); digitalWrite(ledPin1, LOW);
  pinMode(ledPin2, OUTPUT); digitalWrite(ledPin2, LOW);
  pinMode(ledPin3, OUTPUT); digitalWrite(ledPin3, LOW);
  pinMode(ledPin4, OUTPUT); digitalWrite(ledPin4, LOW);
  pinMode(ledPin5, OUTPUT); digitalWrite(ledPin5, LOW);
  pinMode(ledPin6, OUTPUT); digitalWrite(ledPin6, HIGH); // Вывод светодиодного индикатора режимов Авто/Ручной.

  // setup_1(); // отсылка к void setup() файла ntp.h

  // Маршрут до корневого каталога веб страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html", false, processor); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/script.js", "text/javascript"); });

  server.on("/teplica.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/teplica.png", "image/png"); });

  server.on("/sungif.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/sungif.gif", "image/gif"); });

  server.on("/baklazan.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/baklazan.gif", "image/gif"); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/favicon.ico", "image/ico"); });

  server.on("/casa.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/casa.png", "image/png"); });

  server.on("/flame.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/flame.png", "image/png"); });

  server.on("/humid.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/humid.png", "image/png"); });

  server.on("/sun.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/sun.png", "image/png"); });

  server.on("/temp.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/images/temp.png", "image/png"); });

  server.on("/seven-segment.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/seven-segment.ttf", "application/octet-stream"); });

  //============= Отправляем в браузер показания датчиков =============
  server.on("/IN1", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDHTTemperature().c_str()); });

  server.on("/IN2", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDHTHumidity().c_str()); });

  // server.on("/IN3", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send_P(200, "text/plain", getLightLevel().c_str()); });

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

  // //============= Отправляем остаток времени до включения таймеров =============

  server.on("/time_Left1", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", timeLeft1().c_str()); });

  server.on("/time_Left2", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", timeLeft2().c_str()); });

  server.on("/time_Left3", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", timeLeft3().c_str()); });

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
                defTime3 = request ->getParam(PARAM_INPUT_3)->value();     // Таймер 1
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
  AsyncElegantOTA.loop();
  loopSensors(); // запуск loop в файле sensors.h

  // Проверяем, если галка снята (ручной режим), то
  if (Auto == "false") digitalWrite(ledPin1, ledState1);
  if (Auto == "false") digitalWrite(ledPin2, ledState2);
  if (Auto == "false") digitalWrite(ledPin3, ledState3);
  if (Auto == "false") digitalWrite(ledPin4, ledState4);
  if (Auto == "false") digitalWrite(ledPin5, ledState5);

  // Считываем с датчика показания температуры каждые 5 секунд.
  uint32_t currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    float IN4 = bme.readTemperature() - 1.04; // Назначим локальную переменную IN4, для датчика температуры.
    uint8_t IN8 = (output_value);             // Назначим локальную переменную IN9, для датчика дождя.
    // Serial.print(IN4);
    // Serial.println(" *C");

    // Если пошел дождь, меняем анимацию на страничке.
    if ((IN8 > 50) && !triggerActive3)
    {
      ws.textAll(String(!ledState6 + 10));
      // String message = String("Пошел дождь ") + String(IN8);
      // Serial.println(message);
      triggerActive3 = true;
    }
    if ((IN8 < 50) && triggerActive3)
    {
      ws.textAll(String(ledState6 + 10));
      // String message = String("Дождь прекратился ") + String(IN8);
      // Serial.println(message);
      triggerActive3 = false;
    }

    // Верхняя форточка - порог открытия
    // if ((IN4 > defTemp1.toFloat() && (IN8 < 50)) && Auto == "true" && !triggerActive1)
    if ((IN4 > defTemp1.toFloat()) && Auto == "true" && !triggerActive1)
    {
      ws.textAll(String(!ledState1)); // отправляем статус кнопки "ON"
      // String message = String("Верхняя - t° выше порога. Текущая: ") + String("IN4") + String("C");
      // Serial.println(message);
      triggerActive1 = true;
      digitalWrite(ledPin1, HIGH);
    }

    // Порог закрытия
    // if (((IN4 < defTemp1_1.toFloat()) || (IN8 > 50)) && Auto == "true" && triggerActive1)
    if ((IN4 < defTemp1_1.toFloat()) && Auto == "true" && triggerActive1)
    {
      ws.textAll(String(ledState1)); // отправляем статус кнопки "OFF"
      // String message = String("Верхняя - t° ниже порога. Текущая: ") + String("IN4") + String("C");
      // Serial.println(message);
      triggerActive1 = false;
      digitalWrite(ledPin1, LOW);
    }

    // Нижняя форточка - порог открытия
    if (IN4 > defTemp2.toFloat() && Auto == "true" && !triggerActive2)
    {
      ws.textAll(String(!ledState2 + 2));
      // String message = String("Низ - t° > порога. Текущая температура: ") + String(IN4) + String("C");
      // Serial.println(message);
      triggerActive2 = true;
      digitalWrite(ledPin2, HIGH);
    }
    // Порог закрытия
    if (IN4 < defTemp2_1.toFloat() && Auto == "true" && triggerActive2)
    {
      ws.textAll(String(ledState2 + 2));
      // String message = String("Низ - t° < порога. Текущая температура: ") + String(IN4) + String("C");
      // Serial.println(message);
      triggerActive2 = false;
      digitalWrite(ledPin2, LOW);
    }
  }

  // #########################  Дальше раздел таймеров полива ######################################

  if (millis() / 3600000L - timer1 >= (ledState3 ? defTime3_1.toFloat() : defTime3.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState3 + 4)); // отправляем статус кнопки "ON"
    timer1 = millis() / 3600000L;       // количество секунд с момента старта
    ledState3 = !ledState3;
    digitalWrite(ledPin3, ledState3);   // подаем на GPIO25, высокий/низкий уровень
  }

  // ######## 2 #########
  if (millis() / 3600000L - timer2 >= (ledState4 ? defTime4_1.toFloat() : defTime4.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState4 + 6));
    timer2 = millis() / 3600000L;
    ledState4 = !ledState4;
    digitalWrite(ledPin4, ledState4);
  }

  //######### 3 #########
  if (millis() / 3600000L - timer3 >= (ledState5 ? defTime5_1.toFloat() : defTime5.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState5 + 8));
    timer3 = millis() / 3600000L;
    ledState5 = !ledState5;
    digitalWrite(ledPin5, ledState5);
  }
}
