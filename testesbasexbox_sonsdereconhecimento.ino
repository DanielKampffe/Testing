#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

const int IN1 = 33; const int IN2 = 32; 
const int IN3 = 27; const int IN4 = 26; 
const int pinoBuzzer = 4; 
const int freq = 1000; const int res = 8;    

XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

int vEsqAtual = 0; int vDirAtual = 0;
bool estavaConectado = false; 

// AJUSTES DE PERFORMANCE
const int degrauSuaver = 20;       
const int limiteVelocidade = 200;  
const float calibracaoEsq = 0.85;  
const float zonaMorta = 0.12;

// VARIÁVEIS DE CONTROLE DE SOM
unsigned long tempoSomEvento = 0;
int tipoSomAtivo = 0; // 0: nenhum, 1: conexão, 2: desconexão

void acionarMotores(int vEsqAlvo, int vDirAlvo) {
  if (vEsqAlvo == 0) vEsqAtual = 0;
  if (vDirAlvo == 0) vDirAtual = 0;

  if (vEsqAlvo != 0) {
    if (abs(vEsqAlvo - vEsqAtual) < degrauSuaver) vEsqAtual = vEsqAlvo;
    else vEsqAtual += (vEsqAlvo > vEsqAtual) ? degrauSuaver : -degrauSuaver;
  }
  if (vDirAlvo != 0) {
    if (abs(vDirAlvo - vDirAtual) < degrauSuaver) vDirAtual = vDirAlvo;
    else vDirAtual += (vDirAlvo > vDirAtual) ? degrauSuaver : -degrauSuaver;
  }

  int vEsqFinal = vEsqAtual * calibracaoEsq;
  if (vEsqFinal > 0) { ledcWrite(IN2, abs(vEsqFinal)); ledcWrite(IN1, 0); }
  else if (vEsqFinal < 0) { ledcWrite(IN2, 0); ledcWrite(IN1, abs(vEsqFinal)); }
  else { ledcWrite(IN1, 0); ledcWrite(IN2, 0); }

  if (vDirAtual > 0) { ledcWrite(IN3, 0); ledcWrite(IN4, abs(vDirAtual)); }
  else if (vDirAtual < 0) { ledcWrite(IN3, abs(vDirAtual)); ledcWrite(IN4, 0); }
  else { ledcWrite(IN3, 0); ledcWrite(IN4, 0); }
}

// MÁQUINA DE ESTADOS PARA SONS DE EVENTO (SEM BLOQUEAR)
void gerenciarSons() {
  if (tipoSomAtivo == 0) return;

  unsigned long decorrido = millis() - tempoSomEvento;
  static int faseAtual = -1; // Variável para garantir que o tone só seja chamado UMA vez por nota

  if (tipoSomAtivo == 1) { // CONECTADO
    if (decorrido < 100 && faseAtual != 0) {
      tone(pinoBuzzer, 1000, 100); 
      faseAtual = 0;
    } 
    else if (decorrido >= 100 && decorrido < 200 && faseAtual != 1) {
      // Intervalo de silêncio (delay 100)
      faseAtual = 1;
    } 
    else if (decorrido >= 200 && decorrido < 300 && faseAtual != 2) {
      tone(pinoBuzzer, 1500, 100); // Toca a nota de 1500Hz UMA ÚNICA VEZ
      faseAtual = 2;
    } 
    else if (decorrido >= 300) {
      tipoSomAtivo = 0;
      faseAtual = -1;
    }
  } 
  else if (tipoSomAtivo == 2) { // DESCONECTADO
    if (decorrido < 200 && faseAtual != 0) {
      tone(pinoBuzzer, 800, 200);
      faseAtual = 0;
    } 
    else if (decorrido >= 200 && decorrido < 450 && faseAtual != 1) {
      faseAtual = 1;
    } 
    else if (decorrido >= 450 && decorrido < 850 && faseAtual != 2) {
      tone(pinoBuzzer, 400, 400);
      faseAtual = 2;
    } 
    else if (decorrido >= 850) {
      tipoSomAtivo = 0;
      faseAtual = -1;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(pinoBuzzer, OUTPUT);
  
  // SOM DE LIGAR SISTEMA (Pode ter delay aqui pois é antes de tudo)
  tone(pinoBuzzer, 600, 100); delay(150);
  tone(pinoBuzzer, 800, 100); delay(150);
  tone(pinoBuzzer, 1200, 200); delay(200);
  noTone(pinoBuzzer);

  ledcAttach(IN1, freq, res); ledcAttach(IN2, freq, res);
  ledcAttach(IN3, freq, res); ledcAttach(IN4, freq, res);
  xboxController.begin();
}

void loop() {
  xboxController.onLoop();
  bool conectadoAgora = xboxController.isConnected();

  // Detecção de status para disparar sons
  if (conectadoAgora && !estavaConectado) { 
    tipoSomAtivo = 1; tempoSomEvento = millis(); estavaConectado = true; 
  } 
  if (!conectadoAgora && estavaConectado) { 
    tipoSomAtivo = 2; tempoSomEvento = millis(); estavaConectado = false; 
  }

  gerenciarSons();

  if (conectadoAgora && !xboxController.isWaitingForFirstNotification()) {
    
    // --- BUZINA E SIRENE ---
    if (xboxController.xboxNotif.btnRS) {
      // SIRENE: Alterna rápido
      tone(pinoBuzzer, (millis() % 400 < 200) ? 700 : 1300);
    } 
    else if (xboxController.xboxNotif.btnLS) {
      // BUZINA: Agora contínua (se preferir bi-bi, use a lógica do millis anterior)
      tone(pinoBuzzer, 450); 
    } 
    else if (tipoSomAtivo == 0) {
      noTone(pinoBuzzer);
    }

    // Mixer de Movimento
    uint16_t joyMax = XboxControllerNotificationParser::maxJoy;
    uint16_t trigMax = XboxControllerNotificationParser::maxTrig;
    int vBase = ((int)xboxController.xboxNotif.trigRT * limiteVelocidade / trigMax) - 
                ((int)xboxController.xboxNotif.trigLT * limiteVelocidade / trigMax);

    float eixoX = ((float)xboxController.xboxNotif.joyLHori / joyMax) - 0.5;
    eixoX *= 2.0; 
    float curva = 0;
    if (abs(eixoX) > zonaMorta) {
      curva = (eixoX > 0) ? (eixoX - zonaMorta) / (1.0 - zonaMorta) : (eixoX + zonaMorta) / (1.0 - zonaMorta);
    }

    acionarMotores(constrain(vBase + (curva * limiteVelocidade), -limiteVelocidade, limiteVelocidade),
                   constrain(vBase - (curva * limiteVelocidade), -limiteVelocidade, limiteVelocidade));
  } else {
    acionarMotores(0, 0);
    if (tipoSomAtivo == 0) noTone(pinoBuzzer);
    if (xboxController.getCountFailedConnection() > 5) { delay(10); ESP.restart(); }
  }
  delay(5);
}
