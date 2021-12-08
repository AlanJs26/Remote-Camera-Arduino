#include <DNSServer.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>

// Configuração tela
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Configuração do servo motor

#include <ESP32_Servo.h>

static const int servoXPin = 12;
static const int servoYPin = 18;
Servo servoX;
Servo servoY;

// Pinos para o motor de passo
// https://www.curtocircuito.com.br/blog/Categoria%20Arduino/controle-de-motor-de-passo-nema-driver-a4988
// Os pinos ms1 ms2 ms3 controlam o tamanho de cada passo
// ms1 - HIGH
// ms2 - HIGH     para tamanho de 1/8 por passo (total de 1600 passos por revolução)
// ms3 - LOW
// 
// o pino RST precisa estar em HIGH para funcionar
// o pino SLP quando em LOW ativa a função sleep de economia de energia (para o funcionamento normal, o pino precisa estar em HIGH)
// o pino EN precisa estar em LOW para funcionar

static const int directionPin = 25; // HIGH para sentido horário
static const int stepPin = 33;

static const int resetPin = 15;

static const int endSwitchRightPin = 16;
static const int endSwitchLeftPin = 17;


static const int ledRedPin = 26;
static const int ledYellowPin = 27;
static const int ledGreenPin = 14;

int memorySteps = 0;

unsigned long stepMillis = 0;

#define RIGHT true
#define LEFT false

bool currentDirection = RIGHT;
bool activateStepperMotor = true;

WiFiManager wifiManager;

#define API_KEY "AIzaSyAcT5oGP6ZFtJ5sHXsqxVa6DwgwxBzVexw" 
#define DATABASE_URL "remotecamera-6f583-default-rtdb.firebaseio.com"
#define DATABASE_SECRET "zC7bJfF0iGMQ1IPKq5bYj2WOrAx9UF9j9ghzaoTq"

FirebaseData fbdo;
FirebaseData streamConnectedWith;
FirebaseData streamTestAlive;
FirebaseAuth auth;

FirebaseConfig config;
/*FirebaseJson json;*/

String connectedUid;
String connectedName;
String connectedWithPath;
String currentPercentagePath;
String horizontalPercentagePath;
String testAlivePath;
String directionPath;

int currentPercentageX = 50;
int currentPercentageY = 50;
float horizontalPercentage = 0.0;

float horizontalPercentageIncrement = 1.0;

int rightDir = 0;
int leftDir  = 0;
int upDir    = 0;
int downDir  = 0;
int horizontalRightDir = 0;
int horizontalLeftDir  = 0;

unsigned long updateMillis = 0;
unsigned long testAliveMillis = 0;
unsigned long servoMillis = 0;

bool signupOK = false;

void saveConfigCallback() { Serial.println("Configuração salva"); }

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void updateReconnectValidUntil() {
  HTTPClient http;

  String serverPath = "http://api.timezonedb.com/v2.1/"
                      "get-time-zone?key=KGKRF9ISMK4U&format=json&by=zone&zone="
                      "America/Sao_Paulo&fields=timestamp";
  // Your Domain name with URL path or IP address with path
  http.begin(serverPath.c_str());

  // Send HTTP GET request
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.println("\nfething current time online");
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);

    FirebaseJson json;
    FirebaseJsonData result;

    json.setJsonData(payload.c_str());
    json.get(result, "timestamp");

    if(result.success){
      /*result.to<double>()*/
      String uid = auth.token.uid.c_str();
      String validUntilPath = "/users/"+uid+"/reconnectValidUntil";

      Serial.printf("setDouble -- \"%s\": \"%E\"", validUntilPath.c_str(), result.to<double>()+30*60);
      if (Firebase.setDouble(fbdo, validUntilPath.c_str(), result.to<double>()+30*60)) {
        Serial.println(" --> Success");
        Serial.println();
      } else {
        Serial.println(fbdo.errorReason());
      }
    }

  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void gen_random(char *s, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    int randomChar = random(0, 61);
    if (randomChar < 26)
      s[i] = 'a' + randomChar;
    else if (randomChar < 26 + 26)
      s[i] = 'A' + randomChar - 26;
    else
      s[i] = '0' + randomChar - 26 - 26;
  }
  s[len] = 0;
}

bool step(bool dir){
  if((digitalRead(endSwitchLeftPin) && dir == LEFT) || (digitalRead(endSwitchRightPin) && dir == RIGHT)){
    stepMillis = millis();
    return false;
  }

  if(dir){
    digitalWrite(directionPin, HIGH);
  }else{
    digitalWrite(directionPin, LOW);
  }


  if(millis() - stepMillis >= 8){
    digitalWrite(stepPin, LOW);
    stepMillis = millis();
    /* Serial.println("."); */
    return true;
  }

  if(millis() - stepMillis >= 4) digitalWrite(stepPin, HIGH);
  return false;

}

void displayBigText(char* text){
  display.clearDisplay();

  display.setTextSize(4);
  display.setTextColor(WHITE);
  display.setCursor(15, 18);
  // Display static text
  display.println(text);
  display.display(); 
}

#define NUMFLAKES     20 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2

int8_t i, f, icons[NUMFLAKES][3];

void matrix(uint8_t w, uint8_t h, bool isLooping) {

  if(!isLooping){
  // Initialize 'snowflake' positions
  for(f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
    icons[f][YPOS]   = -LOGO_HEIGHT;
    icons[f][DELTAY] = random(6, 15);
    Serial.print(F("x: "));
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(F(" y: "));
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(F(" dy: "));
    Serial.println(icons[f][DELTAY], DEC);
  }
  
  }
  else { // Loop forever...
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.clearDisplay(); // Clear the display buffer

    // Draw each snowflake:
    for(f=0; f< NUMFLAKES; f++) {
      for(i=0; i<f; i++){
        char randChar = random(96, 96+25);
        display.setCursor(icons[f][XPOS], icons[f][YPOS]-i*10);
        display.print(randChar);
      }
    }

    display.display(); // Show the display buffer on the screen

    // Then update coordinates of each flake...
    for(f=0; f< NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
        icons[f][YPOS]   = -LOGO_HEIGHT;
        icons[f][DELTAY] = random(6, 15);
      }
    }
  }
}

void setup() {


  Serial.begin(115200);
  Serial.println("\n\r");


  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(1000);


  matrix(LOGO_WIDTH, LOGO_HEIGHT, false);



  
  /* Setup servo motors */

  servoX.attach(servoXPin, 550, 2350);
  servoY.attach(servoYPin, 550, 2350);

  // wifi state led
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledYellowPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  digitalWrite(ledRedPin, HIGH);
  digitalWrite(ledYellowPin, LOW);
  digitalWrite(ledGreenPin, LOW);

  /*Directions pins*/
  /* pinMode(16, OUTPUT); */
  /* pinMode(17, OUTPUT); */
  /* pinMode(18, OUTPUT); */
  /* pinMode(19, OUTPUT); */

  /* setup stepper motor */
  pinMode(directionPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(endSwitchLeftPin, INPUT_PULLDOWN);
  pinMode(endSwitchRightPin, INPUT_PULLDOWN);

  digitalWrite(directionPin, HIGH);
  digitalWrite(stepPin, LOW);


  /* digitalWrite(16, LOW); */
  /* digitalWrite(17, LOW); */
  /* digitalWrite(18, LOW); */
  /* digitalWrite(19, LOW); */

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  /*fbdo.setResponseSize(4096);*/
  /*config.token_status_callback = tokenStatusCallback;*/
  /*config.max_token_generation_retry = 5;*/

  Firebase.reconnectWiFi(true);


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 20);
  display.println("Inicializando...");
  display.println("Para continuar acesse a rede wifi \"Remote Camera\"");
  display.display();

  Serial.println("Abertura Portal");
  if (
      wifiManager.autoConnect("Remote Camera", "")
      /*||*/
      /*wifiManager.startConfigPortal("ESP32-CONFIG", "12345678")*/
  ) { Serial.println("Conectado na Rede!!!");

    /*Serial.println("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);*/

    Serial.print("Signing up new Anonymous user... ");
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Signed!!");
      signupOK = true;
    } else {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }

    String uid = auth.token.uid.c_str();
    connectedWithPath = "/users/" + uid + "/connectedWith";
    Serial.println();
    Serial.printf("listening to %s", connectedWithPath.c_str());
    Serial.println();

    if (!Firebase.beginStream(streamConnectedWith, connectedWithPath.c_str()))
      Serial.printf("streamConnectedWith begin error, %s\n\n", streamConnectedWith.errorReason().c_str());

    testAlivePath = "/users/" + uid + "/testAlive";
    Serial.printf("listening to %s\n", testAlivePath.c_str());
    Serial.println();

    if (!Firebase.beginStream(streamTestAlive, testAlivePath.c_str()))
      Serial.printf("streamTestAlive begin error, %s\n\n", streamTestAlive.errorReason().c_str());


  } else {
    Serial.println("Falha ao conectar. Reiniciando...");
    /*delay(20000);*/
    ESP.restart();
  }
  Firebase.begin(&config, &auth);


  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(15, 18);
  display.print("Conectado!");
  display.display();

}

// 0 - nothing
// 1 - create new camera code
// 2 - wait for new user on connectedWith
// 4 - prepare the direction stream
// 5 - calibrate stepper motor
// 3 - listen for direction and respond to testAlive changes
int state = 1;

void loop() {

  if(digitalRead(resetPin) == HIGH){
    ESP.restart();
  }

  // connected to wifi but not connected to firebase
  if (WiFi.status() == WL_CONNECTED && !signupOK) {
    digitalWrite(ledRedPin, LOW);
    digitalWrite(ledGreenPin, LOW);

    matrix(LOGO_WIDTH, LOGO_HEIGHT, true);

    digitalWrite(ledYellowPin, LOW);
    delay(100);
    digitalWrite(ledYellowPin, HIGH);
    delay(100);
    matrix(LOGO_WIDTH, LOGO_HEIGHT, true);
    digitalWrite(ledYellowPin, LOW);
    delay(100);
    digitalWrite(ledYellowPin, HIGH);

    delay(200);
    matrix(LOGO_WIDTH, LOGO_HEIGHT, true);
    delay(200);
    matrix(LOGO_WIDTH, LOGO_HEIGHT, true);
    delay(200);
    // connected to wifi and firebase
  } else if (WiFi.status() == WL_CONNECTED && signupOK && Firebase.ready()) {
    digitalWrite(ledYellowPin, LOW);
    digitalWrite(ledRedPin, LOW);
    digitalWrite(ledGreenPin, HIGH);


    /* display.clearDisplay(); */
    /* display.setTextSize(2); */
    /* display.setCursor(3, 25); */
    /* display.print("Conectando"); */
    /* display.display(); */


    /* Handle Stream Errors */
    if (state<=2 && !Firebase.readStream(streamConnectedWith))
      Serial.printf("streamConnectedWith read error, %s\n\n", streamConnectedWith.errorReason().c_str());

    if (state<=2 && streamConnectedWith.streamTimeout()) {
      Serial.println("streamConnectedWith timed out, resuming...\n");
      if (!streamConnectedWith.httpConnected())
        Serial.printf("error code: %d, reason: %s\n\n\r", streamConnectedWith.httpCode(),
                      streamConnectedWith.errorReason().c_str());
    }

    if(millis() - testAliveMillis >= 3000){
      if (!Firebase.readStream(streamTestAlive))
        Serial.printf("streamTestAlive read error, %s\n\n\r", streamTestAlive.errorReason().c_str());

      if (streamTestAlive.streamTimeout()) {
        Serial.println("streamTestAlive timed out, resuming...\n");
        if (!streamTestAlive.httpConnected())
          Serial.printf("error code: %d, reason: %s\n\n\r", streamTestAlive.httpCode(),
              streamTestAlive.errorReason().c_str());
      }
    }

    if (state == 1) {
      Serial.print("Adding random code");

      String uid = auth.token.uid.c_str();

      char code[5] = "";
      gen_random(code, 4);
      char codePath[] = "/cameras/codes";
      char fullCodePath[20];

      displayBigText(code);

      Serial.printf(" --> %s\n\r", code);
      sprintf(fullCodePath, "%s/%s", codePath, code);

      Serial.printf("setString -- \"%s\": \"%s\"", fullCodePath, uid.c_str());

      if (Firebase.setString(fbdo, fullCodePath, uid.c_str())) {
        Serial.println(" --> Success");
        Serial.println();
      } else {
        Serial.println(fbdo.errorReason());
      }
      state = 2;
    } else if (state == 2) {
      if (streamConnectedWith.streamAvailable()) {
        /*Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent
         * "*/
        /*"type, %s\n\n",*/
        /*stream.streamPath().c_str(), stream.dataPath().c_str(),*/
        /*stream.dataType().c_str(), stream.eventType().c_str());*/
        /*printResult(stream); // see addons/RTDBHelper.h*/
        /*Serial.println();*/

        String data = streamConnectedWith.to<String>();
        String streamPath = streamConnectedWith.streamPath();

        if(streamPath == connectedWithPath){

          if (data.length() >= 25) {

            Serial.println("\n--------------------");
            Serial.printf("Received stream payload size: %d (Max. %d) -- %s\n",
                streamConnectedWith.payloadLength(), streamConnectedWith.maxPayloadLength(), streamPath.c_str());

            Serial.println();
            Serial.println(data);
            Serial.println("--------------------\n");


            String namePath = "/users/" + data + "/name";
            connectedUid = data;

            if (data != "null") {
              /*namePath += data.c_str();*/
              Serial.println("fetching " + namePath);
              if (Firebase.getString(fbdo, namePath.c_str())) {
                connectedName = fbdo.to<String>();
                Serial.print("Connecting to ");
                Serial.println(fbdo.to<String>());

                directionPath = "/users/" + data + "/direction";
                Serial.printf("listening to %s\n", directionPath.c_str());
                Serial.println();

                /*if (!Firebase.beginStream(fbdo, directionPath.c_str()))*/
                  /*Serial.printf("fbdo begin error, %s\n\r", fbdo.errorReason().c_str());*/

                currentPercentagePath = "/users/" + data + "/currentPercentage";
                horizontalPercentagePath = "/users/" + data + "/horizontalPercentage";
                updateReconnectValidUntil();
                state = 3;
              }
            }
          }

        }
      }
    } else if(state == 3){
      if (Firebase.beginStream(fbdo, directionPath.c_str())){
        digitalWrite(ledGreenPin, HIGH);
        Firebase.endStream(streamConnectedWith);
        state = 4;
      }else {
        Serial.printf("fbdo begin error, %s\n\r", fbdo.errorReason().c_str());
      }
    }else if(state == 4){

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(15, 18);
        display.print("Calibrando motor");
        display.display();

        Serial.println("Calibrando motor [1/3]");

        bool isAtBeginning = false;
        //go to the beginning of the trail 
        while(isAtBeginning == false){
          isAtBeginning = digitalRead(endSwitchLeftPin);
          step(LEFT);
        }

        Serial.println("Calibrando motor [2/3]");

        bool isAtEnd = false;
        int totalSteps = 0;

        //go to the end of the trail, counting how many steps were taken  
        while(isAtEnd == false){
          if(step(RIGHT)){
            totalSteps += 1;
          }
          isAtEnd = digitalRead(endSwitchRightPin);
        }

        Serial.println("Calibrando motor [3/3]");

        horizontalPercentage = 100.0;
        horizontalPercentageIncrement = 100.0/totalSteps;

        // go to the middle of the trail
        while(horizontalPercentage > 50){
          if(step(LEFT))
            horizontalPercentage-=horizontalPercentageIncrement;
        }

        Serial.println("Motor calibrado!");
        Serial.print(horizontalPercentage);
        Serial.print(" ");
        Serial.print(horizontalPercentageIncrement);
        Serial.print(" ");
        Serial.println(totalSteps);


        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(15, 15);
        display.println("Conectado com ");
        display.setCursor(15, 30);
        int nameFontSize = 3;
        if(connectedName.length() > 13){
          display.setCursor(5, 30);
          nameFontSize = 1;
        }else if(connectedName.length() > 4){
          display.setCursor(5, 30);
          nameFontSize = 2;
        }

        display.setTextSize(nameFontSize);
        display.print(connectedName);
        display.display();


        state = 5;
    }else if (state == 5){

      if (!Firebase.readStream(fbdo))
        Serial.printf("fbdo read error, %s\n\r", fbdo.errorReason().c_str());
      if (fbdo.streamTimeout()) {
        Serial.println("fbdo timed out, resuming...\n\r");
        if (!fbdo.httpConnected())
          Serial.printf("error code: %d, reason: %s\n\r", fbdo.httpCode(),
              fbdo.errorReason().c_str());
      }

      if(millis() - testAliveMillis >= 10000){

        if (streamTestAlive.streamAvailable()) {

          String data = streamTestAlive.to<String>();
          String streamPath = streamTestAlive.streamPath();
          String uid = auth.token.uid.c_str();

          if(data != "null"){
            Serial.println("\n--------------------");
            Serial.printf("Received stream payload size: %d (Max. %d) -- %s\n",
                streamTestAlive.payloadLength(), streamTestAlive.maxPayloadLength(), streamPath.c_str());

            Serial.println();
            Serial.println(data);
            Serial.println("--------------------\n");
          }

          updateReconnectValidUntil();
        }
      }

      // listen for directions
      if (fbdo.streamAvailable()){
        String data = fbdo.to<String>();
        if(data != "null"){


          FirebaseJsonArray *direction = fbdo.to<FirebaseJsonArray *>();
          String streamPath = fbdo.streamPath();
          /*String uid = auth.token.uid.c_str();*/
          const char* rawitems = direction->raw();

          FirebaseJsonData directions[6];

          direction->get(directions[0], 0);
          direction->get(directions[1], 1);
          direction->get(directions[2], 2);
          direction->get(directions[3], 3);
          direction->get(directions[4], 4);
          direction->get(directions[5], 5);
          //parse or print the array here
          Serial.println();
          Serial.println("Iterate Search result item...");
          Serial.println("-------------------------------");
          Serial.println(rawitems);

          // 0 1 2 3 4 5
          // 1 0 0 0 0 0 -- up
          // 0 1 0 0 0 0 -- right
          // 0 0 1 0 0 0 -- left
          // 0 0 0 1 0 0 -- down
          // 0 0 0 0 1 0 -- horizontal right
          // 0 0 0 0 0 1 -- horizontal left

          rightDir = 0;
          leftDir = 0;
          if(directions[1].to<int>() == 1){
            rightDir = 1;
          }
          if(directions[2].to<int>() == 1 && directions[1].to<int>() == 0){
            leftDir = 1;
          }

          upDir = 0;
          downDir = 0;
          if(directions[0].to<int>() == 1){
            upDir = 1;
          }
          if(directions[3].to<int>() == 1 && directions[0].to<int>() == 0){
            downDir = 1;
          }


          horizontalRightDir = 0;
          horizontalLeftDir = 0;
          if(directions[4].to<int>() == 1){
            horizontalRightDir = 1;
          }
          if(directions[5].to<int>() == 1 && directions[4].to<int>() == 0){
            horizontalLeftDir = 1;
          }

          Serial.println();
        }
      }

      if(millis() - updateMillis >= 80){
        updateMillis = millis();
        /* if (Firebase.getInt(fbdo, currentPercentagePath.c_str())) { */
        // current percentage x
        if(rightDir == 1){
          if(currentPercentageX+1 <= 90){
            currentPercentageX+=1;
          }else{
            currentPercentageX=100;
          }
        }
        if(leftDir == 1){
          if(currentPercentageX-1 >= 10){
            currentPercentageX-=1;
          }else{
            currentPercentageX=10;
          }
        }

        if((leftDir == 1 && currentPercentageX-1 >= 0) || (rightDir == 1 && currentPercentageX+1 <= 100)){
          Firebase.setIntAsync(streamTestAlive, currentPercentagePath.c_str(), currentPercentageX);
          Serial.print(currentPercentagePath + " --> ");
          Serial.println(currentPercentageX);
        }

        // current percentage y
        if(upDir == 1){
          if(currentPercentageY+1 <= 90){
            currentPercentageY+=1;
          }else{
            currentPercentageY=100;
          }
        }
        if(downDir == 1){
          if(currentPercentageY-1 >= 10){
            currentPercentageY-=1;
          }else{
            currentPercentageY=10;
          }
        }

        if((downDir == 1 && currentPercentageY-1 >= 0) || (upDir == 1 && currentPercentageY+1 <= 100)){
          Serial.print("currentPercentageY --> ");
          Serial.println(currentPercentageY);
        }

        // horizontal percentage
        if(horizontalRightDir == 1 && horizontalPercentage+1 <= 100){
          horizontalPercentage+=horizontalPercentageIncrement;
          memorySteps++;
          currentDirection = RIGHT;
          activateStepperMotor = true;
          Serial.println(horizontalPercentage);
        }
        if(horizontalLeftDir == 1 && horizontalPercentage-1 >= 0){
          horizontalPercentage-=horizontalPercentageIncrement;
          memorySteps++;
          currentDirection = LEFT;
          activateStepperMotor = true;
          Serial.println(horizontalPercentage);
        }

        if(horizontalLeftDir == 0 && horizontalRightDir == 0){
          activateStepperMotor = false;
          memorySteps = 2;
        }

        if((horizontalLeftDir == 1 || horizontalRightDir == 1) && memorySteps*horizontalPercentageIncrement >= 2){
          memorySteps = 0;
          Firebase.setIntAsync(streamTestAlive, horizontalPercentagePath.c_str(), (int)(horizontalPercentage)); 
          Serial.print(horizontalPercentagePath + " --> ");
          Serial.println(horizontalPercentage);
        }

        testAliveMillis = millis();

      }

      if(millis() - servoMillis >= 100){
        servoMillis = millis();
        servoX.write((int)(currentPercentageX*1.8));
        servoY.write(100 - (int)(currentPercentageY*1.8));
      }

      if(activateStepperMotor)
        step(currentDirection);

    }


    // not connected at all
  } else if (WiFi.status() != WL_CONNECTED) {

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 18);
    display.print("Desconectado");
    display.display();

    digitalWrite(ledRedPin, HIGH);
    digitalWrite(ledGreenPin, LOW);
    digitalWrite(ledYellowPin, LOW);
    wifiManager.autoConnect("Remote Camera", "");
  } else {
    delay(300);
    Serial.println(Firebase.ready());
    Serial.println(WiFi.status());
    Serial.println(signupOK);
  }
}
