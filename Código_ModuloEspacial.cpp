// ============================================================
//  MÓDULO CÁPSULA ESPACIAL — Sistema de Monitoramento
//  Arduino Uno | TinkerCAD + Hardware Real
// ============================================================
//  Sensores:
//    TMP36            → A0 (Temperatura)
//    Phototransistor  → A1 (Luminosidade)
//    Gas Sensor       → A2 (Concentração de Gás)
//    Tilt SW-200      → D7 (Inclinação)
//  Saídas:
//    LCD 16x2 (4-bit) → D2, D3, D4, D5, D11, D12
//    Buzzer           → D8
// ============================================================

#include <LiquidCrystal.h>

// ── PINOS ────────────────────────────────────────────────────
const int PIN_TEMP    = A0;
const int PIN_LUZ     = A1;
const int PIN_GAS     = A2;
const int PIN_TILT    = 7;
const int PIN_BUZZER  = 8;

// LCD: RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// ── LIMITES DE ALARME ────────────────────────────────────────
const float LIMITE_TEMP  = 95.0;   // °C
const float LIMITE_LUZ   = 75.0;   // %
const float LIMITE_GAS   = 30.0;   // %
// Tilt: 100% quando inclinado → 100 > 90 → alarme

// ── CONTROLE DE DISPLAY ──────────────────────────────────────
int modoDisplay = 0;                // 0=Temp, 1=Luz, 2=Inclinação
unsigned long tUltimoDisplay = 0;
const unsigned long INTERVALO_DISPLAY = 2000; // 5 segundos

// ── CONTROLE DE BUZZER ───────────────────────────────────────
unsigned long tUltimoBip = 0;
bool estadoBuzzer = false;
const unsigned long INTERVALO_BIP = 600; // ms entre bips

// ============================================================
// FUNÇÕES DE LEITURA
// ============================================================

float lerTemperatura() {
  int raw = analogRead(PIN_TEMP);
  // TMP36: Vout = 500mV + 10mV/°C
  float tensao = raw * (5.0 / 1023.0);
  return (tensao - 0.5) * 100.0;
}

float lerLuminosidade() {
  int raw = analogRead(PIN_LUZ);
  float pct = (raw / 1023.0) * 100.0;
  return constrain(pct, 0.0, 100.0);
}

float lerGas() {
  int raw = analogRead(PIN_GAS);
  float pct = (raw / 1023.0) * 100.0;
  return constrain(pct, 0.0, 100.0);
}

bool lerInclinacao() {
  // INPUT_PULLUP: LOW = inclinado (circuito fechado)
  return (digitalRead(PIN_TILT) == LOW);
}

// ============================================================
// FUNÇÕES DE BUZZER
// ============================================================

void atualizarBuzzer(bool alarmeAtivo) {
  if (!alarmeAtivo) {
    digitalWrite(PIN_BUZZER, LOW);
    estadoBuzzer = false;
    return;
  }
  unsigned long agora = millis();
  if (agora - tUltimoBip >= INTERVALO_BIP) {
    tUltimoBip = agora;
    estadoBuzzer = !estadoBuzzer;
    digitalWrite(PIN_BUZZER, estadoBuzzer ? HIGH : LOW);
  }
}

// ============================================================
// EXIBIÇÃO NORMAL (rotação a cada 5s)
// ============================================================

void exibirMedida(float temp, float luz, float tiltPct) {
  unsigned long agora = millis();
  if (agora - tUltimoDisplay < INTERVALO_DISPLAY) return;

  tUltimoDisplay = agora;
  lcd.clear();

  switch (modoDisplay) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Temperatura:");
      lcd.setCursor(0, 1);
      lcd.print(temp, 1);
      lcd.print((char)223); // símbolo °
      lcd.print("C");
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Luminosidade:");
      lcd.setCursor(0, 1);
      lcd.print(luz, 1);
      lcd.print("%");
      break;

    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Inclinacao:");
      lcd.setCursor(0, 1);
      lcd.print(tiltPct, 0);
      lcd.print("%");
      break;
  }

  modoDisplay = (modoDisplay + 1) % 3;
}

// ============================================================
// EXIBIÇÃO DE ALARME
// ============================================================

String alarmeAnterior = "";

void exibirAlarme(String linha2) {
  if (linha2 == alarmeAnterior) return; // evita flickering
  alarmeAnterior = linha2;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*** ALERTA ***");
  lcd.setCursor(0, 1);
  lcd.print(linha2);
}

void limparAlarme() {
  if (alarmeAnterior != "") {
    alarmeAnterior = "";
    lcd.clear();
    tUltimoDisplay = 0; // força redesenho imediato
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);

  pinMode(PIN_TILT,   INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Tela de boot
  lcd.setCursor(0, 0);
  lcd.print("  CAPSULA 2025  ");
  lcd.setCursor(0, 1);
  lcd.print(" Inicializando  ");

  // Bip duplo de inicialização
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH); delay(120);
    digitalWrite(PIN_BUZZER, LOW);  delay(120);
  }
  delay(2000);
  lcd.clear();

  Serial.println("=== CAPSULA ESPACIAL ONLINE ===");
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================

void loop() {
  // ── Leituras ──────────────────────────────────────────────
  float temp      = lerTemperatura();
  float luz       = lerLuminosidade();
  float gas       = lerGas();
  bool  inclinado = lerInclinacao();
  float tiltPct   = inclinado ? 100.0 : 0.0;

  // ── Log Serial (debug) ────────────────────────────────────
  Serial.print("Temp: ");  Serial.print(temp, 1);  Serial.print("C  |  ");
  Serial.print("Luz: ");   Serial.print(luz, 1);   Serial.print("%  |  ");
  Serial.print("Gas: ");   Serial.print(gas, 1);   Serial.print("%  |  ");
  Serial.print("Tilt: ");  Serial.println(inclinado ? "INCLINADO" : "Normal");

  // ── Avaliação de Alarmes (prioridade: Temp > Luz > Tilt > Gas)
  bool alarmeTemp  = (temp > LIMITE_TEMP);
  bool alarmeLuz   = (luz  > LIMITE_LUZ);
  bool alarmeStall = inclinado;
  bool alarmeGas   = (gas  > LIMITE_GAS);

  bool qualquerAlarme = alarmeTemp || alarmeLuz || alarmeStall || alarmeGas;

  if (qualquerAlarme) {
    String mensagem = "";
    if      (alarmeTemp)  mensagem = "Temp. Critica!  ";
    else if (alarmeLuz)   mensagem = "Altos UV!       ";
    else if (alarmeStall) mensagem = "Entrando STALL! ";
    else if (alarmeGas)   mensagem = "Altas Conc. Gas!";

    exibirAlarme(mensagem);
    atualizarBuzzer(true);

  } else {
    limparAlarme();
    atualizarBuzzer(false);
    exibirMedida(temp, luz, tiltPct);
  }
  delay(100);
}
