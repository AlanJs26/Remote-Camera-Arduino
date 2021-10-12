#include <DNSServer.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <addons/RTDBHelper.h>
#include <addons/TokenHelper.h>
/*#include <cstdio>*/

WiFiManager wifiManager;

#define API_KEY "AIzaSyAcT5oGP6ZFtJ5sHXsqxVa6DwgwxBzVexw"
#define DATABASE_URL "remotecamera-6f583-default-rtdb"
#define DATABASE_SECRET "zC7bJfF0iGMQ1IPKq5bYj2WOrAx9UF9j9ghzaoTq"

FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;

FirebaseConfig config;
FirebaseJson json;

bool signupOK = false;

void saveConfigCallback() { Serial.println("Configuração salva"); }

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
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

  digitalWrite(26, LOW);

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
  if (wifiManager.autoConnect("ESP32-CONFIG", "12345678")
      /*|| wifiManager.startConfigPortal("ESP32-CONFIG", "12345678")*/
  ) {
    Serial.println("Conectado na Rede!!!");

    /*Serial.println("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);*/

    Serial.print("Signing up new Anonymous user... ");
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Signed!!");
      signupOK = true;
    } else {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }

    /*String uid = auth.token.uid.c_str();*/
    /*String connectedWithPath = "/users/" + uid + "/connectedWith";*/
    /*Serial.println(connectedWithPath);*/

    /*if (!Firebase.beginStream(stream, connectedWithPath))*/
    /*Serial.printf("stream begin error, %s\n\n",
     * stream.errorReason().c_str());*/

  } else {
    Serial.println("Falha ao conectar. Reiniciando...");
    /*delay(20000);*/
    ESP.restart();
  }
  Firebase.begin(&config, &auth);
  Serial.println("exited");
}

// 0 - nothing
// 1 - create new camera code
// 2 - wait for new user on connectedWith
// 3 - listen for directions and respond to testAlive changes
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

    /*delay(10000);*/
    /*if (stream.streamTimeout()) {*/
    /*Serial.println("stream timed out, resuming...\n");*/
    /*delay(10000);*/

    /*if (!stream.httpConnected())*/
    /*Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(),*/
    /*stream.errorReason().c_str());*/
    /*}*/

    if (state == 1) {
      Serial.println("Adding random code");
      /*delay(10000);*/
      String uid = auth.token.uid.c_str();

      char code[5] = "";
      gen_random(code, 4);
      char codePath[] = "/cameras/codes";
      char fullCodePath[20];
      sprintf(fullCodePath, "%s/%s", codePath, code);

      /*Serial.println(fullCodePath);*/
      Serial.printf("\"%s\": \"%s\"\n", fullCodePath, uid.c_str());
      /*delay(10000);*/

      /*json.set(code, uid.c_str());*/
      json.set("alan", "joselegal");

      /*Firebase.setString(fbdo, "/cameras/codes/bq3w", uid.c_str());*/
      /*Firebase.setString(fbdo, "/cameras/codes/bq3w", "teste");*/
      if (Firebase.pushJSON(fbdo, "/cameras/codes", json)) {
        Serial.println("nice... i think.");
      } else {
        Serial.println(fbdo.errorReason());
      }
      state = 2;
    } else if (state == 2) {
      /*if (stream.streamAvailable()) {*/
      /*Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent "*/
      /*"type, %s\n\n",*/
      /*stream.streamPath().c_str(), stream.dataPath().c_str(),*/
      /*stream.dataType().c_str(), stream.eventType().c_str());*/
      /*printResult(stream); // see addons/RTDBHelper.h*/
      /*Serial.println();*/

      /*// This is the size of stream payload received (current and max value)*/
      /*// Max payload size is the payload size under the stream path since
       * the*/
      /*// stream connected and read once and will not update until stream*/
      /*// reconnection takes place. This max value will be zero as no payload*/
      /*// received in case of ESP8266 which BearSSL reserved Rx buffer size
       * is*/
      /*// less than the actual stream payload.*/
      /*Serial.printf("Received stream payload size: %d (Max. %d)\n\n",*/
      /*stream.payloadLength(), stream.maxPayloadLength());*/
      /*}*/
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
