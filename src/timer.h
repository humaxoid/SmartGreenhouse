// Таймер полива

// Переменные хранения времени (unsigned long)
uint32_t timer1, timer2, timer3;

// На всех выводах GPIO по умолчанию устанавливаем 1.
//bool ledState3 = true, ledState4 = true, ledState5 = true;

String defTime3 = "20000";    // Периодичность включения таймера по умолчанию #################################
String defTime3_1 = "1000";   // Длительность работы таймера по умолчанию     #################################
String defTime4 = "20000";    // Периодичность включения таймера по умолчанию #################################
String defTime4_1 = "1000";   // Длительность работы таймера по умолчанию     #################################
String defTime5 = "20000";    // Периодичность включения таймера по умолчанию ########################################
String defTime5_1 = "1000";   // Длительность работы таймера по умолчанию     #################################


// Время работы милисек.
//#define work_time1 3000
//#define work_time2 5000
//#define work_time3 6000

// Сюда забиваем периодичность включения таймера (милисек.)
//#define period1 10000
//#define period2 10000
//#define period3 10000

// Вычисляем разницу для получения полного периода
//float time1 = (period1 - work_time1);
//float time2 = (period2 - work_time2);
//float time3 = (period3 - work_time3);

void loop_4()
{
    if (millis() - timer1 >= (ledState3 ? defTime3_1.toFloat() : defTime3.toFloat()) && Auto == "true")
{
  ws.textAll(String(!ledState3 + 4)); // отправляем статус кнопки "ON"
  timer1 = millis();                  // сброс таймера
  ledState3 = !ledState3;
  //triggerActive3 = true;
  digitalWrite(ledPin3, ledState3);
  //Serial.println(millis() / 1000L);
}
    if (millis() - timer2 >= (ledState4 ? defTime4_1.toFloat() : defTime4.toFloat()) && Auto == "true")
{
  ws.textAll(String(!ledState4 + 6));
  timer1 = millis();
  ledState4 = !ledState4;
  digitalWrite(ledPin4, ledState4);
}
    if (millis() - timer3 >= (ledState5 ? defTime5_1.toFloat() : defTime5.toFloat()) && Auto == "true")
{
  ws.textAll(String(!ledState5 + 8));
  timer1 = millis();
  ledState5 = !ledState5;
  digitalWrite(ledPin5, ledState5);
}
}