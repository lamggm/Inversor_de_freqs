
# Inversor DC de Baixa Tensão com Arduino + IR2110 + MOSFET

Projeto de um inversor em **baixa tensão (12–24 V DC)** usando:

- 1x Arduino (UNO ou similar)
- 2x drivers de MOSFET **IR2110**
- 4x MOSFETs N–channel de potência (logic–level, ex: IRLZ44N, IRLZ44S, etc.)
- Ponte H completa para acionar motor / carga em baixa tensão

> **ATENÇÃO:** Este projeto é para **BAIXA TENSÃO DC**.  
> Nada de ligar direto em 127/220 V AC.  
> Versão “tomar choque” é outro projeto, com isolação, layout decente e normas.

---

## Estrutura sugerida do repositório

```text
inversor-arduino-ir2110/
├── README.md
├── firmware/
│   └── arduino_inversor_dc/
│       └── arduino_inversor_dc.ino
├── hardware/
│   └── ponte_h_ir2110.md
└── docs/
    └── testes_e_notas.md
```

- `firmware/`: código do Arduino.
- `hardware/`: anotações de ligação, esquemas, fotos.
- `docs/`: resultados de teste, medições, problemas, etc.

---

## 1. Objetivo

Montar uma **ponte H completa** com 4 MOSFETs N–channel, usando **2× IR2110** como drivers high/low side, controlados por um Arduino, para gerar uma saída AC simulada em baixa tensão (onda quadrada ou SPWM simples).

---

## 2. Componentes usados

Itens que temos:

- 2× IR2110  
- 2× diodo UF4001 (rápidos)  
- 4× diodo 1N4007  
- 2× capacitor eletrolítico 10 µF (bootstrap)  
- 1× capacitor eletrolítico 330 µF (barramento DC)  
- 4× resistores 15 Ω (gate)  
- 2× resistores 42 Ω (uso geral / série em entrada, etc.)

Itens que **precisam ser adicionados**:

- 4× MOSFET N–channel de potência (logic level, ≥ 30 V, com Rds(on) baixo)  
- 1× fonte 12–24 V DC (barramento de potência)  
- 1× fonte 10–15 V DC para VCC dos IR2110 (pode ser a mesma da ponte se bem dimensionada)  
- Resistores de pull-down para gates (ex.: 10 kΩ)  
- Capacitores cerâmicos 100 nF próximos aos CIs (recomendado)  
- Fusível e fios decentes para a corrente da carga

---

## 3. Topologia da Ponte H

MOSFETs:

- **Q1**: high-side esquerdo  
- **Q2**: low-side esquerdo  
- **Q3**: high-side direito  
- **Q4**: low-side direito  

Conexões de potência:

- Dreno de Q1 e Q3 → **+Vdc**  
- Fonte de Q2 e Q4 → **GND**  
- Nó entre Q1–Q2 → **Saída A** (para carga)  
- Nó entre Q3–Q4 → **Saída B** (para carga)  
- Carga (ex: motor) entre **A** e **B**

Capacitor de barramento:

- **330 µF** entre +Vdc e GND, o mais perto possível dos MOSFETs.

---

## 4. Ligação dos IR2110

Cada IR2110 controla **meia ponte** (um high-side + um low-side).

### 4.1. Pinos principais (por meia ponte)

- `VCC`: +12 V (driver)  
- `COM`: GND (comum com Arduino e barramento)  
- `VDD`: 5 V (lógica, do Arduino)  
- `IN`/`HIN` / `LIN`: entradas de controle (do Arduino)  
- `HO`: saída para gate do MOSFET high-side (via resistor de 15 Ω)  
- `LO`: saída para gate do MOSFET low-side (via resistor de 15 Ω)  
- `VS`: nó da meia ponte (fonte do high-side / dreno do low-side)  
- `VB`: bootstrap (VB = VS + ~10–12 V)

### 4.2. Bootstrap (para cada IR2110)

- **Diodo UF4001**:
  - Ânodo em `VCC`
  - Cátodo em `VB`
- **Capacitor 10 µF** entre `VB` e `VS`

Isso forma a fonte de alta do high-side.

### 4.3. Desacoplamento

- 10 µF entre `VCC` e `COM` (driver)  
- Se tiver, adicionar 100 nF cerâmico em paralelo (recomendado)

---

## 5. Conexão com o Arduino

Sugestão de pinos (pode ajustar depois):

- IR2110 #1 (meia ponte A – Q1/Q2)
  - `HIN_A` → Arduino D9  
  - `LIN_A` → Arduino D10
- IR2110 #2 (meia ponte B – Q3/Q4)
  - `HIN_B` → Arduino D5  
  - `LIN_B` → Arduino D6

Todos os `COM` dos IR2110 e o GND do Arduino **ligados juntos**.

Resistores de gate:

- 15 Ω em série entre `HO` → gate high-side  
- 15 Ω em série entre `LO` → gate low-side  

Pull-downs (recomendado, não estavam na lista):

- 10 kΩ do gate de cada MOSFET para a respectiva referência (VS para high-side, GND para low-side)

---

## 6. Firmware (Arduino)

Arquivo sugerido:  
`firmware/arduino_inversor_dc/arduino_inversor_dc.ino`

Exemplo simples de **onda quadrada** com **dead-time grosseiro** para testes iniciais em baixa tensão e carga resistiva:

```cpp
// Controle simples de ponte H com 2x IR2110
// TESTE INICIAL EM BAIXA TENSÃO (12–24 V) E CARGA RESISTIVA

// Pinos do Arduino
const int HIN_A = 9;   // IR2110 #1 - high-side A
const int LIN_A = 10;  // IR2110 #1 - low-side A
const int HIN_B = 5;   // IR2110 #2 - high-side B
const int LIN_B = 6;   // IR2110 #2 - low-side B

// Frequência da "saída AC" (em Hz)
const float F_OUT = 50.0;   // exemplo 50 Hz
// Dead-time bruto (µs)
const unsigned int DEAD_TIME_US = 5;  // ajustar depois conforme MOSFET

void setup() {
  pinMode(HIN_A, OUTPUT);
  pinMode(LIN_A, OUTPUT);
  pinMode(HIN_B, OUTPUT);
  pinMode(LIN_B, OUTPUT);

  // Garante tudo desligado no início
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, LOW);
}

// Função para desligar todos os MOSFETs
void allOff() {
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, LOW);
}

// Um "meio ciclo": A positivo, B negativo
void halfCyclePositivo() {
  allOff();
  delayMicroseconds(DEAD_TIME_US);

  // A = +V (Q1 on), B = GND (Q4 on)
  digitalWrite(HIN_A, HIGH);  // high-side A
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, HIGH);  // low-side B
}

// Outro "meio ciclo": A negativo, B positivo
void halfCycleNegativo() {
  allOff();
  delayMicroseconds(DEAD_TIME_US);

  // A = GND (Q2 on), B = +V (Q3 on)
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, HIGH);  // low-side A
  digitalWrite(HIN_B, HIGH);  // high-side B
  digitalWrite(LIN_B, LOW);
}

void loop() {
  // Período total (50 Hz -> 20 ms)
  const float T = 1.0 / F_OUT;       // em segundos
  const unsigned long halfPeriod_us = (unsigned long)((T / 2.0) * 1e6);

  halfCyclePositivo();
  delayMicroseconds(halfPeriod_us);

  halfCycleNegativo();
  delayMicroseconds(halfPeriod_us);
}
```

> Esse código é **didático**:  
> - Onda quadrada pura, sem PWM senoidal.  
> - Dead-time tosco com `delayMicroseconds`.  
> Serve para testar se a ponte H está chaveando certo, **em baixa tensão**.

Depois pode evoluir para:

- PWM de alta frequência + modulação senoidal (SPWM)  
- Medição de corrente / proteção  
- Rampas de frequência para motor

---

## 7. Passo a passo de montagem

1. **Montar a parte de potência (Ponte H)**
   - Fixar os 4 MOSFETs em dissipador (se necessário).
   - Ligar drenos high-side ao +Vdc, fontes low-side ao GND.
   - Formar as saídas A e B nos nós das meias pontes.
   - Instalar o capacitor de 330 µF no barramento (entre +Vdc e GND).

2. **Montar os IR2110**
   - Soldar/desenhar `VCC→+12 V`, `COM→GND`, `VDD→5 V`.
   - Colocar os capacitores de 10 µF entre VCC–COM (desacoplamento).
   - Conectar HO/LO aos gates via resistores de 15 Ω.
   - Ligar VS nos nós das meias pontes, VB com diodo UF4001 + capacitor de bootstrap.

3. **Conectar o Arduino**
   - Ligar GND do Arduino ao GND de potência.
   - Conectar os pinos HIN/LIN aos pinos digitais definidos no código.
   - Carregar o firmware de teste.

4. **Testar sem carga**
   - Alimentar somente lógica (5 V + 12 V driver).
   - Confirmar que as saídas de gate estão comutando com o osciloscópio (ideal) ou multímetro (parcial).
   - Depois ligar a fonte de barramento (12–24 V) com **carga resistiva pequena**.

5. **Testar com carga**
   - Colocar uma resistência de potência entre A e B.
   - Verificar aquecimento dos MOSFETs.
   - Só depois pensar em motor.

---

## 8. Próximos passos

- Adicionar versão SPWM (tabela de seno + PWM rápido).
- Documentar resultados em `docs/testes_e_notas.md`.
- Desenhar o esquema bonitinho e colocar em `hardware/ponte_h_ir2110.md` ou PDF.

---

## Licença

Escolha a licença que preferir (MIT, GPL, etc.) e informe aqui.
