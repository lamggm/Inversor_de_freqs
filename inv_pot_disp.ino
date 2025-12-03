#include <Wire.h>
#include <U8g2lib.h>

// ---------- OLED SH1107 128x128 I2C ----------
// Modo paginado (_1_) pra caber no UNO
U8G2_SH1107_128X128_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- Pinos da ponte H ----------
const uint8_t HIN_A = 9;    // IR2110 #1 - high-side A
const uint8_t LIN_A = 10;   // IR2110 #1 - low-side A
const uint8_t HIN_B = 3;    // IR2110 #2 - high-side B
const uint8_t LIN_B = 11;   // IR2110 #2 - low-side B

// ---------- Potenciômetro ----------
const uint8_t POT_PIN = A0; // pot entre 5V e GND, pino central em A0

// ---------- Parâmetros de tempo ----------
const uint16_t T_STEP_MIN_MS = 2;    // mais rápido  (~125 Hz)
const uint16_t T_STEP_MAX_MS = 80;   // mais lento   (~3 Hz)

// Esses são usados no ISR, então voláteis:
volatile uint16_t T_step_ms = 10;    // passo atual (ms)
volatile float    freqHz    = 0.0f;  // freq aproximada
volatile int      potRaw    = 0;     // leitura do pot

// ---------- Máquina de estados da onda ----------
enum Phase : uint8_t {
  PH_PLUS12 = 0,   // A > B  -> +12 V
  PH_ZERO1  = 1,   // 0 V
  PH_MINUS12= 2,   // A < B  -> -12 V
  PH_ZERO2  = 3    // 0 V
};

volatile Phase currentPhase = PH_PLUS12;
volatile uint16_t tickCounter = 0;   // conta ticks de 1 ms

// ---------- Funções da ponte H (usadas no ISR) ----------

inline void allOff_fast() {
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, LOW);
}

// A = +12, B = 0  -> Vcarga = +12 V
inline void phase_plus12_fast() {
  allOff_fast();
  delayMicroseconds(5);      // deadtime curto

  digitalWrite(HIN_A, HIGH); // nó A = +12 V (high-side A ON)
  digitalWrite(LIN_A, LOW);  // low-side A OFF

  digitalWrite(HIN_B, LOW);  // high-side B OFF
  digitalWrite(LIN_B, HIGH); // nó B = 0 V (low-side B ON)
}

// 0 V (carga "solta")
inline void phase_zero_fast() {
  allOff_fast();
}

// A = 0, B = +12 -> Vcarga = -12 V
inline void phase_minus12_fast() {
  allOff_fast();
  delayMicroseconds(5);      // deadtime curto

  digitalWrite(HIN_A, LOW);  // nó A = 0 V (low-side A ON)
  digitalWrite(LIN_A, HIGH);

  digitalWrite(HIN_B, HIGH); // nó B = +12 V (high-side B ON)
  digitalWrite(LIN_B, LOW);  // low-side B OFF
}

inline void applyPhase_fast(Phase ph) {
  switch (ph) {
    case PH_PLUS12:
      phase_plus12_fast();
      break;
    case PH_ZERO1:
    case PH_ZERO2:
      phase_zero_fast();
      break;
    case PH_MINUS12:
      phase_minus12_fast();
      break;
  }
}

// ---------- Timer1: tick de 1 ms pro inversor ----------
// F_CPU = 16 MHz, presc = 64, OCR1A = 249 -> 1 kHz (1 ms)
void setupTimer1_tick1ms() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 249;               // 1 ms
  TCCR1B |= (1 << WGM12);    // CTC
  TCCR1B |= (1 << CS11) | (1 << CS10); // prescaler 64

  TIMSK1 |= (1 << OCIE1A);   // habilita interrupção compare A
  sei();
}

// ---------- ISR: inversor manda em tudo ----------
ISR(TIMER1_COMPA_vect) {
  tickCounter++;

  uint16_t localStep = T_step_ms; // lê uma vez

  if (tickCounter >= localStep) {
    tickCounter = 0;

    // avança fase
    Phase ph = currentPhase;
    switch (ph) {
      case PH_PLUS12:
        ph = PH_ZERO1;
        break;
      case PH_ZERO1:
        ph = PH_MINUS12;
        break;
      case PH_MINUS12:
        ph = PH_ZERO2;
        break;
      case PH_ZERO2:
      default:
        ph = PH_PLUS12;
        break;
    }
    currentPhase = ph;
    applyPhase_fast(ph);
  }
}

// ---------- Desenho no OLED (modo paginado) ----------

void desenhaOLED() {
  char buf[32];

  float fLocal;
  uint16_t stepLocal;
  int potLocal;

  // copia voláteis pra variáveis locais (pra não ficar lendo no meio do desenho)
  noInterrupts();
  fLocal    = freqHz;
  stepLocal = T_step_ms;
  potLocal  = potRaw;
  interrupts();

  u8g2.firstPage();
  do {
    // Título
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 10, "Inversor 3 niveis");
    u8g2.drawHLine(0, 13, 128);

    // Frequência
    snprintf(buf, sizeof(buf), "F = %5.1f Hz", fLocal);
    u8g2.drawStr(0, 28, buf);

    // T_step
    snprintf(buf, sizeof(buf), "T = %3u ms", stepLocal);
    u8g2.drawStr(0, 42, buf);

    // Pot bruto
    snprintf(buf, sizeof(buf), "POT = %4d", potLocal);
    u8g2.drawStr(0, 56, buf);

    // Barrinha de nível
    u8g2.drawStr(0, 74, "Nivel:");
    uint8_t w = map(potLocal, 0, 1023, 0, 100);
    u8g2.drawFrame(40, 66, 100, 12);   // moldura
    u8g2.drawBox(40, 66, w, 12);       // barra

    // Status
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 92, "Saida: +12 / 0 / -12");
    u8g2.drawStr(0, 104,"Carga entre A-B");

  } while (u8g2.nextPage());
}

// ---------- Setup / Loop ----------

void setup() {
  // Ponte H
  pinMode(HIN_A, OUTPUT);
  pinMode(LIN_A, OUTPUT);
  pinMode(HIN_B, OUTPUT);
  pinMode(LIN_B, OUTPUT);
  allOff_fast();

  // Pot
  pinMode(POT_PIN, INPUT);

  // OLED
  u8g2.begin();
  // Se não aparecer nada, testa:
  // u8g2.setI2CAddress(0x3C * 2); ou 0x3D * 2;

  // Tela inicial
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 20, "Inversor Ponte H");
    u8g2.drawStr(0, 36, "OLED SH1107 OK");
  } while (u8g2.nextPage());

  delay(500);

  // Inicia fase e timer do inversor
  currentPhase = PH_PLUS12;
  applyPhase_fast(currentPhase);
  tickCounter = 0;
  setupTimer1_tick1ms();
}

void loop() {
  unsigned long nowMs = millis();

  // 1) Lê pot e atualiza T_step/freq (devagar, fora do ISR)
  static unsigned long lastPotMs = 0;
  if (nowMs - lastPotMs >= 50) {
    lastPotMs = nowMs;

    int raw = analogRead(POT_PIN);  // 0..1023

    uint16_t newStep = map(raw, 0, 1023, T_STEP_MIN_MS, T_STEP_MAX_MS);
    float periodo_ms = 4.0f * (float)newStep;
    float newFreq = (periodo_ms > 0.0f) ? (1000.0f / periodo_ms) : 0.0f;

    noInterrupts();
    potRaw   = raw;
    T_step_ms = newStep;
    freqHz   = newFreq;
    interrupts();
  }

  // 2) Atualiza OLED só "quando der" (isso aqui pode travar, ISR cuida do resto)
  static unsigned long lastOledMs = 0;
  if (nowMs - lastOledMs >= 150) {
    lastOledMs = nowMs;
    desenhaOLED();
  }

  // loop principal não encosta na ponte H; só ISR manda.
}
