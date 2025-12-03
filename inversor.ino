// Onda quadrada 3 níveis na carga: +12 -> 0 -> -12 -> 0 -> ...
// Com controle de frequência via potenciômetro em A0

const uint8_t HIN_A = 9;    // IR2110 #1 - high-side A
const uint8_t LIN_A = 10;   // IR2110 #1 - low-side A
const uint8_t HIN_B = 3;    // IR2110 #2 - high-side B
const uint8_t LIN_B = 11;   // IR2110 #2 - low-side B

const uint8_t POT_PIN = A0; // potenciômetro

// Limites do tempo de cada nível (em ms)
// Período de 1 ciclo = 4 * T_step_ms
// Ex: T_step = 5ms -> período 20ms -> 50 Hz aprox.
const uint16_t T_STEP_MIN_MS = 2;   // mais rápido (~125 Hz)
const uint16_t T_STEP_MAX_MS = 80;  // mais lento (~3 Hz)

uint16_t T_step_ms = 10; // valor inicial

void allOff() {
  // Desliga todos os MOSFETs (segurança + "deadtime")
  digitalWrite(HIN_A, LOW);
  digitalWrite(LIN_A, LOW);
  digitalWrite(HIN_B, LOW);
  digitalWrite(LIN_B, LOW);
}

// Estado 1: A = +12, B = 0  -> Vcarga = +12 V
void estado_plus12() {
  allOff();
  delayMicroseconds(5);      // deadtime pequeno

  digitalWrite(HIN_A, HIGH); // high-side A ON  (nó A = +12V)
  digitalWrite(LIN_A, LOW);  // low-side A OFF

  digitalWrite(HIN_B, LOW);  // high-side B OFF
  digitalWrite(LIN_B, HIGH); // low-side B ON  (nó B = 0V)
}

// Estado 2: tudo OFF -> Vcarga ≈ 0 V
void estado_zero() {
  allOff();
}

// Estado 3: A = 0, B = +12 -> Vcarga = -12 V
void estado_minus12() {
  allOff();
  delayMicroseconds(5);      // deadtime pequeno

  digitalWrite(HIN_A, LOW);  // high-side A OFF
  digitalWrite(LIN_A, HIGH); // low-side A ON  (nó A = 0V)

  digitalWrite(HIN_B, HIGH); // high-side B ON (nó B = +12V)
  digitalWrite(LIN_B, LOW);  // low-side B OFF
}

void setup() {
  pinMode(HIN_A, OUTPUT);
  pinMode(LIN_A, OUTPUT);
  pinMode(HIN_B, OUTPUT);
  pinMode(LIN_B, OUTPUT);

  pinMode(POT_PIN, INPUT);

  allOff();
}

void loop() {
  // Lê o potenciômetro e calcula o tempo de cada nível
  int potRaw = analogRead(POT_PIN);  // 0..1023
  // Mapeia para intervalo [T_STEP_MIN_MS, T_STEP_MAX_MS]
  T_step_ms = map(potRaw, 0, 1023, T_STEP_MIN_MS, T_STEP_MAX_MS);

  // +12 V na carga (A > B)
  estado_plus12();
  delay(T_step_ms);

  // 0 V na carga (A ≈ B)
  estado_zero();
  delay(T_step_ms);

  // -12 V na carga (B > A)
  estado_minus12();
  delay(T_step_ms);

  // 0 V de novo
  estado_zero();
  delay(T_step_ms);
}
