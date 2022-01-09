// Файл термостата. Управление реле по условиям температуры.

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain;charset=utf-8", "Страница не найдена");
}

String defTemp1 = "25.0";   // Верхнее пороговое значение температуры по умолчанию
String defTemp1_1 = "24.0"; // Нижнее пороговое значение температуры по умолчанию
String defTemp2 = "26.0";
String defTemp2_1 = "25.0";
String lastTemp;         // Последние показания температуры, которые будут сравниваться с пороговыми значениеми.

String Flag = "checked"; // Переменная "Flag" сообщает нам, установлен ли флажок режима "Авто" или нет.
String Auto = "true";    // Если флажок установлен, переменная "Auto" примет значение true.

// Создадим переменные, чтобы отслеживать, были ли активированы триггеры или нет
bool triggerActive1 = false;
bool triggerActive2 = false;

// Следующие переменные будут использоваться для проверки того, получили ли мы HTTP-запрос
// GET из этих полей ввода, и сохранения значений в переменные соответственно.
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
const long interval = 5000;
uint32_t previousMillis = 0;

// void setup_3()
// {  }

void loop_3()
{
 // Считываем с датчика показания температуры каждые 5 секунд.
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
   float IN4 = bme.readTemperature()-1.04; //////////
  // const int IN4 = bme.readTemperature()-1.04;
       Serial.print(IN4);  ///////////////
       Serial.println(" *C"); ////////////   

// Верхняя форточка - порог открытия
    if (IN4 > defTemp1.toFloat() && Auto == "true" && !triggerActive1) {
      ws.textAll(String(!ledState1)); // отправляем статус кнопки "ON"
      String message = String("Верх - t° < порога. Текущая температура: ") + String(IN4) + String("C");
      Serial.println(message);
      triggerActive1 = true;
      digitalWrite(ledPin1, HIGH); }   // по дефолту HIGH
// Порог закрытия
    if (IN4 < defTemp1_1.toFloat() && Auto == "true" && triggerActive1) {
    ws.textAll(String(ledState1));  // отправляем статус кнопки "OFF"
          String message = String("Верх - t° < порога. Текущая температура: ") + String(IN4) + String("C");
    Serial.println(message);
    triggerActive1 = false;
      digitalWrite(ledPin1, LOW); }  // по дефолту LOW
	
// Нижняя форточка - порог открытия
    if (IN4 > defTemp2.toFloat() && Auto == "true" && !triggerActive2) {
      ws.textAll(String(!ledState2+2));
      String message = String("Низ - t° > порога. Текущая температура: ") + String(IN4) + String("C");
    Serial.println(message);
    triggerActive2 = true;
      digitalWrite(ledPin2, HIGH); }  // по дефолту HIGH
// Порог закрытия
    if (IN4 < defTemp2_1.toFloat() && Auto == "true" && triggerActive2) {
    ws.textAll(String(ledState2+2));
    String message = String("Низ - t° < порога. Текущая температура: ") + String(IN4) + String("C");
    Serial.println(message);
    triggerActive2 = false;
      digitalWrite(ledPin2, LOW); }   // по дефолту LOW
  }
}