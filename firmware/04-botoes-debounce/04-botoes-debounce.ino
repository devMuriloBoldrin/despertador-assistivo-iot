// Projeto: Despertador Assistivo IoT - TCC FATEC 2026
// Etapa 4: Botoes fisicos com debounce (GPIO 4 e 5), soneca e desligamento

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// Credenciais de rede local
const char* ssid = "SSID_WIFI";
const char* password = "SENHA_WIFI";

// Pinos na protoboard
const int pinoMotor  = 23;  // Base do BC337
const int pinoLed    = 2;   // LED indicador
const int pinoSnooze = 4;   // Botao de Soneca 
const int pinoStop   = 5;     // Botao de Desligar 

// Configuracao NTP para Horario de Brasilia
const char* ntpServer = "a.st1.ntp.br";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

WebServer server(80);
Preferences prefs;

// Variaveis do alarme
int alarmeHora = 7;
int alarmeMinuto = 0;
bool alarmeAtivo = false;
bool alarmeDisparadoHoje = false;
int ultimoDiaDisparado = -1; // Guarda o dia do mês que disparou

// Estados basicos 
bool emDisparo = false;
bool emSoneca = false;
unsigned long tempoInicioSoneca = 0;
const unsigned long TEMPO_SONECA = 5 * 60 * 1000; // 5 minutos de soneca

// Variaveis para debounce dos botoes fisicos
unsigned long ultimoDebounceSnooze = 0;
unsigned long ultimoDebounceStop = 0;
const unsigned long TEMPO_DEBOUNCE = 250; // ms

void desligarTudo() {
  emDisparo = false;
  emSoneca = false;
  alarmeDisparadoHoje = true;
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  Serial.println("Alarme desligado em definitivo.");
}

void ativarSoneca() {
  emDisparo = false;
  emSoneca = true;
  tempoInicioSoneca = millis();
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  Serial.println("Soneca ativada (5 minutos).");
}

void handleRoot() {
  struct tm timeinfo;
  char strHora[30] = "--:--:--";
  if (getLocalTime(&timeinfo)) {
    strftime(strHora, sizeof(strHora), "%H:%M:%S", &timeinfo);
  }

  char horaFormatada[6];
  sprintf(horaFormatada, "%02d:%02d", alarmeHora, alarmeMinuto);

  String html = "<html><head><meta charset='UTF-8'></head><body>";
  html += "<h1>Despertador Assistivo IoT - Etapa 4</h1>";
  html += "<p>Hora atual NTP: <b>" + String(strHora) + "</b></p>";
  
  if (emDisparo) {
    html += "<p style='color:red;'><b>STATUS: ALARME DISPARANDO!</b></p>";
  } else if (emSoneca) {
    html += "<p style='color:orange;'><b>STATUS: MODO SONECA ATIVO</b></p>";
  } else {
    html += "<p>STATUS: Em repouso (Aguardando horario)</p>";
  }
  
  html += "<hr>";
  html += "<h3>Configurar Alarme</h3>";
  html += "<form action='/salvar' method='GET'>";
  html += "Horario: <input type='time' name='horario' value='" + String(horaFormatada) + "'> ";
  html += "<label><input type='checkbox' name='ativo' value='1' " + String(alarmeAtivo ? "checked" : "") + "> Ativar</label><br><br>";
  html += "<input type='submit' value='Salvar na Flash'>";
  html += "</form>";

  if (emDisparo || emSoneca) {
    html += "<br><a href='/desligar'><button style='padding:10px;'>Desligar Alarme</button></a>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSalvar() {
  if (server.hasArg("horario")) {
    String horario = server.arg("horario");
    int doisPontos = horario.indexOf(':');
    if (doisPontos > 0) {
      alarmeHora = horario.substring(0, doisPontos).toInt();
      alarmeMinuto = horario.substring(doisPontos + 1).toInt();
    }
  }

  alarmeAtivo = server.hasArg("ativo");
  alarmeDisparadoHoje = false; // Permite testar o novo horario configurado no mesmo dia

  prefs.begin("alarme_nvs", false);
  prefs.putInt("hora", alarmeHora);
  prefs.putInt("minuto", alarmeMinuto);
  prefs.putBool("ativo", alarmeAtivo);
  prefs.end();

  Serial.printf("Configuracao salva: %02d:%02d | Ativo: %d\n", alarmeHora, alarmeMinuto, alarmeAtivo);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDesligar() {
  desligarTudo();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(pinoMotor, OUTPUT);
  pinMode(pinoLed, OUTPUT);
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);

  // Botoes usam o pull-up interno do ESP32
  pinMode(pinoSnooze, INPUT_PULLUP);
  pinMode(pinoStop, INPUT_PULLUP);

  prefs.begin("alarme_nvs", true);
  alarmeHora = prefs.getInt("hora", 7);
  alarmeMinuto = prefs.getInt("minuto", 0);
  alarmeAtivo = prefs.getBool("ativo", false);
  prefs.end();

  Serial.printf("\nConectando a rede: %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Conectado!");
  Serial.print("Endereco IP local: http://");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/salvar", handleSalvar);
  server.on("/desligar", handleDesligar);
  server.begin();
  Serial.println("Servidor Web HTTP iniciado.");
}

void loop() {
  server.handleClient();
  unsigned long agora = millis();

  // Checagem de horario NTP
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Reseta a trava diaria ao virar o dia do mes
    if (alarmeDisparadoHoje && timeinfo.tm_mday != ultimoDiaDisparado) {
      alarmeDisparadoHoje = false;
    }

    if (alarmeAtivo && !alarmeDisparadoHoje && !emDisparo && !emSoneca) {
      if (timeinfo.tm_hour == alarmeHora && timeinfo.tm_min == alarmeMinuto) {
        Serial.println("Disparo iniciado pelo horario NTP!");
        emDisparo = true;
        ultimoDiaDisparado = timeinfo.tm_mday;
      }
    }
  }

  // Leitura do Botao 1 (Soneca - GPIO 4) com debounce
  if (digitalRead(pinoSnooze) == LOW && (agora - ultimoDebounceSnooze > TEMPO_DEBOUNCE)) {
    ultimoDebounceSnooze = agora;
    if (emDisparo) {
      Serial.println("Botao fisico D4 pressionado: SONECA.");
      ativarSoneca();
    }
  }

  // Leitura do Botao 2 (Desligar - GPIO 5) com debounce
  if (digitalRead(pinoStop) == LOW && (agora - ultimoDebounceStop > TEMPO_DEBOUNCE)) {
    ultimoDebounceStop = agora;
    if (emDisparo || emSoneca) {
      Serial.println("Botao fisico D5 pressionado: DESLIGAR.");
      desligarTudo();
    }
  }

  // Contagem regressiva da Soneca
  if (emSoneca) {
    if (agora - tempoInicioSoneca >= TEMPO_SONECA) {
      Serial.println("Tempo de soneca finalizado. Retomando vibracao e led!");
      emSoneca = false;
      emDisparo = true;
    }
  }

  // Pulsos dos atuadores durante o disparo
  if (emDisparo) {
    static unsigned long ultimoCiclo = 0;
    static bool ligado = false;
    if (agora - ultimoCiclo >= 500) {
      ultimoCiclo = agora;
      ligado = !ligado;
      digitalWrite(pinoMotor, ligado ? HIGH : LOW);
      digitalWrite(pinoLed, ligado ? HIGH : LOW);
    }
  }
}
