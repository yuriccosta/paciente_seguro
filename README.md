# Paciente Seguro: Monitoramento Remoto de Temperatura e Batimento Cardíaco

Este projeto implementa um sistema de monitoramento remoto de saúde utilizando a placa **BitDogLab com RP2040**, com gerenciamento de comunicação via **MQTT** para enviar dados de temperatura e batimentos cardíacos a um broker. O sistema também possui alarmes automáticos e manuais, além de sinais visuais e sonoros para monitoramento local.

---

## Objetivo
O objetivo do projeto é implementar um sistema de monitoramento remoto que leia e envie dados de temperatura e batimentos cardíacos para um servidor broker MQTT, permitindo alertas automáticos e manuais, bem como visualização em display OLED e LEDs.

---

## Funcionalidades

- **Leitura de temperatura e batimentos cardíacos**
- **Publicação de dados via MQTT**
- **Assinatura de tópicos para controle remoto**
- **Alarmes automáticos e manuais**
- **Display OLED com informações em tempo real**
- **LED RGB indicando estado de alarme**
- **Buzzer para alertas sonoros**
- **Controle via botão físico com debounce por interrupção**

---

## Componentes Utilizados

| Componente     | Função no Sistema                                           |
|----------------|-------------------------------------------------------------|
| Botão A (GPIO X) | Alterna o estado do alarme manual (via interrupção)        |
| Joystick X      | Simula a temperatura (26–46 °C)                            |
| Joystick Y      | Simula o batimento cardíaco (40–120 bpm)                   |
| LED RGB         | Verde: paciente estável; Vermelho: situação alarmante      |
| Buzzer PWM      | Alerta sonoro em situações críticas                        |
| Display OLED    | Exibe temperatura e batimentos cardíacos                   |
| Wi-Fi           | Conexão à rede para comunicação MQTT                       |
| MQTT            | Comunicação entre a placa e o broker                       |

---

## Cores do LED RGB

- **Verde**: Situação do paciente estável
- **Vermelho**: Situação do paciente alarmante (temperatura ou batimento fora da faixa, ou botão pressionado)

---

## Estrutura do Código

- **Leitura de sensores**: Simulação de leitura de temperatura e batimentos cardíacos via ADC.
- **Publicação MQTT**: Envia os dados para os tópicos `/temperatura`, `/batimento` e `/alarme`.
- **Assinatura de tópicos de comando**: Recebe comandos via `/comando/temperatura` e `/comando/batimento` para ajuste de faixas, além de `/print`, `/ping` e `/exit` para funções auxiliares.
- **Alarmes**: Ativação automática (via faixa) ou manual (via botão físico).
- **Display OLED**: Mostra temperatura e batimentos cardíacos em tempo real.
- **LED RGB e Buzzer**: Sinalizam o estado do paciente.
- **Broker MQTT (Mosquitto)**: Instalado em dispositivo Android para receber os dados.
