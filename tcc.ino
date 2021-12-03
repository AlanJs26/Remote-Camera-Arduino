#include <DNSServer.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>
/* #include <addons/RTDBHelper.h> */
/* #include <addons/TokenHelper.h> */

// Configuração do servo motor

#include <Servo.h>

static const int servoXPin = 13;
static const int servoYPin = 12;
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

static const int directionPin = 18; // HIGH para sentido horário
static const int stepPin = 19;

static const int endSwitchRightPin = 16;
static const int endSwitchLeftPin = 17;

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

int currentPercentageX = 0;
int currentPercentageY = 0;
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

void step(bool dir){
  if(digitalRead(endSwitchLeftPin) || digitalRead(endSwitchRightPin)){
    stepMillis = millis();
    return;
  }

  if(dir){
    digitalWrite(directionPin, HIGH);
  }else{
    digitalWrite(directionPin, LOW);
  }

  if(millis() - stepMillis >= 2000) digitalWrite(stepPin, HIGH);

  if(millis() - stepMillis >= 4000){
    digitalWrite(stepPin, LOW);
    stepMillis = millis();
  }

}

void setup() {


  Serial.begin(115200);
  Serial.println();
  
  /* Setup servo motors */
  servoX.attach(servoXPin);
  servoY.attach(servoYPin);

  // wifi state led
  pinMode(26, OUTPUT);
  digitalWrite(26, LOW);

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

  Serial.println("Abertura Portal");
  if (
      wifiManager.autoConnect("ESP32-CONFIG", "12345678")
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
}

// 0 - nothing
// 1 - create new camera code
// 2 - wait for new user on connectedWith
// 4 - prepare the direction stream
// 5 - calibrate stepper motor
// 3 - listen for direction and respond to testAlive changes
int state = 1;

void loop() {

  // connected to wifi but not connected to firebase
  if (WiFi.status() == WL_CONNECTED && !signupOK) {
    digitalWrite(26, LOW);
    delay(100);
    digitalWrite(26, HIGH);
    delay(100);
    digitalWrite(26, LOW);
    delay(100);
    digitalWrite(26, HIGH);
    delay(600);
    // connected to wifi and firebase
  } else if (WiFi.status() == WL_CONNECTED && signupOK && Firebase.ready()) {
    digitalWrite(26, HIGH);

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
              connectedName = fbdo.to<String>();
              /*namePath += data.c_str();*/
              Serial.println("fetching " + namePath);
              if (Firebase.getString(fbdo, namePath.c_str())) {
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
                state = 4;
              }
            }
          }

        }
      }
    } else if(state == 4){
      if (Firebase.beginStream(fbdo, directionPath.c_str())){
        Firebase.endStream(streamConnectedWith);
        state = 3;
      }else {
        Serial.printf("fbdo begin error, %s\n\r", fbdo.errorReason().c_str());
      }
    }else if(state == 5){
        bool isAtBeginning = false;

        //go to the beginning of the trail 
        while(isAtBeginning == false){
          isAtBeginning = digitalRead(endSwitchLeftPin);
          step(LEFT);
        }

        bool isAtEnd = false;
        int totalSteps = 0;

        //go to the end of the trail, counting how many steps were taken  
        while(isAtEnd == false){
          isAtEnd = digitalRead(endSwitchRightPin);
          step(RIGHT);
          totalSteps++;
        }

        horizontalPercentage = 100;
        horizontalPercentageIncrement = 100.0/totalSteps;

        // go to the middle of the trail
        while(horizontalPercentage <= 50){
          step(LEFT);
          horizontalPercentage-=horizontalPercentageIncrement;
        }


        state = 3;
    }else if (state == 3){

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

          /* for(int i = 0; i<4; i++){ */
            /* [>Serial.printf("%i ",directions[i].to<int>());<] */

            /* if(directions[i].to<int>() == 1){ */
              /* digitalWrite(16+i, HIGH); */
            /* }else{ */
              /* digitalWrite(16+i, LOW); */
            /* } */
          /* } */
          Serial.println();
        }
      }

      if(millis() - updateMillis >= 50){
        updateMillis = millis();
        /* if (Firebase.getInt(fbdo, currentPercentagePath.c_str())) { */
        // current percentage x
        if(rightDir == 1){
          if(currentPercentageX+1 <= 100){
            currentPercentageX+=1;
          }else{
            currentPercentageX=100;
          }
        }
        if(leftDir == 1){
          if(currentPercentageX-1 >= 0){
            currentPercentageX-=1;
          }else{
            currentPercentageX=0;
          }
        }

        if((leftDir == 1 && currentPercentageX-1 >= 0) || (rightDir == 1 && currentPercentageX+1 <= 100)){
          Firebase.setIntAsync(streamTestAlive, currentPercentagePath.c_str(), currentPercentageX);
          Serial.print(currentPercentagePath + " --> ");
          Serial.println(currentPercentageX);
        }

        // current percentage y
        if(upDir == 1){
          if(currentPercentageY+1 <= 100){
            currentPercentageY+=1;
          }else{
            currentPercentageY=100;
          }
        }
        if(downDir == 1){
          if(currentPercentageY-1 >= 0){
            currentPercentageY-=1;
          }else{
            currentPercentageY=0;
          }
        }

        // horizontal percentage
        if(horizontalRightDir == 1 && horizontalPercentage+1 <= 100){
          horizontalPercentage+=horizontalPercentageIncrement;
          currentDirection = RIGHT;
          activateStepperMotor = true;
        }
        if(horizontalLeftDir == 1 && horizontalPercentage-1 >= 0){
          horizontalPercentage-=horizontalPercentageIncrement;
          currentDirection = LEFT;
          activateStepperMotor = true;
        }

        if(horizontalLeftDir == 0 && horizontalRightDir == 0){
          activateStepperMotor = false;
        }

        if(horizontalLeftDir == 1 || horizontalRightDir == 1){
          Firebase.setIntAsync(streamTestAlive, horizontalPercentagePath.c_str(), (int)(horizontalPercentage)); 
          Serial.print(horizontalPercentagePath + " --> ");
          Serial.println(horizontalPercentage);
        }

        testAliveMillis = millis();
      }

      servoX.write((int)(currentPercentageX*1.8));
      servoY.write((int)(currentPercentageY*1.8));
      if(activateStepperMotor)
        step(currentDirection);
    }


    // not connected at all
  } else if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(26, LOW);
    wifiManager.autoConnect();
  } else {
    delay(300);
    Serial.println(Firebase.ready());
    Serial.println(WiFi.status());
    Serial.println(signupOK);
  }
}
