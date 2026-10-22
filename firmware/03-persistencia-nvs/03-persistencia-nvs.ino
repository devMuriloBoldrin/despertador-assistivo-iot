// Projeto: Despertador Assistivo IoT - TCC FATEC 2026
// Etapa 3: Persistencia de alarme na memoria Flash (NVS) e disparo basico

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// Credenciais de rede local
const char* ssid = "SSID_WIFI";
const char* password = "SENHA_WIFI";

// Pinos na protoboard
const int pinoMotor = 23; // Base do BC337
const int pinoLed   = 2;  // LED indicador

// Configuracao NTP para Horario de Brasilia
const char* ntpServer = "a.st1.ntp.br";
const long  gmtOffset_sec = -3 * 3600;
const int   daylightOffset_sec = 0;

WebServer server(80);
Preferences prefs;

// Variaveis do alarme
int alarmeHora = 7;
int alarmeMinuto = 0;
bool alarmeAtivo = false;
bool alarmeDisparadoHoje = false;
bool emDisparo = false;

// Pagina web simples com formulario para configurar o alarme
void handleRoot() {
  struct tm timeinfo;
  char strHora[30] = "--:--:--";
  if (getLocalTime(&timeinfo)) {
    strftime(strHora, sizeof(strHora), "%H:%M:%S", &timeinfo);
  }

  char horaFormatada[6];
  sprintf(horaFormatada, "%02d:%02d", alarmeHora, alarmeMinuto);

  String html = "<html><head><meta charset='UTF-8'></head><body>";
  html += "<h1>Despertador Assistivo IoT - Etapa 3</h1>";
  html += "<p>Hora atual NTP: <b>" + String(strHora) + "</b></p>";
  html += "<hr>";

  html += "<h3>Configurar Alarme</h3>";
  html += "<form action='/salvar' method='GET'>";
  html += "Horario: <input type='time' name='horario' value='" + String(horaFormatada) + "'> ";
  html += "<label><input type='checkbox' name='ativo' value='1' " + String(alarmeAtivo ? "checked" : "") + "> Ativar</label><br><br>";
  html += "<input type='submit' value='Salvar na Flash'>";
  html += "</form>";

  if (emDisparo) {
    html += "<p style='color:red;'><b>ALARME DISPARANDO!</b></p>";
    html += "<a href='/desligar'><button>Desligar Alarme</button></a>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Salva as configuracoes na memoria flash NVS
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
  alarmeDisparadoHoje = false;

  // Grava na particao NVS
  prefs.begin("alarme_nvs", false);
  prefs.putInt("hora", alarmeHora);
  prefs.putInt("minuto", alarmeMinuto);
  prefs.putBool("ativo", alarmeAtivo);
  prefs.end();

  Serial.printf("Alarme salvo na Flash: %02d:%02d | Ativo: %d\n", alarmeHora, alarmeMinuto, alarmeAtivo);

  // Redireciona de volta para a pagina inicial
  server.sendHeader("Location", "/");
  server.send(303);
}

// Desliga o disparo temporariamente
void handleDesligar() {
  emDisparo = false;
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  alarmeDisparadoHoje = true;

  Serial.println("Alarme desligado pelo usuario via web.");
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

  // Le o alarme salvo na memoria Flash
  prefs.begin("alarme_nvs", true);
  alarmeHora = prefs.getInt("hora", 7);
  alarmeMinuto = prefs.getInt("minuto", 0);
  alarmeAtivo = prefs.getBool("ativo", false);
  prefs.end();

  Serial.printf("Configuracao carregada: %02d:%02d | Ativo: %d\n", alarmeHora, alarmeMinuto, alarmeAtivo);

  // Conexao Wi-Fi
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

  // Rotas do servidor
  server.on("/", handleRoot);
  server.on("/salvar", handleSalvar);
  server.on("/desligar", handleDesligar);
  server.begin();
  Serial.println("Servidor Web HTTP iniciado na porta 80.");
}

void loop() {
  server.handleClient();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Reseta a trava se mudou de minuto
    if (alarmeDisparadoHoje && timeinfo.tm_min != alarmeMinuto) {
      alarmeDisparadoHoje = false;
    }

    // Checa se deu o horario do alarme
    if (alarmeAtivo && !alarmeDisparadoHoje && !emDisparo) {
      if (timeinfo.tm_hour == alarmeHora && timeinfo.tm_min == alarmeMinuto) {
        Serial.println("Horario atingido! Disparando motores e LED.");
        emDisparo = true;
      }
    }
  }

  // Se estiver disparando, pisca o LED e pulsa os motores
  if (emDisparo) {
    static unsigned long ultimoPulso = 0;
    static bool estado = false;
    if (millis() - ultimoPulso >= 500) {
      ultimoPulso = millis();
      estado = !estado;
      digitalWrite(pinoMotor, estado ? HIGH : LOW);
      digitalWrite(pinoLed, estado ? HIGH : LOW);
    }
  }
}
