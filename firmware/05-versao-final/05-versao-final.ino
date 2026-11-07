// Projeto: Despertador Assistivo IoT - TCC FATEC 2026
// Etapa 5: Versao final com multi-alarmes (NVS), telemetria JSON e botoes fisicos 

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// Credenciais de rede local
const char* ssid = "Familia";
const char* password = "rocamboleegolias";

// Mapeamento dos pinos na protoboard
const int pinoMotor  = 23; // Base do BC337 (4 motores em paralelo)
const int pinoLed    = 2;  // LED azul de aviso
const int pinoSnooze = 4;  // Botao 1: Soneca (active low)
const int pinoStop   = 5;  // Botao 2: Desligar (active low)

// Configuracao NTP para Horario de Brasilia (GMT-3)
const char* ntpServer = "a.st1.ntp.br";
const long  gmtOffset_sec = -3 * 3600;
const int   daylightOffset_sec = 0;

// Estrutura para os multiplos alarmes
const int MAX_ALARMES = 3;
struct Alarme {
  int hora;
  int minuto;
  bool ativo;
  bool disparadoHoje;
};

Alarme alarmes[MAX_ALARMES];

// Estados do despertador
enum EstadoSistema { IDLE, DISPARADO, SONECA };
EstadoSistema estadoAtual = IDLE;

int alarmeAtivoIndice = -1;
unsigned long tempoInicioSoneca = 0;
const unsigned long TEMPO_SONECA = 5 * 60 * 1000; // 5 minutos

// Controle dos botoes (debounce e estado anterior)
unsigned long ultimoDebounceSnooze = 0;
unsigned long ultimoDebounceStop = 0;
const unsigned long TEMPO_DEBOUNCE = 250; // ms
bool ultimoEstadoSnooze = HIGH;
bool ultimoEstadoStop = HIGH;

// Pulsos dos atuadores
unsigned long ultimoCicloPulso = 0;
bool estadoPulso = false;

WebServer server(80);
Preferences prefs;

// Declaracao das funcoes auxiliares
void carregarAlarmesFlash();
void salvarAlarmeFlash(int index);
void verificarDisparos(struct tm* timeinfo);
void atualizarAtuadores();
void aplicarSoneca();
void desligarAlarme();
String montarJsonStatus();

// Interface Web moderna embarcada na Flash
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Despertador Assistivo IoT</title>
  <style>
    :root { --bg: #0f172a; --card: #1e293b; --primary: #38bdf8; --text: #f8fafc; --danger: #ef4444; --warning: #f59e0b; --success: #10b981; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .container { width: 100%; max-width: 480px; }
    .card { background: var(--card); border-radius: 12px; padding: 20px; margin-bottom: 16px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    h1, h2, h3 { margin-top: 0; color: var(--primary); }
    .clock { font-size: 2.8rem; font-weight: 700; text-align: center; letter-spacing: 2px; margin: 10px 0; color: #fff; }
    .status-badge { text-align: center; padding: 8px; border-radius: 6px; font-weight: bold; margin-bottom: 10px; font-size: 1.1rem; }
    .status-IDLE { background: #334155; color: #94a3b8; }
    .status-DISPARADO { background: var(--danger); color: #fff; animation: pulse 1s infinite alternate; }
    .status-SONECA { background: var(--warning); color: #000; }
    @keyframes pulse { from { opacity: 0.7; } to { opacity: 1; } }
    .alarm-item { display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid #334155; padding: 14px 0; gap: 8px; }
    .alarm-item:last-child { border-bottom: none; }
    input[type="time"] { background: #0f172a; border: 1px solid #475569; color: #fff; padding: 8px; border-radius: 6px; font-size: 1.1rem; }
    .btn { border: none; border-radius: 6px; padding: 10px 14px; font-weight: bold; cursor: pointer; transition: 0.2s; }
    .btn-save { background: var(--primary); color: #0f172a; }
    .btn-stop { background: var(--danger); color: #fff; width: 100%; font-size: 1.2rem; padding: 14px; margin-top: 10px; }
    .btn-snooze { background: var(--warning); color: #000; width: 100%; font-size: 1.1rem; padding: 12px; margin-top: 8px; }
    .switch { position: relative; display: inline-block; width: 48px; height: 26px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #475569; border-radius: 34px; transition: .3s; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 4px; bottom: 4px; background-color: white; border-radius: 50%; transition: .3s; }
    input:checked + .slider { background-color: var(--success); }
    input:checked + .slider:before { transform: translateX(22px); }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div id="statusBadge" class="status-badge status-IDLE">Despertador Assistivo</div>
      <div class="clock" id="horaAtual">--:--:--</div>
      
      <div id="controlesDisparo" style="display: none;">
        <button class="btn btn-snooze" onclick="acaoRemota('/snooze')">⏰ ADIAR 5 MINUTOS (SONECA)</button>
        <button class="btn btn-stop" onclick="acaoRemota('/stop')">🛑 DESLIGAR ALARME</button>
      </div>
    </div>

    <div class="card">
      <h2>Gerenciar Alarmes</h2>
      <div id="listaAlarmes"></div>
    </div>
  </div>

  <script>
    let alarmesCarregados = false;

    function renderizarAlarmes(alarmes) {
      let html = '';
      alarmes.forEach((alm, i) => {
        let h = String(alm.hora).padStart(2, '0');
        let m = String(alm.minuto).padStart(2, '0');
        html += `
          <div class="alarm-item">
            <div>
              <strong>Alarme ${i + 1}</strong><br>
              <input type="time" id="time_${i}" value="${h}:${m}">
            </div>
            <label class="switch">
              <input type="checkbox" id="chk_${i}" ${alm.ativo ? 'checked' : ''} onchange="salvar(${i})">
              <span class="slider"></span>
            </label>
            <button class="btn btn-save" onclick="salvar(${i})">Salvar</button>
          </div>
        `;
      });
      document.getElementById('listaAlarmes').innerHTML = html;
    }

    async function atualizarTelemetria() {
      try {
        const res = await fetch('/status');
        const data = await res.json();
        
        document.getElementById('horaAtual').innerText = data.hora;
        const badge = document.getElementById('statusBadge');
        
        if (data.estado === 'IDLE') {
          badge.innerText = 'Despertador Assistivo';
          badge.className = 'status-badge status-IDLE';
        } else {
          badge.innerText = data.estado;
          badge.className = 'status-badge status-' + data.estado;
        }

        const ctrl = document.getElementById('controlesDisparo');
        ctrl.style.display = (data.estado === 'DISPARADO' || data.estado === 'SONECA') ? 'block' : 'none';

        if (!alarmesCarregados) {
          renderizarAlarmes(data.alarmes);
          alarmesCarregados = true;
        }
      } catch (err) {
        console.error("Erro na leitura de status:", err);
      }
    }

    async function salvar(index) {
      const timeInput = document.getElementById(time_${index});
      if (!timeInput || !timeInput.value) {
        alert("Escolha um horario valido.");
        return;
      }
      
      const parts = timeInput.value.split(':');
      const h = parseInt(parts[0], 10);
      const m = parseInt(parts[1], 10);
      const chkVal = document.getElementById(chk_${index}).checked ? "1" : "0";

      const btn = event?.target;
      if (btn && btn.tagName === 'BUTTON') btn.innerText = "Salvando...";

      try {
        const res = await fetch(/setAlarm?id=${index}&h=${h}&m=${m}&en=${chkVal});
        if (res.ok) {
          if (btn && btn.tagName === 'BUTTON') {
            btn.innerText = "Salvo!";
            setTimeout(() => { btn.innerText = "Salvar"; }, 1500);
          }
        }
      } catch (e) {
        alert("Falha ao salvar alarme.");
        if (btn && btn.tagName === 'BUTTON') btn.innerText = "Salvar";
      }
    }

    async function acaoRemota(rota) {
      await fetch(rota);
      atualizarTelemetria();
    }

    setInterval(atualizarTelemetria, 1000);
    atualizarTelemetria();
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(pinoMotor, OUTPUT);
  pinMode(pinoLed, OUTPUT);
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);

  pinMode(pinoSnooze, INPUT_PULLUP);
  pinMode(pinoStop, INPUT_PULLUP);

  // Carrega os 3 alarmes da memoria flash
  carregarAlarmesFlash();

  // Conexao Wi-Fi nao-bloqueante (timeout de 10s)
  Serial.printf("\nConectando a rede: %s", ssid);
  WiFi.begin(ssid, password);
  int tentativasWifi = 0;
  
  while (WiFi.status() != WL_CONNECTED && tentativasWifi < 20) {
    delay(500);
    Serial.print(".");
    tentativasWifi++;
  }

  // Se nao conectar, reinicia o esp
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFalha ao conectar no Wi-Fi. Reiniciando...");
    ESP.restart();
  }

  Serial.println("\nWi-Fi Conectado!");
  Serial.print("Endereco IP local: http://");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Rotas HTTP
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", HTML_PAGE);
  });

  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json", montarJsonStatus());
  });

  server.on("/setAlarm", HTTP_GET, []() {
    if (server.hasArg("id") && server.hasArg("h") && server.hasArg("m") && server.hasArg("en")) {
      int id = server.arg("id").toInt();
      if (id >= 0 && id < MAX_ALARMES) {
        alarmes[id].hora = server.arg("h").toInt();
        alarmes[id].minuto = server.arg("m").toInt();
        alarmes[id].ativo = (server.arg("en").toInt() == 1);
        alarmes[id].disparadoHoje = false;
        salvarAlarmeFlash(id);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/snooze", HTTP_GET, []() {
    aplicarSoneca();
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", HTTP_GET, []() {
    desligarAlarme();
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Servidor Web HTTP iniciado com sucesso.");
}

void loop() {
  server.handleClient();
  unsigned long agora = millis();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    verificarDisparos(&timeinfo);
  }

  // Leitura do Botao 1 com Edge Detection (Soneca - GPIO 4)
  bool leituraSnooze = digitalRead(pinoSnooze);
  if (leituraSnooze == LOW && ultimoEstadoSnooze == HIGH && (agora - ultimoDebounceSnooze > TEMPO_DEBOUNCE)) {
    ultimoDebounceSnooze = agora;
    if (estadoAtual == DISPARADO) {
      Serial.println("Botao fisico D4 pressionado: SONECA.");
      aplicarSoneca();
    }
  }
  ultimoEstadoSnooze = leituraSnooze;

  // Leitura do Botao 2 com Edge Detection (Desligar - GPIO 5)
  bool leituraStop = digitalRead(pinoStop);
  if (leituraStop == LOW && ultimoEstadoStop == HIGH && (agora - ultimoDebounceStop > TEMPO_DEBOUNCE)) {
    ultimoDebounceStop = agora;
    if (estadoAtual == DISPARADO || estadoAtual == SONECA) {
      Serial.println("Botao fisico D5 pressionado: DESLIGAR.");
      desligarAlarme();
    }
  }
  ultimoEstadoStop = leituraStop;

  // Retomada da Soneca
  if (estadoAtual == SONECA) {
    if (agora - tempoInicioSoneca >= TEMPO_SONECA) {
      Serial.println("Tempo de soneca esgotado. Retomando disparo.");
      estadoAtual = DISPARADO;
    }
  }

  atualizarAtuadores();
}

void atualizarAtuadores() {
  if (estadoAtual == DISPARADO) {
    unsigned long agora = millis();
    if (agora - ultimoCicloPulso >= 500) {
      ultimoCicloPulso = agora;
      estadoPulso = !estadoPulso;
      digitalWrite(pinoMotor, estadoPulso ? HIGH : LOW);
      digitalWrite(pinoLed, estadoPulso ? HIGH : LOW);
    }
  } else {
    digitalWrite(pinoMotor, LOW);
    digitalWrite(pinoLed, LOW);
    estadoPulso = false;
  }
}

void aplicarSoneca() {
  if (estadoAtual == DISPARADO) {
    estadoAtual = SONECA;
    tempoInicioSoneca = millis();
    digitalWrite(pinoMotor, LOW);
    digitalWrite(pinoLed, LOW);
  }
}

void desligarAlarme() {
  estadoAtual = IDLE;
  if (alarmeAtivoIndice >= 0 && alarmeAtivoIndice < MAX_ALARMES) {
    alarmes[alarmeAtivoIndice].disparadoHoje = true;
  }
  alarmeAtivoIndice = -1;
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  Serial.println("Alarme desligado.");
}

void verificarDisparos(struct tm* timeinfo) {
  // Ignora avaliacao se o NTP ainda nao sincronizou o ano (1970 = tm_year 70, entao checamos se eh > 2020)
  if (timeinfo->tm_year < 120) return;

  for (int i = 0; i < MAX_ALARMES; i++) {
    if (alarmes[i].disparadoHoje && timeinfo->tm_min != alarmes[i].minuto) {
      alarmes[i].disparadoHoje = false;
    }
  }

  if (estadoAtual != IDLE) return;

  for (int i = 0; i < MAX_ALARMES; i++) {
    if (alarmes[i].ativo && !alarmes[i].disparadoHoje) {
      if (timeinfo->tm_hour == alarmes[i].hora && timeinfo->tm_min == alarmes[i].minuto) {
        Serial.printf("Horario atingido! Alarme %d disparando.\n", i + 1);
        estadoAtual = DISPARADO;
        alarmeAtivoIndice = i;
        break;
      }
    }
  }
}

void carregarAlarmesFlash() {
  prefs.begin("alarmes_db", false);
  for (int i = 0; i < MAX_ALARMES; i++) {
    String pfx = "a" + String(i);
    alarmes[i].hora   = prefs.getInt((pfx + "_h").c_str(), 7 + i);
    alarmes[i].minuto = prefs.getInt((pfx + "_m").c_str(), 0);
    alarmes[i].ativo  = prefs.getBool((pfx + "_en").c_str(), false);
    alarmes[i].disparadoHoje = false;
  }
  prefs.end();
}

void salvarAlarmeFlash(int i) {
  prefs.begin("alarmes_db", false);
  String pfx = "a" + String(i);
  prefs.putInt((pfx + "_h").c_str(), alarmes[i].hora);
  prefs.putInt((pfx + "_m").c_str(), alarmes[i].minuto);
  prefs.putBool((pfx + "_en").c_str(), alarmes[i].ativo);
  prefs.end();
  Serial.printf("Alarme %d salvo na Flash: %02d:%02d | Ativo: %d\n", i + 1, alarmes[i].hora, alarmes[i].minuto, alarmes[i].ativo);
}

String montarJsonStatus() {
  struct tm timeinfo;
  char bufHora[16] = "--:--:--";
  if (getLocalTime(&timeinfo)) {
    strftime(bufHora, sizeof(bufHora), "%H:%M:%S", &timeinfo);
  }

  String json;
  json.reserve(256); // Reserva memoria para o JSON nao travar o esp
  
  json = "{";
  json += "\"hora\":\"" + String(bufHora) + "\",";
  
  String st = "IDLE";
  if (estadoAtual == DISPARADO) st = "DISPARADO";
  else if (estadoAtual == SONECA) st = "SONECA";
  json += "\"estado\":\"" + st + "\",";

  json += "\"alarmes\":[";
  for (int i = 0; i < MAX_ALARMES; i++) {
    json += "{\"hora\":" + String(alarmes[i].hora) + ",";
    json += "\"minuto\":" + String(alarmes[i].minuto) + ",";
    json += "\"ativo\":" + String(alarmes[i].ativo ? "true" : "false") + "}";
    if (i < MAX_ALARMES - 1) json += ",";
  }
  json += "]}";
  
  return json;
}
