// Projeto: Despertador Assistivo IoT - TCC FATEC 2026
// Etapa 1: Teste inicial dos motores de vibracao e LED

// Pinos definidos na protoboard
const int pinoMotor = 23; // Vai na base do transistor BC337
const int pinoLed   = 2;  // LED 

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando teste de bancada...");

  // Configura os pinos como saida
  pinMode(pinoMotor, OUTPUT);
  pinMode(pinoLed, OUTPUT);

  // Inicia tudo desligado
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  
  delay(1000);
  Serial.println("Iniciando ciclos de vibracao e luz.");
}

void loop() {
  // Liga os 4 motores em paralelo e o LED
  Serial.println("-> Acionando motores e LED");
  digitalWrite(pinoMotor, HIGH);
  digitalWrite(pinoLed, HIGH);
  delay(1500); // Mantem ligado por 1.5 segundo

  // Desliga tudo
  Serial.println("-> Desligando");
  digitalWrite(pinoMotor, LOW);
  digitalWrite(pinoLed, LOW);
  delay(3000); // Segura 3 segundos em repouso
}
