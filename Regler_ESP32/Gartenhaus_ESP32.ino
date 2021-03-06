//-------------Einbinden seperater Bibilotheken-------------------------
#include <Stepper.h> //Bibilothek für den Schrittmotor
#include <DHT.h> //Bibilothek für den Sensor DHT11
#include <WiFi.h> //Bibilothek für die WIFI Schnittstelle
#include <Preferences.h> //Bibilothek für den Zwischenspeicher
//--------------Deffinition von Ports----------------------
#define LightSensor 34
#define GroundHumidSensor 35
#define TempHumidSensor 17
#define pump 32
#define heater 33
#define cooler 25
#define sprayer 26
#define uvLight 27
#define workingLED 14
#define DHTTYPE DHT11
#define STEPS 2048

//--------------------------initalisieren-------------------
const int leng = 20;
const char* ssid     = "VTHHH";
const char* password = "v19t25h16h06h11";
const char* host = "192.168.178.26";
const int serverPort = 5000, max_status_shutters = 13312;
WiFiClient client, c1;
WiFiServer server(serverPort);
bool IsActive = false, live = false;
int sendcounter = 0, tempI = 0, humidI = 0, groundHumidI = 0, lightI = 0;
float temp[leng], humid[leng], groundHumid[leng], light[leng];
Stepper Motor(STEPS, 12, 13, 4, 15);
DHT dht(TempHumidSensor, DHTTYPE);
Preferences preferences; 
int higherPin[] {
  heater, sprayer, pump, uvLight
},
lowerPin[] {
  cooler, 0, 0, 0
}, high[] {
  0, 0, 0, 0
}, low[] {
  0, 0, 0, 0
};

//---------------------------------setup----------------------------------------
void setup() {
  //Initalisierung weiterer Werte
  Motor.setSpeed(5);
  Serial.begin(9600);
  for (int f = 0; f < leng; f++) {
    temp[f] = -100;
    humid[f] = -100;
    groundHumid[f] = -100;
    light[f] = -100;
  }

  //Definition der Ports ob INPUT oder OUTPUT
  pinMode(LightSensor, INPUT);
  pinMode(GroundHumidSensor, INPUT);
  pinMode(TempHumidSensor, INPUT);
  pinMode(pump, OUTPUT);
  pinMode(heater, OUTPUT);
  pinMode(cooler, OUTPUT);
  pinMode(sprayer, OUTPUT);
  pinMode(uvLight, OUTPUT);
  pinMode(workingLED, OUTPUT);
  digitalWrite(higherPin[0], LOW);
  digitalWrite(higherPin[1], LOW);
  digitalWrite(higherPin[2], LOW);
  digitalWrite(higherPin[3], LOW);
  digitalWrite(lowerPin[0], LOW);
  high[0] = 0;
  high[1] = 0;
  high[2] = 0;
  high[3] = 0;
  low[0] = 0;
  low[3] = 0;
  dht.begin();

  Serial.println("----------Start-------------");
  Connecting(); //Verbindungsaufbau zum lokalen Netzwerk

  Serial.println("\n\nStarting Server");
  server.begin(); //Server starten
  Serial.print("Server gestartet unter Port ");
  Serial.println(serverPort);

  //eindeutige Regler-ID aus dem Speicher holen
  Serial.println("\n\nCheck ID");
  preferences.begin("storage", true);
  int id = preferences.getUInt("id", 0); 
  //preferences.putUInt("shutters", 13312);
  preferences.end();
  Serial.println("Checking");
  Serial.println(id);

//Wenn vorhanden, am Server reconnecten, falls nicht neu connecten
  if (id < 1) {
    Serial.println("new");
    SendMessage("new arduino", false);
  }
  else {
    Serial.println("reconect");
    SendMessage("reconect arduino_" + String(id), false);
  }
}

//-------------------------------------------------loop----------------------------------------
void loop() {
  if (live) {
    digitalWrite(workingLED,LOW);
    LiveLoop();
  }
  else {
    NonLiveLoop();

  }
  //Einlesen der Werte und speichern der letzten 20
  if (tempI++ > leng) {
    tempI = 0;
  }
  if (humidI++ > leng) {
    humidI = 0;
  }
  if (groundHumidI++ > leng) {
    groundHumidI = 0;
  }
  if (lightI++ > leng) {
    lightI = 0;
  }
  temp[tempI] = dht.readTemperature();
  humid[humidI] = dht.readHumidity();
  groundHumid[groundHumidI] = analogRead(GroundHumidSensor);
  light[lightI] = analogRead(LightSensor);

  //Wenn dem Regler eine Pflanze zugeordnet ist, werden alls 500ms die eingelesenen Werte an die Datenbank gesendet
  if (IsActive) {
    Serial.print("-------------------------------------------------------------");
    Serial.println(sendcounter);
    if (sendcounter++ > 500) {
      sendcounter = 0;
      preferences.begin("storage", true);
      int id = preferences.getUInt("id", 0);
      preferences.end();
      String messag = "" + String(id) + "_" + String(GetAverage(temp)) + "_" + GetAverage(humid) + "_" + GetAverage(groundHumid) + "_" + ((int)GetAverage(light));
      messag.replace("nan", "-100") ;
      SendMessage(messag, true);
    }
  }
  Shutters_Stepper();
}
//---------------------------------normal Loop---------------------------------
void NonLiveLoop() { 
  if (IsActive) {
    digitalWrite(workingLED,HIGH);
    Check();
  }
  else{
    digitalWrite(workingLED,LOW);
  }
  GetMessage();
}


//-------------------------Duschnittswert eines float Arrays---------------------
float GetAverage(float array_[leng]) {
  float help = 0;
  int help2 = 0;
  for (int f = 0; f < leng; f++) {
    if (array_[f] != -100) {
      help += array_[f];
      help2++;
    }
  }
  if (help2 == 0) {
    return 0;
  }
  return help / help2;
}

//---------------------------------Regelung----------------------------------
void Check() {
  float data[3] = {GetAverage(temp), GetAverage(humid), GetAverage(groundHumid)};
  //Einlesen der Soll-Werte
  preferences.begin("storage", true);
  float Min[3] = {preferences.getFloat("MinTemp", -100) , preferences.getFloat("MinHumid", -100), preferences.getFloat("MinGroundHumid", -100)};
  float Max[3] = {preferences.getFloat("MaxTemp", -100) , preferences.getFloat("MaxHumid", -100) , preferences.getFloat("MaxGroundHumid", -100)};
  preferences.end();
  //Mittelwert berechnen
  float middle;
  for (int f = 0; f < 3; f++) {
    middle = (Min[f] + Max[f]) / 2;
    switch (f) {
      case 0:    Serial.print("Vergleich Temperatur "); break;
      case 1:    Serial.print("Vergleich Luftfeuchtigkeit "); break;
      case 2:    Serial.print("Vergleich Bodenfeuchtigkeit "); break;
    }
    Serial.print(": min-");
    Serial.print(Min[f]);
    Serial.print(" max-");
    Serial.print(Max[f]);
    Serial.print(" middle-");
    Serial.print(middle);
    Serial.print(" value-");
    Serial.println(data[f]);
    //Aktoren ansteuern
    if (data[f] < Min[f]) { //Unter Min
      digitalWrite(higherPin[f], HIGH);
      high[f] = 1;
    }
    else if (data[f] > Max[f] && f != 2 && f != 1) { //Über Max
      digitalWrite(lowerPin[f], HIGH);
      low[f] = 1;
    }
    else if (data[f] > middle) {
      digitalWrite(higherPin[f], LOW);
      high[f] = 0;
    }
    else if (data[f] < middle) {
      digitalWrite(lowerPin[f], LOW);
      low[f] = 0;
    }
  }
  //Regelung Lichtstärke
  preferences.begin("storage", true);
  int x = preferences.getUInt("Light", -100), counter = 0;
  Serial.print("Vergleich Licht");
  Serial.print(": wert-");
  Serial.print(x);
  Serial.print(" value-");
  for (int f = 0; f < x && f < leng; f++) {
    if (light[leng - f] == -100) {
      x++;
    }
    else if (light[leng - f] > 1000) {
      counter++;
    }
  }
  Serial.println(counter);
  if (counter == preferences.getUInt("Light", -100)) {
    low[3] = 1;
  }
  else if (counter == 0 ) {
    digitalWrite(higherPin[3], HIGH);
    high[3] = 1;
  }
  else if ( counter < 3) {
    low[3] = 0;
  }
  else if ( counter > 1) {
    digitalWrite(higherPin[3], LOW);
    high[3] = 0;
  }
  preferences.end();
}

//--------------------------------------------Mit lokalem Netzwerk verbinden-----------------------------------------
void Connecting() {
  Serial.print("\n\nConnecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

//solange versuchen bis Verbunden
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
}

//------------------------------------------Empfangen der Nachricht vom Server---------------------------------------
void GetMessage() {
  String message = "";
  WiFiClient client_ = server.available(); //Warten auf Verbindung
  if (client_) {
    Serial.println("\n\nConnected");
    while (client_.connected()) {
      if (client_.available()) {
        char c = client_.read(); //einlesen des Befehles
        if (c == '|' ) { //Endflag der Kommunikation
          //Auswertung des Befehls
          if (message.indexOf("_") == 0) { 
            Serial.print("Message from Socket: ");
            Serial.println(message);
            live = true;
            byte ip[4];
            ip[0] = message.substring(1, message.indexOf(".")).toInt();
            String ip_help = message.substring(message.indexOf(".") + 1, message.length() + 1);
            Serial.println(ip_help);
            ip[1] = ip_help.substring(0, message.indexOf(".")).toInt();
            ip_help = ip_help.substring(ip_help.indexOf(".") + 1, ip_help.length() + 1);
            Serial.println(ip_help);
            ip[2] = ip_help.substring(0, message.indexOf(".")).toInt();
            ip[3] = ip_help.substring(ip_help.indexOf(".") + 1, ip_help.length() + 1).toInt();
            do {
              c1.connect(ip, 5000);
              Serial.print("Connecting to ");
              Serial.print(ip[0]);
              Serial.print(".");
              Serial.print(ip[1]);
              Serial.print(".");
              Serial.print(ip[2]);
              Serial.print(".");
              Serial.println(ip[3]);
            } while (!c1.connected());
          }
          else {
            Serial.print("Message from Socket: ");
            Serial.println(message);
            SetToSave(message);
          }
          client_.println("Done<EOF>");
          client_.flush();
          client_.stop();

          return;
        }
        else {
          message += c;
        }
      }
    }
  }
}

//-----------------------------------Nachricht an Server senden-----------------------------------------------
String SendMessage(String message, bool data_) {
  if (data_) {
    message = "set data_" + message;
  }
  //Verbindung aufbauen
  host = "192.168.178.26";
  Serial.print("Connecting to ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(serverPort);
  while (!client.connect(host, serverPort)) {
    Serial.print("X");
  }

//Nachricht senden
  Serial.print("Send to server: ");
  Serial.println(message);
  message += "_<EOF>";
  client.println(message); //send
  String erg = client.readStringUntil('|'); //auf Antwort warten
  Serial.print(erg);
  Serial.println("\nClosing connection");
  client.println("Done<EOF>");
  client.flush();
  client.stop();
  Serial.println("Close connection");
  return erg;

}

//---------------------------------------------Daten im Zwischenspeicher hinterlegen-------------------------------
void SetToSave(String save) {
  if (save.substring(save.indexOf("a") + 1, save.indexOf("b")) == "-100") {
    IsActive = false;
    digitalWrite(higherPin[0], LOW);
    digitalWrite(higherPin[1], LOW);
    digitalWrite(higherPin[2], LOW);
    digitalWrite(higherPin[3], LOW);
    digitalWrite(lowerPin[0], LOW);
    high[0] = 0;
    high[1] = 0;
    high[2] = 0;
    high[3] = 0;
    low[0] = 0;
    low[3] = 0;
  }
  else {
    IsActive = true;
  }
  save.replace(",", ".");
  preferences.begin("storage", false);
  preferences.putUInt("id", save.substring(0, save.indexOf("a")).toFloat());
  preferences.putFloat("MinTemp", save.substring(save.indexOf("a") + 1, save.indexOf("b")).toFloat());
  preferences.putFloat("MaxTemp", save.substring(save.indexOf("b") + 1, save.indexOf("c")).toFloat());
  preferences.putFloat("MinGroundHumid", save.substring(save.indexOf("c") + 1, save.indexOf("d")).toFloat());
  preferences.putFloat("MaxGroundHumid", save.substring(save.indexOf("d") + 1, save.indexOf("e")).toFloat());
  preferences.putFloat("MinHumid", save.substring(save.indexOf("e") + 1, save.indexOf("f")).toFloat());
  preferences.putFloat("MaxHumid", save.substring(save.indexOf("f") + 1, save.indexOf("g")).toFloat());
  switch (save.substring(save.indexOf("g") + 1, save.length() + 1).toInt()) {
    case 0:   preferences.putUInt("Light", 6);  break;
    case 1:   preferences.putUInt("Light", 4);  break;
    case 2:   preferences.putUInt("Light", 2);  break;
  }
  preferences.end();
}

//---------------------------------------------------------Live Schleife--------------------------------------------------------
void LiveLoop() {
  //Aktorenstatus an Appliaktion senden
  String onoff = "" + String(high[0]) + String(high[1]) + String(high[2]) + String(high[3]) + String(low[0]) + String(low[3]);
  Serial.println(String(GetAverage(temp)) + "_" + GetAverage(humid) + "_" + GetAverage(groundHumid) + "_" + ((int)GetAverage(light)) + "_" + onoff + "|");
  c1.println(String(GetAverage(temp)) + "_" + GetAverage(humid) + "_" + GetAverage(groundHumid) + "_" + ((int)GetAverage(light)) + "_" + onoff + "|");
  String message = "";
  while (c1.connected()) {
    if (c1.available()) {
      char c = c1.read(); //Nachricht von Applikation auslesen
      if (c == '|' ) {
        if (message == "live off") { //Live Funktion beenden
          live = false;
          c1.println("Done<EOF>");
          c1.flush();
          c1.stop();
          digitalWrite(higherPin[0], LOW);
          digitalWrite(higherPin[1], LOW);
          digitalWrite(higherPin[2], LOW);
          digitalWrite(higherPin[3], LOW);
          digitalWrite(lowerPin[0], LOW);
          high[0] = 0;
          high[1] = 0;
          high[2] = 0;
          high[3] = 0;
          low[0] = 0;
          low[3] = 0;
          return;
        }
        //Steuerungsbefehl auswerten
        Serial.print("Message from Socket: ");
        Serial.println(message);
        char datas[6];
        message.toCharArray(datas, 7);
        for (int f = 0; f < 4; f++) {
          Serial.print(datas[f]);
          if (datas[f] == '1') {
            digitalWrite(higherPin[f], HIGH);
            high[f] = 1;
          }
          else {
            digitalWrite(higherPin[f], LOW);
            high[f] = 0;
          }
        }
        Serial.print(datas[4]);
        if (datas[4] == '1') {
          digitalWrite(lowerPin[0], HIGH);
          low[0] = 1;
        }
        else {
          digitalWrite(lowerPin[0], LOW);
          low[0] = 0;
        }
        Serial.println(datas[5]);
        if (datas[5] == '1') {
          low[3] = 1;
        }
        else {
          low[3] = 0;
        }
        return;
      }
      else {
        message += c;
      }
    }
  }
}

//-------------------------------------Rolladen Steuerung---------------------------------------------
void Shutters_Stepper() {
  preferences.begin("storage", true);
  int status_shutters = preferences.getUInt("shutters", 0);
  preferences.end();
  //Serial.print("status_shutter:");
  //Serial.println(status_shutters);
  if (low[3] == 1 && status_shutters > 0 && status_shutters <= max_status_shutters) {
    if (status_shutters >= 1024) {
      Motor.step(1024);
      status_shutters -= 1024;
      Serial.println("--------------------!1024");
      preferences.begin("storage", false);
      preferences.putUInt("shutters", status_shutters);
      preferences.end();
    } //halbe Umdrehung runterlassen
    else {
      Motor.step(status_shutters);
      status_shutters -= status_shutters;
      Serial.println("--------------------!????");
      preferences.begin("storage", false);
      preferences.putUInt("shutters", status_shutters);
      preferences.end();
    }//runterlassen
  }
  else if (low[3] == 0 && status_shutters >= 0 && status_shutters < max_status_shutters) {
    int h_status_shutters = max_status_shutters - status_shutters;
    if (h_status_shutters >= 1024) {
      Motor.step(-1024);
      status_shutters += 1024;
      Serial.println("--------------------+1024");
      preferences.begin("storage", false);
      preferences.putUInt("shutters", status_shutters);
      preferences.end();
    }//halbe Umdrehung hochziehen
    else {
      Motor.step(-h_status_shutters);
      status_shutters += h_status_shutters;
      Serial.println("--------------------+????");
      preferences.begin("storage", false);
      preferences.putUInt("shutters", status_shutters);
      preferences.end();
    }//hochziehen
  }
}
