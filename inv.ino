// Inversor DC senoidal com 2x IR2110 + ponte H completa
// Gera SPWM (modulação senoidal por PWM) em baixa tensão (12–24 V)
// Usa tabela de seno e atualiza a cada SAMPLE_PERIOD_US para formar ~50 Hz

#include <Arduino.h>                   // Inclui a biblioteca padrão do Arduino

// ====== Definição dos pinos ======
const uint8_t HIN_A = 9;              // Pino 9 (OC1A) -> entrada HIN do IR2110 da perna A (high-side A)
const uint8_t LIN_A = 10;             // Pino 10 (OC1B) -> entrada LIN do IR2110 da perna A (low-side A)
const uint8_t HIN_B = 3;              // Pino 3 (OC2B) -> entrada HIN do IR2110 da perna B (high-side B)
const uint8_t LIN_B = 11;             // Pino 11 (OC2A) -> entrada LIN do IR2110 da perna B (low-side B)

// ====== Parâmetros da senoide ======
const float F_OUT = 50.0f;            // Frequência fundamental desejada na saída (Hz) -> aqui 50 Hz
const uint16_t SINE_STEPS = 256;      // Número de amostras da tabela de seno (resolução angular da onda)
// A frequência de atualização da tabela é Fs = F_OUT * SINE_STEPS
const float FS = F_OUT * SINE_STEPS;  // Frequência de amostragem da senoide (Hz)
// A partir da frequência de amostragem, calculamos o período da amostra em microssegundos
const uint16_t SAMPLE_PERIOD_US = (uint16_t)(1e6f / FS);  // Período entre amostras (µs)

// ====== Tabela de seno: 256 amostras no intervalo -127 .. +127 ======
// Cada valor é um int8_t (8 bits com sinal) representando o valor relativo da senoide
int8_t sineLUT[SINE_STEPS] = {
  0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 35, 38, 41, 44, 47,
  50, 53, 56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 85, 88, 91, 94,
  96, 99, 102, 104, 107, 109, 112, 114, 116, 119, 121, 123, 125, 127, 127, 128,
  128, 129, 129, 129, 129, 129, 129, 128, 128, 127, 127, 125, 123, 121, 119, 116,
  114, 112, 109, 107, 104, 102, 99, 96, 94, 91, 88, 85, 83, 80, 77, 74,
  71, 68, 65, 62, 59, 56, 53, 50, 47, 44, 41, 38, 35, 31, 28, 25,
  22, 19, 16, 12, 9, 6, 3, 0, -3, -6, -9, -12, -16, -19, -22, -25,
  -28, -31, -35, -38, -41, -44, -47, -50, -53, -56, -59, -62, -65, -68, -71, -74,
  -77, -80, -83, -85, -88, -91, -94, -96, -99, -102, -104, -107, -109, -112, -114, -116,
  -119, -121, -123, -125, -127, -127, -128, -128, -129, -129, -129, -129, -129, -129, -128, -128,
  -127, -127, -125, -123, -121, -119, -116, -114, -112, -109, -107, -104, -102, -99, -96, -94,
  -91, -88, -85, -83, -80, -77, -74, -71, -68, -65, -62, -59, -56, -53, -50, -47,
  -44, -41, -38, -35, -31, -28, -25, -22, -19, -16, -12, -9, -6, -3, 0, 3,
  6, 9, 12, 16, 19, 22, 25, 28, 31, 35, 38, 41, 44, 47, 50, 53,
  56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 85, 88, 91, 94, 96, 99,
  102, 104, 107, 109, 112, 114, 116, 119, 121, 123, 125, 127, 127, 128, 128, 129
};

// Índice atual na tabela de seno (0 .. SINE_STEPS-1)
uint16_t phaseIndex = 0;              // Começa na fase zero da senoide

// ====== Função auxiliar: controla uma perna da ponte H ======
// highPin: pino ligado à entrada HIN do IR2110 (high-side)
// lowPin:  pino ligado à entrada LIN do IR2110 (low-side)
// value: valor da senoide nessa perna, no intervalo -127 .. +127
inline void driveLeg(uint8_t highPin, uint8_t lowPin, int8_t value) {
  uint8_t mag = (uint8_t)( (value >= 0 ? value : -value) * 2 ); // Calcula a magnitude absoluta do valor (0..127) e escala para 0..254

  if (value >= 0) {                     // Se o valor da senoide for positivo ou zero
    analogWrite(lowPin, 0);            // Garante que o low-side dessa perna esteja totalmente desligado (duty = 0)
    analogWrite(highPin, mag);         // Aplica PWM no high-side com duty proporcional à magnitude
  } else {                             // Se o valor da senoide for negativo
    analogWrite(highPin, 0);           // Garante que o high-side dessa perna esteja desligado
    analogWrite(lowPin, mag);          // Aplica PWM no low-side com duty proporcional à magnitude
  }
}

// ====== Configuração dos temporizadores para PWM rápido ======
void setupFastPWM() {
  // ------- Configuração do Timer1 (pinos 9 e 10) -------
  TCCR1A = 0;                          // Zera o registrador de controle A do Timer1
  TCCR1B = 0;                          // Zera o registrador de controle B do Timer1
  TCCR1A |= (1 << WGM10);              // Configura Timer1 em modo Phase Correct PWM de 8 bits (WGM10 = 1)
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1); // Habilita saída PWM não-invertida em OC1A (pino 9) e OC1B (pino 10)
  TCCR1B |= (1 << CS10);               // Define prescaler = 1 para o Timer1 (CS10 = 1)

  // Nesse modo, a frequência de PWM do Timer1 é aproximadamente:
  // F_pwm1 ≈ 16 MHz / (2 * 255) ≈ 31.37 kHz

  // ------- Configuração do Timer2 (pinos 11 e 3) -------
  TCCR2A = 0;                          // Zera o registrador de controle A do Timer2
  TCCR2B = 0;                          // Zera o registrador de controle B do Timer2
  TCCR2A |= (1 << WGM20) | (1 << WGM21); // Configura Timer2 em modo Fast PWM 8 bits (WGM20 = 1, WGM21 = 1)
  TCCR2A |= (1 << COM2A1) | (1 << COM2B1); // Habilita saída PWM não-invertida em OC2A (pino 11) e OC2B (pino 3)
  TCCR2B |= (1 << CS20);               // Define prescaler = 1 para o Timer2 (CS20 = 1)

  // Nesse modo, a frequência de PWM do Timer2 é aproximadamente:
  // F_pwm2 ≈ 16 MHz / 256 ≈ 62.5 kHz
}

// ====== Função setup() ======
void setup() {
  pinMode(HIN_A, OUTPUT);             // Configura pino HIN_A (9) como saída digital
  pinMode(LIN_A, OUTPUT);             // Configura pino LIN_A (10) como saída digital
  pinMode(HIN_B, OUTPUT);             // Configura pino HIN_B (3) como saída digital
  pinMode(LIN_B, OUTPUT);             // Configura pino LIN_B (11) como saída digital

  analogWrite(HIN_A, 0);              // Garante que o PWM do pino HIN_A comece desligado (duty 0)
  analogWrite(LIN_A, 0);              // Garante que o PWM do pino LIN_A comece desligado
  analogWrite(HIN_B, 0);              // Garante que o PWM do pino HIN_B comece desligado
  analogWrite(LIN_B, 0);              // Garante que o PWM do pino LIN_B comece desligado

  setupFastPWM();                     // Chama a função que configura Timer1 e Timer2 em modos de PWM rápido
}

// ====== Função loop(): atualiza a senoide continuamente ======
void loop() {
  int8_t vA = sineLUT[phaseIndex];    // Obtém o valor da senoide para a perna A no índice atual da tabela

  uint16_t idxB = (phaseIndex + (SINE_STEPS / 2)) & (SINE_STEPS - 1); // Calcula o índice para a perna B defasada de 180° (meia volta)
  int8_t vB = sineLUT[idxB];          // Obtém o valor da senoide para a perna B usando o índice defasado

  driveLeg(HIN_A, LIN_A, vA);         // Atualiza a perna A da ponte H com o valor vA (controlando high/low side)
  driveLeg(HIN_B, LIN_B, vB);         // Atualiza a perna B da ponte H com o valor vB (defasado 180°)

  phaseIndex = (phaseIndex + 1) & (SINE_STEPS - 1); // Incrementa o índice da tabela de seno e faz wrap para 0..255

  delayMicroseconds(SAMPLE_PERIOD_US); // Espera o tempo exato entre amostras para definir a frequência fundamental
}
