  // Выводим в последовательный порт время работы скетча
  
  void loop() {
  {
    int Loadtime = millis() / 1000;
    // Считаем целые часы
    if (Loadtime / 60 / 60 < 10)
    {
      Serial.print("0");
    }
    Serial.print(Loadtime / 60 / 60);
    Serial.print(":");
    // Считаем минуты
    if (Loadtime / 60 % 60 < 10)
    {
      Serial.print("0");
    }
    Serial.print((Loadtime / 60) % 60);
    Serial.print(":");
    // Считаем секунды
    if (Loadtime % 60 < 10)
    {
      Serial.print("0");
    }
    Serial.println(Loadtime % 60);
  }
}