// Libraries
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

//Sensor DTH22
#define DHTPIN 13                      
#define DHTTYPE DHT22       
DHT dht(DHTPIN, DHTTYPE);

//Sensor BH1750
BH1750 lightMeter;

//Sensor BME280
Adafruit_BME280 bme;             

//Sensor DS18B20
OneWire ds(15);                   

//Sensor Soil_Moisture_Sensor
int moisture_pin = 36;
int output_value ;

// Network settings
const char* ssid     = "uname";
const char* password = "pass";
IPAddress local_IP(192, 168, 1, 68);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4); 
AsyncWebServer server(80);

// Default Threshold Temperature Value
String inputMessage = "25.0"; 
String lastTemperature;
String enableArmChecked = "checked";
String inputMessage2 = "true";
//---------------------
// Stores LED state
String ledState1;
String ledState2;
String ledState3;
String ledState4;
String ledState5;

// DTH22
String getDHTTemperature() {
 float IN1 = dht.readTemperature();             
  if (isnan(IN1)) {                                         
    Serial.println("Sensor read error DTH22!");
    return "--";
  }
  else {
	 Serial.print("DTH22- temperature: "); 
    Serial.print(IN1);                                     
    Serial.print(" °C ");                                 
    return String(IN1);
  }
}

String getDHTHumidity() {
  float IN2 = dht.readHumidity();                          
  if (isnan(IN2)) {                                       
    Serial.println("Sensor read erro DTH22!");
    return "--";
  }
  else {
   Serial.print("DTH22- Humidity: ");
   Serial.print(IN2);
   Serial.println(" % ");
    return String(IN2);
  }
}

// BH1750
String getLightLevel() {
   float IN3 = lightMeter.readLightLevel();                
   Serial.print("Illumination level: ");                         
   Serial.print(IN3);                                     
   Serial.println(" lux");
   return String(IN3);
}  

// BME280
String getTemperature2() {
  float IN4 = bme.readTemperature()-1.04;
  Serial.print("BME280- temperature: ");
  Serial.print(IN4);
  Serial.print(F(" °C ")); 
  return String(IN4);
}

String getPressure() {
  float IN5 = bme.readPressure()/133.3;
  Serial.print(" pressure: ");
  Serial.print(IN5);
  Serial.print(" мм.рт.ст ");
  return String(IN5);
}
  
String getHumidity() {
  float IN6 = bme.readHumidity();
  Serial.print(" Humidity: ");
  Serial.print(IN6);
  Serial.println(" %");
  return String(IN6);
}

// DS18B20
String gettemperature3() {
byte data[2];    
ds.reset();       
ds.write(0xCC); 
ds.write(0x44);
delay(1000);      
ds.reset();      
ds.write(0xCC); 
ds.write(0xBE);   

data[0] = ds.read(); 
data[1] = ds.read();  

float temperature =  ((data[1] << 8) | data[0]) * 0.0625;
  
float IN7 = ((temperature)+2.3);                        
Serial.print("DS18B20- Soil temperature: ");           
Serial.print(IN7);                                      
Serial.println(" °C");                                
return String(IN7);
}

//Soil Moisture Sensor
String getoutput_value() {
output_value = analogRead(moisture_pin);
output_value = map(output_value, 4090, 2900, 0, 100);
float IN8 = (output_value);                            
Serial.print("Soil moisture content: ");
Serial.print(IN8);                                     
Serial.println("%");
Serial.println();
//if (IN8>=50) digitalWrite(33, HIGH);                  
//else digitalWrite(33, LOW);                           
return String(IN8);
}
//-----------------------

String processor(const String& var){
  Serial.println(var);
 
  if(var == "STATE1"){
    if(digitalRead(32)){
      ledState1 = "To open";
    }
    else{
      ledState1 = "To close";
    }
    Serial.println(ledState1);
    return ledState1;
  }
  
    if(var == "STATE2"){
    if(digitalRead(33)){
      ledState2 = "To open";
    }
    else{
      ledState2 = "To close";
    }
    Serial.println(ledState2);
    return ledState2;
  }
  
  if(var == "STATE3"){
    if(digitalRead(25)){
      ledState3 = "To open";
    }
    else{
      ledState3 = "To close";
    }
    Serial.println(ledState3);
    return ledState3;
  }
  
  if(var == "STATE4"){
    if(digitalRead(26)){
      ledState4 = "To open";
    }
    else{
      ledState4 = "To open";
    }
    Serial.println(ledState4);
    return ledState4;
  }
  
  if(var == "STATE5"){
    if(digitalRead(27)){
      ledState5 = "To open";
    }
    else{
      ledState5 = "To close";
    }
    Serial.println(ledState5);
    return ledState5;
  }
  
 // =================== 
  else if(var == "DATA1"){
    return getDHTTemperature();
  }
  else if(var == "DATA2"){
    return getDHTHumidity();
  }
  else if(var == "DATA3"){
    return getLightLevel();
  }

  else if (var == "DATA4"){
    return getTemperature2();
  }
    else if (var == "DATA5"){
    return getPressure();
  } 
  else if (var == "DATA6"){
    return getHumidity();
  }
    else if (var == "DATA7"){
    return gettemperature3();
  }
      else if (var == "DATA8"){
    return getoutput_value();
  } 
}
 
void setup(){
  Serial.begin(115200);   
  
pinMode(32, OUTPUT);      
digitalWrite(32, LOW);
pinMode(33, OUTPUT);
digitalWrite(33, LOW); 
pinMode(25, OUTPUT);
digitalWrite(25, LOW);
pinMode(26, OUTPUT);
digitalWrite(26, LOW);
pinMode(27, OUTPUT);
digitalWrite(27, LOW);
//pinMode(14, OUTPUT); 
//digitalWrite(14, LOW);
  
  //DTH22
Serial.println(F("Running sensor DHT22..."));
dht.begin();

// Sensor BH1750
Wire.begin();
lightMeter.begin();
Serial.println(F("Running sensor BH1750..."));

  // Initializing the sensor BME280
  if (!bme.begin(0x76)) {
	Serial.println("The BME280 sensor is not detected, check the connection!");
    while (1);
 }

//Sensor Soil_Moisture_Sensor
Serial.println("Running Soil Moisture Sensor... ");

// Static IP:
if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
Serial.println("Client mode could not be configured");
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
 
  // Show my ip.
Serial.println("");
Serial.println(" WiFi connected!");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());

//=============== Sending page to browser ==================

  // root 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // style.css
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  
  // pictures
  server.on("/teplica.png", HTTP_GET, [](AsyncWebServerRequest * request){
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
  
  //=========== Sending the sensors readings to the browser =============
  
  server.on("/IN1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getDHTTemperature().c_str());
  });
  
  server.on("/IN2", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getDHTHumidity().c_str());
  });
  
    server.on("/IN3", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getLightLevel().c_str());
  });
  
    server.on("/IN4", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getTemperature2().c_str());
  });
  
  server.on("/IN5", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getPressure().c_str());
  });
  
    server.on("/IN6", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getHumidity().c_str());
  });
  
     server.on("/IN7", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", gettemperature3().c_str());
  });
  
       server.on("/IN8", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getoutput_value().c_str());
  });
  
// ================ Transferring button control ==================

  server.on("/on1", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(32, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/off1", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(32, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/on2", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(33, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/off2", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(33, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/on3", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(25, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/off3", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(25, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/on4", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(26, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/off4", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(26, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/on5", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(27, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/off5", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(27, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Start web server
  server.begin();
  Serial.println("Web server started!");
}
 
void loop(){
  
}