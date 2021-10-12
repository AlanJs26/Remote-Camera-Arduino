# 1 "/home/alan/Documentos/Codes/Arduino/tcc/tcc.ino"
//Bibliotecas
# 3 "/home/alan/Documentos/Codes/Arduino/tcc/tcc.ino" 2
# 4 "/home/alan/Documentos/Codes/Arduino/tcc/tcc.ino" 2
# 5 "/home/alan/Documentos/Codes/Arduino/tcc/tcc.ino" 2
# 6 "/home/alan/Documentos/Codes/Arduino/tcc/tcc.ino" 2

WiFiManager wifiManager; //Objeto de manipulação do wi-fi

void setup() {

  Serial.begin(115200);
  Serial.println();

  //Definição dos pinos
  pinMode(26, 0x02);

  //LEDs apagados
  digitalWrite(26, 0x0);

  //callback para quando entra em modo de configuração AP
  wifiManager.setAPCallback(configModeCallback);
  //callback para quando se conecta em uma rede, ou seja, quando passa a trabalhar em modo estação
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  Serial.println("Abertura Portal"); //Abre o portal
  digitalWrite(26, 0x0); //Acende LED Vermelho
  wifiManager.resetSettings(); //Apaga rede salva anteriormente
  if (!wifiManager.startConfigPortal("ESP32-CONFIG", "12345678")) { //Nome da Rede e Senha gerada pela ESP
    Serial.println("Falha ao conectar"); //Se caso não conectar na rede mostra mensagem de falha
    delay(2000);
    ESP.restart(); //Reinicia ESP após não conseguir conexão na rede
  } else { //Se caso conectar
    Serial.println("Conectado na Rede!!!");
    delay(10000);
    /*ESP.restart();  //Reinicia ESP após conseguir conexão na rede*/
  }
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) { //Se conectado na rede
    digitalWrite(26, 0x1); //Acende LED AZUL
  } else { //se não conectado na rede
    digitalWrite(26, 0x0); //Apaga LED AZUL
    //Pisca LED Vermelho
    digitalWrite(26, 0x1);
    delay(200);
    digitalWrite(26, 0x0);
    delay(200);
    wifiManager.autoConnect(); //Função para se autoconectar na rede
  }
}

//callback que indica que o ESP entrou no modo AP
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP()); //imprime o IP do AP
  Serial.println(myWiFiManager->getConfigPortalSSID()); //imprime o SSID criado da rede
}

//Callback que indica que salvamos uma nova rede para se conectar (modo estação)
void saveConfigCallback() {
  Serial.println("Configuração salva");
}
