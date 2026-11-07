# Despertador Assistivo IoT

**Trabalho de Conclusão de Curso (TCC) - FATEC Ribeirão Preto (2026)**
**Curso:** Análise e Desenvolvimento de Sistemas (ADS)
**Desenvolvedores:** Gabriel Chaves Amaral e Murilo Corsi Boldrin

Este repositório contém o código e a documentação do nosso projeto de TCC: um relógio despertador IoT focado na acessibilidade de pessoas com deficiência auditiva severa ou profunda. O sistema substitui os alarmes sonoros tradicionais por feedback vibrotátil e luminoso, e pode ser configurado via Wi-Fi por meio de uma interface web hospedada no próprio microcontrolador.

---

## Arquitetura de Hardware

O protótipo foi montado com um ESP32, atuadores mecânicos e componentes discretos para o controle de potência.

- **Microcontrolador:** ESP32 (Wi-Fi + Bluetooth).
- **Atuadores Táteis:** 4 micromotores de vibração (tipo moeda 1027) ligados em paralelo.
- **Sinalizador Visual:** 1 LED azul de alto brilho.
- **Driver de Potência:** Transistor NPN BC337 operando na região de saturação, com Diodo Flyback 1N4007 para proteção contra correntes reversas.
- **Entradas:** 2 push buttons para controle físico.

### Pinout
| Componente | Pino (ESP32) | Descrição |
| :--- | :--- | :--- |
| **Transistor / Motores** | `GPIO 23` | Base do BC337 (Aciona os 4 motores com ~220mA a 3.3V) |
| **LED Indicador** | `GPIO 2` | Aciona o LED de feedback visual |
| **Botão Soneca** | `GPIO 4` | Active Low (`INPUT_PULLUP`). Adia o alarme em 5 minutos |
| **Botão Desligar** | `GPIO 5` | Active Low (`INPUT_PULLUP`). Interrompe o alarme atual |

---

## Funcionalidades do Firmware

O código foi escrito em C/C++ (Arduino Core) com foco em evitar rotinas bloqueantes (evitando o uso de `delay()` onde possível).

- **Sincronismo NTP:** Consulta os servidores `a.st1.ntp.br` para manter o horário sempre atualizado em tempo real.
- **Memória Não-Volátil (NVS):** Gravação de até 3 alarmes independentes na memória flash (usando a biblioteca `Preferences`). Os alarmes não se perdem se o aparelho for desligado da tomada.
- **WebServer Assíncrono:** Página web (HTML/CSS/JS) servida diretamente pelo ESP32.
- **Telemetria JSON:** Atualização do painel via Fetch API para mostrar o estado atual do despertador (`IDLE`, `DISPARADO`, `SONECA`) sem precisar dar refresh na página.
- **Debounce por Software:** Tratamento de repique dos botões usando `millis()`, eliminando a necessidade de colocar capacitores na protoboard.

---

## Estrutura do Repositório

Dividimos o desenvolvimento em 5 etapas para facilitar os testes de bancada e validação de cada módulo:

1. **`firmware/01-teste-atuadores/`** - Validação do circuito de potência (transistor, giro dos motores e LED).
2. **`firmware/02-ntp-webserver-basico/`** - Conexão Wi-Fi, requisição do horário via NTP e rotas HTTP básicas.
3. **`firmware/03-persistencia-nvs/`** - Lógica para salvar e ler os horários na Flash do ESP32.
4. **`firmware/04-botoes-debounce/`** - Leitura dos botões físicos e tratamento de ruído por software.
5. **`firmware/05-versao-final/`** - Código de produção juntando tudo (multi-alarmes, web server assíncrono, edge detection e timeout de Wi-Fi).

---

## Como Executar

1. Faça o clone do repositório ou baixe a pasta `05-versao-final`.
2. Abra o arquivo `.ino` na **Arduino IDE** (é preciso ter o pacote do ESP32 instalado nas configurações de placas).
3. No início do código, altere as variáveis `ssid` e `password` com os dados da sua rede Wi-Fi (2.4GHz).
4. Conecte o ESP32 no PC, selecione a porta COM correta e faça o upload.
5. Abra o **Monitor Serial** (baud rate 115200) para ver o endereço IP gerado pela placa.
6. Acesse o IP pelo navegador do seu celular ou PC (que deve estar na mesma rede Wi-Fi).