// Projeto: Despertador Assistivo IoT - TCC FATEC 2026
// Etapa 2: Conexao Wi-Fi, sincronizacao horaria NTP e Web basico

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// Credenciais de rede local
const char* ssid = "SSID_WIFI";
const char* password = "SENHA_WIFI";

// Configuracao NTP para o Horario de Brasilia
const char* ntpServer = "a.st1.ntp.br";
const long  gmtOffset_sec = -3 * 3600;
const int   daylightOffset_sec = 0;

WebServer server(80);

// Pagina web simples p/ teste
void handleRoot() {
  struct tm timeinfo;
  char strHora[30] = "Hora nao obtida";
  
  if (getLocalTime(&timeinfo)) {
    strftime(strHora, sizeof(strHora), "%H:%M:%S", &timeinfo);
  }

  String html = "<html><head><meta charset='UTF-8'></head><body>";
  html += "<h1>Despertador Assistivo IoT - Etapa 2</h1>";
  html += "<p>Relogio NTP Atual: <b>" + String(strHora) + "</b></p>";
  html += "<p>Status: Conectado na rede e operando.</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.printf("\nConectando a rede: %s", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Conectado!");
  Serial.print("Endereco IP local: http://");
  Serial.println(WiFi.localIP());

  // Inicia sincronizacao horaria com os servidores do NTP.br
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Rota principal
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor Web HTTP iniciado na porta 80.");
}

void loop() {
  server.handleClient();

  // Exibe a hora no monitor serial de tempos em tempos
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint >= 5000) {
    ultimoPrint = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println(&timeinfo, "Hora atual: %H:%M:%S");
    }
  }
}
