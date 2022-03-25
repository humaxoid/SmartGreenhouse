void loopTimer() {

  if (millis() / 3600000L - timer1 >= (ledState3 ? defTime3_1.toFloat() : defTime3.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState3 + 4)); // отправляем статус кнопки "ON"
    timer1 = millis() / 3600000L;       // сброс таймера раз в час
    ledState3 = !ledState3;
    digitalWrite(ledPin3, ledState3);
    // Serial.println(millis() / 1000L);
  }

  if (millis() / 3600000L - timer2 >= (ledState4 ? defTime4_1.toFloat() : defTime4.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState4 + 6)); // отправляем статус кнопки "ON"
    timer2 = millis() / 3600000L;       // сброс таймера раз в час
    ledState4 = !ledState4;
    digitalWrite(ledPin4, ledState4);
  }

  if (millis() / 3600000L - timer3 >= (ledState5 ? defTime5_1.toFloat() : defTime5.toFloat()) && Auto == "true")
  {
    ws.textAll(String(!ledState5 + 8)); // отправляем статус кнопки "ON"
    timer3 = millis() / 3600000L;       // сброс таймера раз в час
    ledState5 = !ledState5;
    digitalWrite(ledPin5, ledState5);
  }
}