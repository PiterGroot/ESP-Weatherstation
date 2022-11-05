  
  //time
  #include <NTPClient.h>
  #include <WiFiUdp.h>\

  class Data{
    public:
      int time;
      float temperature;
      float humidity;
      Data(int _time, float _temp, float _humid){
        time = _time;
        temperature = _temp;
        humidity = _humid;
      }
  };

  char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);
  
  const int dataPointLength = 1440;
  int currentDataPoint = 0;
  
  Data *stationData[dataPointLength] = {};

  //eeprom
  #include <EEPROM.h>

  //wifi config
  #include "config.h"

  //LiquidCrystal lcd
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, 16, 2); 
  //lcd.init(); // initialize the lcd
  //lcd.backlight();
  
  //DHT weather sensor
  #include "DHT.h"
  #define DHTPIN D4    
  #define DHTTYPE DHT11   
  
  //ESP8266 Wifi webserver
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  
  bool startServerOnAwake = true;
  
  //time loop
  unsigned long previousMillis = 0;
  const long sensorInterval = 1000;

    //time loop
  unsigned long previousDataIntervalTime = 0;
  const long dataPointInterval = 60000;

  //led indicator 
  const int greenLedPin =  D3;
  const int redLedPin =  D7;
  const int blueLedPin =  D8;
  int blueLedState = LOW;
  
  //rst switch
  int lastState = HIGH; 
  int resetState;
  const int rstButton = D5;
  const int rstPin = D6;
  
  DHT dht(DHTPIN, DHTTYPE);
  ESP8266WebServer server(80);
  float h, t, f;
  
  void setup() {
    Serial.begin(115200);
    InitializeComponents();
  }
  
  void InitializeComponents(){
    dht.begin();
    pinMode(rstButton, INPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(redLedPin, OUTPUT);
    pinMode(blueLedPin, OUTPUT);

    if (!startServerOnAwake) return; 
    StartServer();                           
  }
  
  void StartServer(){
    int failCounter = 0;
    int failTreshold = 10 ;
    
    WiFi.begin(ssid, password);  //Connect to the WiFi network
    while (WiFi.status() != WL_CONNECTED) {  //Wait for connection
      failCounter++;
      ToggleBlueLed();
      if(failCounter >= failTreshold) digitalWrite(redLedPin, HIGH);
      Serial.println("Waiting to connect...");
      delay(500);
    }
    
    digitalWrite(blueLedPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    Serial.print("\nWebserver hosted on IP address: ");
    Serial.println(WiFi.localIP());
    
    server.on("/", OnLoadClient); //Handle Index page
    server.on("/getData", OnGetData); //Handle Index page
    
    server.onNotFound(HandleNotFound);
  
    server.begin(); //Start the server
    Serial.println("Server listening \n");
  }
  
  void OnGetData(){
    server.send(200, "application/json", "{\"temperature\": " + String(t) + ", \"humidity\": " + String(h) + "}");
  }
  
  void OnLoadClient() {
    server.send(200, "text/html", SendSensorDataHTML(t, h));
  }
  
  void HandleNotFound() {
    server.send(404, "text/plain", "Not found");
  }
  
  void HandleWeatherData(){
    h = dht.readHumidity();
    t = dht.readTemperature();
    f = dht.readTemperature(true);
  
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
  }

  void loop() {
    server.handleClient(); //Handling of incoming client requests 
    timeClient.update();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= sensorInterval) {
      previousMillis = currentMillis;
      HandleWeatherData();
    }
    if (currentMillis - previousDataIntervalTime >= dataPointInterval) {
      previousDataIntervalTime = currentMillis;
      AddDataPoint(timeClient.getEpochTime());
    }
    //reset button logic
    resetState = digitalRead(rstButton);
    if(lastState == LOW && resetState == HIGH)
      pinMode(rstPin, HIGH);

    // save the last state
    lastState = resetState;
  }
  void AddDataPoint(int time){
      if(currentDataPoint >= dataPointLength) currentDataPoint = 0;
      
      stationData[currentDataPoint] = new Data(time, t, h);
      currentDataPoint++;
      // for(int i = 0; i < dataPointLength; i++){
      //   Serial.println(stationData[i]->temperature);
      // }
  }

  bool ledState = false;
  void ToggleBlueLed(){
    if(!ledState) {
      digitalWrite(blueLedPin, HIGH);
    }
    else{
      digitalWrite(blueLedPin, LOW);
    }
    ledState = !ledState;
  }
  
  String SendSensorDataHTML(float TemperatureWeb, float HumidityWeb ) {
    String ptr = "<!DOCTYPE html> <html>\n";
    ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    ptr +="<title>ESP8266 Weather Report</title>\n";
    ptr +="<style>html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
    ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
    ptr +="</style>\n";
    ptr +="<script>\n";
    ptr += "setInterval( function() {\n";
    ptr += "fetch(\"/getData\").then(function(response) {return response.json();}).then(function(data) {document.getElementById(\"temp\").innerHTML = \"Temperature: \" + data.temperature + \"*C\"; document.getElementById(\"humid\").innerHTML = \"Humidity: \" + data.humidity + \"%\";}).catch(function() {console.log(\"Failed to fetch weather data!\");});\n";
    ptr += "}, 1000);\n";
    ptr +="</script>\n";
    ptr +="</head>\n";
    ptr +="<body>\n";
    ptr +="<div id=\"webpage\">\n";
    ptr +="<h1>ESP8266 Weather Station  @PiterGroot</h1>\n";
    ptr +="<p id=\"temp\">Temperature: ";
    ptr += TemperatureWeb;
    ptr +="*C</p>";
    ptr +="<p id=\"humid\">Humidity: ";
    ptr +=(int)HumidityWeb;
    ptr +="</div>\n";
    ptr +="</body>\n";
    ptr +="</html>\n";
    return ptr;
  }
