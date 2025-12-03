#include <Wire.h>
#include <U8g2lib.h>

// ---------- OLED SH1107 128x128 I2C ----------
U8G2_SH1107_128X128_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- Pinos da ponte H ----------
const uint8_t HIN_A = 9;
const uint8_t LIN_A = 10;
const uint8_t HIN_B = 3;
const uint8_t LIN_B = 11;

// ---------- Pot ----------
const uint8_t POT_PIN = A0;

// ---------- Parâmetros ----------
const uint16_t T_STEP_MIN_MS = 2;
const uint16_t T_STEP_MAX_MS = 80;

// agora softstart de 5 s
const uint16_t SOFTSTART_TIME_MS = 5000;  // rampa total em ms

volatile uint16_t T_step_ms = 10;
volatile float    freqHz    = 0.0f;
volatile int      potRaw    = 0;

enum Phase : uint8_t {
  PH_PLUS12 = 0,
  PH_ZERO1  = 1,
  PH_MINUS12= 2,
  PH_ZERO2  = 3
};

volatile Phase   currentPhase = PH_PLUS12;
volatile uint16_t tickCounter = 0;

// soft-starter: 0…100 %
volatile uint16_t soft_m = 0;
unsigned long     softStartBeginMs = 0;

// ---------- Funções rápidas (ISR-safe) ----------
inline void allOff_fast() {
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, LOW);
}

inline void phase_plus12_fast() {
  allOff_fast();
  delayMicroseconds(5);
  digitalWrite(HIN_A, HIGH);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, HIGH);
}

inline void phase_zero_fast() {
  allOff_fast();
}

inline void phase_minus12_fast() {
  allOff_fast();
  delayMicroseconds(5);
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, HIGH);
  digitalWrite(HIN_B, HIGH);
  digitalWrite(LIN_B, LOW);
}

inline void applyPhase_full(Phase ph) {
  switch (ph) {
    case PH_PLUS12:  phase_plus12_fast();  break;
    case PH_ZERO1:
    case PH_ZERO2:   phase_zero_fast();    break;
    case PH_MINUS12: phase_minus12_fast(); break;
  }
}

// aplica softstart: m = 0..100 %
inline void applyPhase_softened(Phase ph, uint16_t m) {
  if (m >= 100) {
    applyPhase_full(ph);
    return;
  }
  if (m == 0) {
    phase_zero_fast();
    return;
  }

  // pseudo-PWM estocástico: quanto maior m, mais tempo “ativo”
  uint32_t hash = micros() + ph * 97;
  uint8_t rnd = (hash & 0xFF);

  if (rnd < (m * 255UL) / 100UL) {
    applyPhase_full(ph);
  } else {
    phase_zero_fast();
  }
}

// ---------- ISR: inversor manda em tudo ----------
ISR(TIMER1_COMPA_vect) {
  tickCounter++;

  uint16_t localStep = T_step_ms;

  if (tickCounter >= localStep) {
    tickCounter = 0;

    Phase ph = currentPhase;
    switch (ph) {
      case PH_PLUS12:  ph = PH_ZERO1;  break;
      case PH_ZERO1:   ph = PH_MINUS12;break;
      case PH_MINUS12: ph = PH_ZERO2;  break;
      case PH_ZERO2:
      default:         ph = PH_PLUS12; break;
    }
    currentPhase = ph;

    uint16_t m = soft_m;
    applyPhase_softened(ph, m);
  }
}

// ---------- Timer 1 ms ----------
void setupTimer1_tick1ms() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // 1 kHz: OCR1A = 249 com prescaler 64
  OCR1A = 249;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

// ---------- OLED ----------
void desenhaOLED() {
  char buf[32];

  float    fLocal;
  uint16_t stepLocal;
  int      potLocal;
  uint16_t mLocal;

  noInterrupts();
  fLocal    = freqHz;
  stepLocal = T_step_ms;
  potLocal  = potRaw;
  mLocal    = soft_m;
  interrupts();

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setFontPosTop();                // garante y = topo

    u8g2.drawStr(33, 0,  "Inversor PABLIN");   // linha 0
    u8g2.drawHLine(0, 14, 128);               // logo abaixo (12 px da fonte + margenzinha)

    snprintf(buf, sizeof(buf), "F = %5.1f Hz", fLocal);
    u8g2.drawStr(33, 18, buf);                // linha 2

    snprintf(buf, sizeof(buf), "T = %3u ms", stepLocal);
    u8g2.drawStr(33, 32, buf);                // linha 3

    snprintf(buf, sizeof(buf), "POT = %4d", potLocal);
    u8g2.drawStr(33, 46, buf);                // linha 4

    snprintf(buf, sizeof(buf), "Soft = %3u%%", mLocal);
    u8g2.drawStr(33, 60, buf);                // linha 5

    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setFontPosTop();

    u8g2.drawStr(33, 80,  "SOFTst: 5 s");
    u8g2.drawStr(33, 90,  "Saida: +12 / 0 / -12");

  } while (u8g2.nextPage());
}


// ---------- Setup ----------
void setup() {
  pinMode(HIN_A, OUTPUT);
  pinMode(LIN_A, OUTPUT);
  pinMode(HIN_B, OUTPUT);
  pinMode(LIN_B, OUTPUT);
  allOff_fast();

  pinMode(POT_PIN, INPUT);

  u8g2.begin();

  // Tela 1: identificação
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(33, 20, "Inversor de Freq");
    u8g2.drawStr(33, 36, "BOMBOOCLAT");
    u8g2.drawStr(33, 50, "P. Pablo");
  } while (u8g2.nextPage());
  delay(2000);

  // Tela 2: SOFTSTARTER antes do funcionamento principal
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(33, 30, "SOFTST");
    u8g2.drawStr(33, 46, "5s");
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(33, 62, "Saida inicia em 0%");
    u8g2.drawStr(33, 72, "e sobe ate 100%");
  } while (u8g2.nextPage());
  delay(800);

  // Estado inicial da ponte
  currentPhase = PH_PLUS12;
  applyPhase_full(currentPhase);

  // Softstart começa em 0% AGORA
  soft_m = 0;
  softStartBeginMs = millis();

  setupTimer1_tick1ms();
}

// ---------- Loop ----------
void loop() {
  unsigned long nowMs = millis();

  // Atualiza pot / T_step / freq (fora do ISR)
  static unsigned long lastPotMs = 0;
  if (nowMs - lastPotMs >= 60) {
    lastPotMs = nowMs;

    int raw = analogRead(POT_PIN);

    uint16_t newStep = map(raw, 0, 1023, T_STEP_MIN_MS, T_STEP_MAX_MS);
    float periodo_ms = 4.0f * (float)newStep;
    float newFreq = (periodo_ms > 0.0f) ? (1000.0f / periodo_ms) : 0.0f;

    noInterrupts();
    potRaw    = raw;
    T_step_ms = newStep;
    freqHz    = newFreq;
    interrupts();
  }

  // Soft-starter: 0 → 100% em SOFTSTART_TIME_MS
  uint32_t dt = nowMs - softStartBeginMs;
  if (dt < SOFTSTART_TIME_MS) {
    uint16_t m = (dt * 100UL) / SOFTSTART_TIME_MS;
    if (m > 100) m = 100;
    noInterrupts();
    soft_m = m;
    interrupts();
  } else {
    noInterrupts();
    soft_m = 100;
    interrupts();
  }

  // Atualiza OLED de vez em quando
  static unsigned long lastOledMs = 0;
  if (nowMs - lastOledMs >= 150) {
    lastOledMs = nowMs;
    desenhaOLED();
  }
}
