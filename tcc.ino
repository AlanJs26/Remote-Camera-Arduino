#include <DNSServer.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <addons/RTDBHelper.h>
#include <addons/TokenHelper.h>


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
String testAlivePath;
String directionPath;

int currentPercentage = 0;

int rightDir = 0;
int leftDir  = 0;
int upDir    = 0;
int downDir  = 0;

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

void setup() {

  Serial.begin(115200);
  Serial.println();

  pinMode(26, OUTPUT);

  /*Directions pins*/
  pinMode(16, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(18, OUTPUT);
  pinMode(19, OUTPUT);

  digitalWrite(26, LOW);

  digitalWrite(16, LOW);
  digitalWrite(17, LOW);
  digitalWrite(18, LOW);
  digitalWrite(19, LOW);

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

          FirebaseJsonData directions[4];

          direction->get(directions[0], 0);
          direction->get(directions[1], 1);
          direction->get(directions[2], 2);
          direction->get(directions[3], 3);
          //parse or print the array here
          Serial.println();
          Serial.println("Iterate Search result item...");
          Serial.println("-------------------------------");
          Serial.println(rawitems);

          rightDir = 0;
          leftDir = 0;
          if(directions[1].to<int>() == 1){
            rightDir = 1;
          }
          if(directions[2].to<int>() == 1 && directions[1].to<int>() == 0){
            leftDir = 1;
          }

          for(int i = 0; i<4; i++){
            /*Serial.printf("%i ",directions[i].to<int>());*/

            if(directions[i].to<int>() == 1){
              digitalWrite(16+i, HIGH);
            }else{
              digitalWrite(16+i, LOW);
            }
          }
          Serial.println();
          /*for (size_t i = 0; i < direction.size(); i++)*/
          /*{*/
          /*direction.get(result, i);*/
          /*if (result.typeNum == FirebaseJson::JSON_STRING)*/
          /*Serial.printf("Array index %d, String Val: %s\n", i, result.to<String>().c_str());*/
          /*else if (result.typeNum == FirebaseJson::JSON_INT)*/
          /*Serial.printf("Array index %d, Int Val: %d\n", i, result.to<int>());*/
          /*else if (result.typeNum == FirebaseJson::JSON_FLOAT)*/
          /*Serial.printf("Array index %d, Float Val: %f\n", i, result.to<float>());*/
          /*else if (result.typeNum == FirebaseJson::JSON_DOUBLE)*/
          /*Serial.printf("Array index %d, Double Val: %f\n", i, result.to<double>());*/
          /*else if (result.typeNum == FirebaseJson::JSON_BOOL)*/
          /*Serial.printf("Array index %d, Bool Val: %d\n", i, result.to<bool>());*/
          /*else if (result.typeNum == FirebaseJson::JSON_OBJECT)*/
          /*Serial.printf("Array index %d, Object Val: %s\n", i, result.to<String>().c_str());*/
          /*else if (result.typeNum == FirebaseJson::JSON_ARRAY)*/
          /*Serial.printf("Array index %d, Array Val: %s\n", i, result.to<String>().c_str());*/
          /*else if (result.typeNum == FirebaseJson::JSON_NULL)*/
          /*Serial.printf("Array index %d, Null Val: %s\n", i, result.to<String>().c_str());*/
          /*}*/
        }
      }

      if(millis() - updateMillis >= 200){
        updateMillis = millis();
        /* if (Firebase.getInt(fbdo, currentPercentagePath.c_str())) { */
        if(rightDir == 1 && currentPercentage+10 <= 100){
          currentPercentage+=1;
        }
        if(leftDir == 1 && currentPercentage-10 >= 0){
          currentPercentage-=1;
        }

        if(leftDir == 1 || rightDir == 1){
          if (Firebase.setInt(streamTestAlive, currentPercentagePath.c_str(), currentPercentage)) {
            Serial.print(currentPercentagePath + " --> ");
            Serial.println(currentPercentage);
            testAliveMillis = millis();
          }else{
            Serial.println(streamTestAlive.errorReason());
            Serial.println("\r\n");
          }
        }

      }

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
