// Тут код для контроля кнопок на Websocket

// На всех выводах GPIO по умолчанию устанавливаем 0.
bool ledState1 = false, ledState2 = false, ledState3 = false, ledState4 = false, ledState5 = false;

// Объявляем переменные для выходов.
const int ledPin1 = 32; // Верхняя форточка
const int ledPin2 = 33; // Нижняя форточка
const int ledPin3 = 25; // Таймер грядки-1
const int ledPin4 = 26; // Таймер грядки-2
const int ledPin5 = 27; // Таймер грядки-3
const int ledPin6 = 2;  // Вывод светодиодного индикатора режимов Авто/Ручной.

AsyncWebServer server(80); // Создаем сервер через 80 порт
AsyncWebSocket ws("/ws");  // Создаем объект WebSocket

// Уведомляем клиентов о текущем состоянии реле
void notifyClients1() { ws.textAll(String(ledState1)); }
void notifyClients2() { ws.textAll(String(ledState2 + 2)); }
void notifyClients3() { ws.textAll(String(ledState3 + 4)); }
void notifyClients4() { ws.textAll(String(ledState4 + 6)); }
void notifyClients5() { ws.textAll(String(ledState5 + 8)); }

/* функция обратного вызова, которая запускается всякий раз, когда мы получаем новые
  данные от клиентов по протоколу WebSocket. Если мы получаем сообщение “toggle”, мы
  переключаем значение переменной ledState. Кроме того, мы уведомляем всех клиентов,
  вызывая функцию notifyClients (). Таким образом, все клиенты получают уведомление об
  изменении и соответствующим образом обновляют интерфейс.*/

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle1") == 0)
    {
      ledState1 = !ledState1;
      notifyClients1();
    }
    if (strcmp((char *)data, "toggle2") == 0)
    {
      ledState2 = !ledState2;
      notifyClients2();
    }
    if (strcmp((char *)data, "toggle3") == 0)
    {
      ledState3 = !ledState3;
      notifyClients3();
    }
    if (strcmp((char *)data, "toggle4") == 0)
    {
      ledState4 = !ledState4;
      notifyClients4();
    }
    if (strcmp((char *)data, "toggle5") == 0)
    {
      ledState5 = !ledState5;
      notifyClients5();
    }
  }
}

// Настройка прослушивания событий для обработки различных асинхронных шагов протокола WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT: // когда клиент вошел в систему
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT: // когда клиент вышел из системы
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA: // при получении пакета данных от клиента
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:  // в ответ на запрос ping
  case WS_EVT_ERROR: // при получении ошибки от клиента
    break;
  }
}

// Инициализация WebSocket
void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup_2()
{
initWebSocket();

  // =========== Отправляем в браузер все наши картинки ==============
  
  server.on("/teplica.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/teplica.png", "image/png"); });

  server.on("/sungif.gif", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/sungif.gif", "image/gif"); });

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
}