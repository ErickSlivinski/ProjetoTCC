#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

static const char *AP_SSID = "FOGAO-ESP32";
static const char *AP_PASSWORD = "12345678";

static const int SERVO_PIN = 26;
static const bool INVERT_SERVO_DIRECTION = true;

// Valores iniciais. Podem ser alterados pela tela de calibracao enquanto o ESP32 estiver ligado.
static const int POS_FECHADO = 0;
static const int POS_FOGO_ALTO = 180;
static const int POS_FOGO_MEDIO = 120;
static const int POS_FOGO_BAIXO = 60;
static const uint32_t DEFAULT_MOVE_MS = 2500; // Movimento suave e lento para garantir torque maximo do servo

enum FlameLevel {
  NIVEL_DESCONHECIDO,
  NIVEL_FECHADO,
  NIVEL_ALTO,
  NIVEL_MEDIO,
  NIVEL_BAIXO,
  NIVEL_MANUAL
};

WebServer server(80);
Servo servoMotor;
Preferences preferences;

int currentDeg = POS_FECHADO;
int targetDeg = POS_FECHADO;
bool servoAttached = false;
bool moving = false;
bool rampActive = false;
int rampFromDeg = POS_FECHADO;
int rampToDeg = POS_FECHADO;
uint32_t rampStartMs = 0;
uint32_t rampDurationMs = 0;

bool timerRunning = false;
uint32_t timerStartMs = 0;
uint32_t timerDurationMs = 10UL * 60UL * 1000UL;
uint32_t timerDoneSeq = 0;
uint32_t lastSerialStatusMs = 0;
FlameLevel level = NIVEL_FECHADO;
int calFechado = POS_FECHADO;
int calAlto = POS_FOGO_ALTO;
int calMedio = POS_FOGO_MEDIO;
int calBaixo = POS_FOGO_BAIXO;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Fog&atilde;o - 1 man&iacute;pulo</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f4f2ec;
      --panel: #ffffff;
      --ink: #1f2933;
      --muted: #667085;
      --line: #d7dce2;
      --primary: #0f766e;
      --primary-dark: #115e59;
      --danger: #b42318;
      --danger-dark: #8f1d16;
      --ok: #15803d;
      --warn: #a16207;
      font-family: Arial, Helvetica, sans-serif;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      background: linear-gradient(180deg, #ffffff 0, var(--bg) 260px);
      color: var(--ink);
    }

    main {
      width: min(760px, 100%);
      margin: 0 auto;
      padding: 22px 16px 38px;
    }

    header {
      margin-bottom: 16px;
    }

    h1 {
      margin: 0;
      font-size: clamp(28px, 8vw, 44px);
      line-height: 1;
    }

    p {
      color: var(--muted);
      line-height: 1.45;
      margin: 8px 0 0;
    }

    .grid {
      display: grid;
      gap: 14px;
    }

    .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 16px;
      box-shadow: 0 8px 22px rgba(16, 24, 40, 0.07);
    }

    @keyframes alarmPulse {
      0%, 100% { border-color: var(--line); }
      50% { border-color: var(--ok); box-shadow: 0 0 0 3px rgba(21, 128, 61, 0.18); }
    }

    .alarm { animation: alarmPulse 0.6s linear infinite; }

    .status {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }

    .metric {
      border: 1px solid var(--line);
      border-radius: 8px;
      min-height: 78px;
      padding: 12px;
    }

    .label {
      color: var(--muted);
      display: block;
      font-size: 13px;
      margin-bottom: 8px;
    }

    .value {
      display: block;
      font-size: 22px;
      font-weight: 700;
      overflow-wrap: anywhere;
    }

    .row {
      display: flex;
      align-items: center;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 10px;
    }

    .timer-fields {
      display: grid;
      grid-template-columns: minmax(94px, 112px) minmax(94px, 112px) auto auto;
      gap: 10px;
      align-items: end;
      margin-top: 10px;
    }

    .field-label {
      color: var(--muted);
      display: block;
      font-size: 12px;
      font-weight: 700;
      margin-bottom: 5px;
    }

    button {
      appearance: none;
      border: 0;
      border-radius: 8px;
      background: var(--primary);
      color: #fff;
      cursor: pointer;
      font-weight: 700;
      min-height: 44px;
      padding: 0 14px;
    }

    button:hover { background: var(--primary-dark); }
    button.danger { background: var(--danger); }
    button.danger:hover { background: var(--danger-dark); }
    button.secondary { background: #344054; }

    input[type="number"] {
      width: 110px;
      min-height: 44px;
      border: 1px solid var(--line);
      border-radius: 8px;
      font: inherit;
      padding: 0 10px;
    }

    input[type="range"] {
      width: 100%;
      accent-color: var(--primary);
    }

    .slider-line {
      display: grid;
      grid-template-columns: 1fr 74px;
      align-items: center;
      gap: 12px;
      margin-top: 10px;
    }

    .track-state {
      color: var(--muted);
      font-size: 13px;
      margin-top: 8px;
    }

    .warn {
      color: var(--warn);
      font-weight: 700;
    }

    .mono {
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      font-variant-numeric: tabular-nums;
    }

    @media (max-width: 620px) {
      .status { grid-template-columns: 1fr; }
      .timer-fields { grid-template-columns: 1fr 1fr; }
      button { flex: 1 1 138px; }
    }

    .timer-inline-status {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-top: 12px;
      padding-top: 12px;
      border-top: 1px solid var(--line);
    }

    .timer-inline-status .value {
      font-size: 26px;
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>Fog&atilde;o - 1 man&iacute;pulo</h1>
      <p>Conecte-se ao Wi-Fi <strong>FOGAO-ESP32</strong> e acesse <strong>192.168.4.1</strong>.</p>
    </header>

    <section class="grid">
      <div class="panel status" id="statusPanel" aria-live="polite">
        <div class="metric">
          <span class="label">Estado assumido</span>
          <span class="value" id="nivel">--</span>
        </div>
        <div class="metric">
          <span class="label">Tempo restante</span>
          <span class="value mono" id="tempo">--:--</span>
        </div>
        <div class="metric">
          <span class="label">Servo</span>
          <span class="value" id="servo">--</span>
        </div>
      </div>

      <div class="panel">
        <span class="label">Controle & Sincroniza&ccedil;&atilde;o Manual</span>
        <p>Use quando girar o man&iacute;pulo na m&atilde;o. Clique em <strong>Marcar</strong> para dizer ao ESP32 onde o man&iacute;pulo est&aacute; sem mover o motor:</p>
        <div class="row">
          <span class="field-label" style="width:100%;margin-bottom:0;">Sincronizar posi&ccedil;&atilde;o atual (sem mover motor):</span>
          <button type="button" class="secondary" onclick="marcar('fechado')">&#x2714; Marcar Fechado</button>
          <button type="button" class="secondary" onclick="marcar('alto')">Marcar Alto</button>
          <button type="button" class="secondary" onclick="marcar('medio')">Marcar M&eacute;dio</button>
          <button type="button" class="secondary" onclick="marcar('baixo')">Marcar Baixo</button>
        </div>
        <div class="row" style="margin-top:14px;padding-top:10px;border-top:1px solid var(--line);">
          <span class="field-label" style="width:100%;margin-bottom:0;">Girar man&iacute;pulo com o servo:</span>
          <button type="button" onclick="irPara('alto')">Ir para Alto</button>
          <button type="button" onclick="irPara('medio')">Ir para M&eacute;dio</button>
          <button type="button" onclick="irPara('baixo')">Ir para Baixo</button>
          <button type="button" class="danger" onclick="fechar()">Fechar agora</button>
        </div>
      </div>

      <div class="panel">
        <span class="label">Timer de cozimento</span>
        <div class="timer-fields">
          <label>
            <span class="field-label">Minutos</span>
            <input id="timerMin" type="number" min="0" max="720" value="0" aria-label="Minutos do timer">
          </label>
          <label>
            <span class="field-label">Segundos</span>
            <input id="timerSec" type="number" min="0" max="59" value="5" aria-label="Segundos do timer">
          </label>
          <button type="button" onclick="iniciarTimer()">Iniciar timer</button>
          <button type="button" onclick="cancelarTimer()" class="secondary">Cancelar timer</button>
        </div>
        <div class="timer-inline-status">
          <span class="label" style="margin:0">Tempo restante</span>
          <span class="value mono" id="tempoInline">--:--</span>
        </div>
      </div>

      <div class="panel">
        <span class="label">Calibra&ccedil;&atilde;o e teste do servo</span>
        <p>Ajuste o slider com o fog&atilde;o desligado. Quando o &acirc;ngulo bater com a posi&ccedil;&atilde;o desejada, salve esse valor no bot&atilde;o correspondente.</p>
        <div class="slider-line">
          <input id="servoSlider" type="range" min="0" max="180" value="0" oninput="sliderInput(this.value)" aria-label="&Acirc;ngulo do servo">
          <strong><span id="posLabel">0</span>&deg;</strong>
        </div>
        <div class="track-state" id="trackState">Aguardando comando.</div>
        <div class="row">
          <button type="button" class="secondary" onclick="salvarCalibracao('fechado')">Salvar fechado</button>
          <button type="button" class="secondary" onclick="salvarCalibracao('alto')">Salvar alto</button>
          <button type="button" class="secondary" onclick="salvarCalibracao('medio')">Salvar m&eacute;dio</button>
          <button type="button" class="secondary" onclick="salvarCalibracao('baixo')">Salvar baixo</button>
        </div>
        <p class="mono" id="calValues">Fechado: -- | Alto: -- | M&eacute;dio: -- | Baixo: --</p>
        <p class="warn">N&atilde;o force o man&iacute;pulo contra o servo energizado. Use "Liberar servo" antes de mover na m&atilde;o.</p>
      </div>
    </section>
  </main>

  <script>
    const nivelEl = document.getElementById("nivel");
    const tempoEl = document.getElementById("tempo");
    const tempoInlineEl = document.getElementById("tempoInline");
    const servoEl = document.getElementById("servo");
    const slider = document.getElementById("servoSlider");
    const posLabel = document.getElementById("posLabel");
    const statusPanel = document.getElementById("statusPanel");
    const trackState = document.getElementById("trackState");
    const calValues = document.getElementById("calValues");
    let lastDoneSeq = 0;
    let started = false;
    let audioCtx = null;
    let sliderFrame = 0;
    let visualAnimatingUntil = 0;

    async function call(url, options = {}) {
      const response = await fetch(url, options);
      if (!response.ok) throw new Error(await response.text());
      await updateStatus();
    }

    function formatSeconds(seconds) {
      const min = Math.floor(seconds / 60);
      const sec = seconds % 60;
      return `${String(min).padStart(2, "0")}:${String(sec).padStart(2, "0")}`;
    }

    function formatLevel(level) {
      const names = {
        "Fogo medio": "Fogo m\u00e9dio",
        "Desconhecido": "Desconhecido",
        "Fechado": "Fechado",
        "Fogo alto": "Fogo alto",
        "Fogo baixo": "Fogo baixo",
        "Manual": "Manual"
      };
      return names[level] || level;
    }

    function setSliderVisual(value) {
      const rounded = Math.round(Number(value));
      slider.value = rounded;
      posLabel.textContent = rounded;
    }

    function animateSliderTo(target, duration = 2500) {
      cancelAnimationFrame(sliderFrame);
      const from = Number(slider.value);
      const to = Number(target);
      const start = performance.now();
      visualAnimatingUntil = start + duration + 80;
      trackState.textContent = "Fechando automaticamente...";

      function step(now) {
        const progress = Math.min(1, (now - start) / duration);
        const eased = 1 - Math.pow(1 - progress, 3);
        setSliderVisual(from + (to - from) * eased);

        if (progress < 1) {
          sliderFrame = requestAnimationFrame(step);
        } else {
          trackState.textContent = "Movimento conclu\u00eddo.";
        }
      }

      sliderFrame = requestAnimationFrame(step);
    }

    function ensureAudio() {
      if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      if (audioCtx.state === "suspended") audioCtx.resume();
    }

    function alarm() {
      try {
        ensureAudio();
        for (let i = 0; i < 3; i++) {
          const osc = audioCtx.createOscillator();
          const gain = audioCtx.createGain();
          const start = audioCtx.currentTime + i * 0.22;
          osc.frequency.value = 880;
          gain.gain.setValueAtTime(0.0001, start);
          gain.gain.exponentialRampToValueAtTime(0.18, start + 0.01);
          gain.gain.exponentialRampToValueAtTime(0.0001, start + 0.14);
          osc.connect(gain);
          gain.connect(audioCtx.destination);
          osc.start(start);
          osc.stop(start + 0.18);
        }
      } catch (e) {}
      statusPanel.classList.add("alarm");
      setTimeout(() => statusPanel.classList.remove("alarm"), 6000);
    }

    document.addEventListener("pointerdown", () => {
      try { ensureAudio(); } catch (e) {}
    }, { passive: true });

    function irPara(nivel) {
      call(`/goto?level=${encodeURIComponent(nivel)}`, { method: "POST" }).catch(alert);
    }

    function marcar(nivel) {
      call(`/mark?level=${encodeURIComponent(nivel)}`, { method: "POST" }).catch(alert);
    }

    function salvarCalibracao(nivel) {
      const pos = slider.value;
      call(`/calibrate?level=${encodeURIComponent(nivel)}&pos=${encodeURIComponent(pos)}`, { method: "POST" }).catch(alert);
    }

    function liberarServo() {
      call("/release", { method: "POST" }).catch(alert);
    }

    function fechar() {
      const fromPos = slider.value;
      animateSliderTo(0, 2500);
      call(`/fechar?from=${encodeURIComponent(fromPos)}`).catch(alert);
    }

    function iniciarTimer() {
      const min = document.getElementById("timerMin").value;
      const sec = document.getElementById("timerSec").value;
      call(`/timer?min=${encodeURIComponent(min)}&sec=${encodeURIComponent(sec)}`, { method: "POST" }).catch(alert);
    }

    function cancelarTimer() {
      call("/cancelTimer", { method: "POST" }).catch(alert);
    }

    let sliderThrottleTimer = null;
    let sliderLastSent = 0;
    let sliderPendingValue = null;

    function sliderInput(value) {
      // Atualiza o numero visivel imediatamente
      posLabel.textContent = value;
      sliderPendingValue = value;

      const now = Date.now();
      const elapsed = now - sliderLastSent;

      // Envia imediatamente se ja passou tempo suficiente
      if (elapsed >= 80) {
        clearTimeout(sliderThrottleTimer);
        sliderThrottleTimer = null;
        sliderLastSent = now;
        call(`/setServo?pos=${encodeURIComponent(value)}`).catch(() => {});
      } else if (!sliderThrottleTimer) {
        // Agenda envio para quando o throttle expirar
        sliderThrottleTimer = setTimeout(() => {
          sliderThrottleTimer = null;
          sliderLastSent = Date.now();
          call(`/setServo?pos=${encodeURIComponent(sliderPendingValue)}`).catch(() => {});
        }, 80 - elapsed);
      }
    }

    async function updateStatus() {
      const response = await fetch("/status", { cache: "no-store" });
      const data = await response.json();

      nivelEl.textContent = formatLevel(data.nivel);
      tempoEl.textContent = formatSeconds(data.tempoRestanteSeg);
      tempoInlineEl.textContent = formatSeconds(data.tempoRestanteSeg);
      servoEl.textContent = data.servoAnexado ? `${data.posicao}\u00b0` : "liberado";

      if (data.movendo && data.alvo === 0 && performance.now() > visualAnimatingUntil) {
        animateSliderTo(0);
      } else if (performance.now() > visualAnimatingUntil) {
        setSliderVisual(data.posicao);
        trackState.textContent = data.movendo ? "Movendo servo..." : "Aguardando comando.";
      }

      if (data.calibracao) {
        calValues.textContent = `Fechado: ${data.calibracao.fechado}\u00b0 | Alto: ${data.calibracao.alto}\u00b0 | M\u00e9dio: ${data.calibracao.medio}\u00b0 | Baixo: ${data.calibracao.baixo}\u00b0`;
      }

      if (!started) {
        lastDoneSeq = data.timerDoneSeq;
        started = true;
      } else if (data.timerDoneSeq !== lastDoneSeq) {
        lastDoneSeq = data.timerDoneSeq;
        animateSliderTo(data.alvo);
        alarm();
      }
    }

    updateStatus();
    setInterval(updateStatus, 500);
  </script>
</body>
</html>
)HTML";

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int logicalToServoDeg(int logicalDeg) {
  logicalDeg = clampInt(logicalDeg, 0, 180);
  return INVERT_SERVO_DIRECTION ? (180 - logicalDeg) : logicalDeg;
}

const char *levelName(FlameLevel value) {
  switch (value) {
    case NIVEL_FECHADO:
      return "Fechado";
    case NIVEL_ALTO:
      return "Fogo alto";
    case NIVEL_MEDIO:
      return "Fogo medio";
    case NIVEL_BAIXO:
      return "Fogo baixo";
    case NIVEL_MANUAL:
      return "Manual";
    case NIVEL_DESCONHECIDO:
    default:
      return "Desconhecido";
  }
}

void attachServoIfNeeded() {
  if (servoAttached) {
    return;
  }

  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2500);
  servoAttached = true;
}

void releaseServo() {
  if (!servoAttached) {
    return;
  }

  servoMotor.detach();
  servoAttached = false;
  moving = false;
  rampActive = false;
}

void writeServoLogical(int logicalDeg) {
  attachServoIfNeeded();
  currentDeg = clampInt(logicalDeg, 0, 180);
  servoMotor.write(logicalToServoDeg(currentDeg));
}

void startMove(int deg, uint32_t moveMs) {
  targetDeg = clampInt(deg, 0, 180);
  rampFromDeg = currentDeg;
  rampToDeg = targetDeg;
  rampStartMs = millis();
  rampDurationMs = moveMs < 100UL ? 100UL : moveMs;
  rampActive = true;
  moving = true;
  attachServoIfNeeded();
}

uint32_t remainingMs() {
  if (!timerRunning) {
    return 0;
  }

  const uint32_t elapsed = millis() - timerStartMs;
  if (elapsed >= timerDurationMs) {
    return 0;
  }

  return timerDurationMs - elapsed;
}

bool readIntArg(const String &name, int &value) {
  if (!server.hasArg(name)) {
    return false;
  }

  value = server.arg(name).toInt();
  return true;
}

void loadCalibration() {
  preferences.begin("fogao", false);
  calFechado = preferences.getInt("fechado", POS_FECHADO);
  calAlto = preferences.getInt("alto", POS_FOGO_ALTO);
  calMedio = preferences.getInt("medio", POS_FOGO_MEDIO);
  calBaixo = preferences.getInt("baixo", POS_FOGO_BAIXO);
}

void saveCalibration() {
  preferences.putInt("fechado", calFechado);
  preferences.putInt("alto", calAlto);
  preferences.putInt("medio", calMedio);
  preferences.putInt("baixo", calBaixo);
}

void sendStatus() {
  String json;
  json.reserve(260);
  json += "{\"nivel\":\"";
  json += levelName(level);
  json += "\",\"timerAtivo\":";
  json += (timerRunning ? "true" : "false");
  json += ",\"tempoRestanteSeg\":";
  json += String((remainingMs() + 999UL) / 1000UL);
  json += ",\"posicao\":";
  json += String(currentDeg);
  json += ",\"alvo\":";
  json += String(targetDeg);
  json += ",\"movendo\":";
  json += (moving ? "true" : "false");
  json += ",\"servoAnexado\":";
  json += (servoAttached ? "true" : "false");
  json += ",\"timerDoneSeq\":";
  json += String(timerDoneSeq);
  json += ",\"calibracao\":{";
  json += "\"fechado\":";
  json += String(calFechado);
  json += ",\"alto\":";
  json += String(calAlto);
  json += ",\"medio\":";
  json += String(calMedio);
  json += ",\"baixo\":";
  json += String(calBaixo);
  json += "}";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleClose() {
  timerRunning = false;
  level = NIVEL_FECHADO;
  
  if (server.hasArg("from")) {
    int fromDeg = currentDeg;
    readIntArg("from", fromDeg);
    fromDeg = clampInt(fromDeg, 0, 180);
    currentDeg = fromDeg;
  }

  // Se já estiver na posição de fechado (ou muito perto, diferença < 3 graus),
  // não executa movimento algum para evitar dar meia-volta ou forçar o batente mecânico
  if (abs(currentDeg - calFechado) <= 3) {
    currentDeg = calFechado;
    targetDeg = calFechado;
    moving = false;
    rampActive = false;
    releaseServo();
    sendStatus();
    return;
  }

  startMove(calFechado, DEFAULT_MOVE_MS);
  sendStatus();
}

void handleRelease() {
  timerRunning = false;
  releaseServo();
  sendStatus();
}

void handleCancelTimer() {
  timerRunning = false;
  sendStatus();
}

void handleMark() {
  if (!server.hasArg("level")) {
    server.send(400, "application/json", "{\"erro\":\"Use level=alto, medio ou baixo\"}");
    return;
  }

  String requestedLevel = server.arg("level");
  requestedLevel.toLowerCase();

  if (requestedLevel == "fechado") {
    currentDeg = calFechado;
    targetDeg = calFechado;
    level = NIVEL_FECHADO;
  } else if (requestedLevel == "alto") {
    currentDeg = calAlto;
    targetDeg = calAlto;
    level = NIVEL_ALTO;
  } else if (requestedLevel == "medio") {
    currentDeg = calMedio;
    targetDeg = calMedio;
    level = NIVEL_MEDIO;
  } else if (requestedLevel == "baixo") {
    currentDeg = calBaixo;
    targetDeg = calBaixo;
    level = NIVEL_BAIXO;
  } else {
    server.send(400, "application/json", "{\"erro\":\"Nivel invalido\"}");
    return;
  }

  moving = false;
  rampActive = false;
  sendStatus();
}

void handleGotoLevel() {
  if (!server.hasArg("level")) {
    server.send(400, "application/json", "{\"erro\":\"Use level=fechado, alto, medio ou baixo\"}");
    return;
  }

  String requestedLevel = server.arg("level");
  requestedLevel.toLowerCase();

  if (requestedLevel == "fechado") {
    timerRunning = false;
    level = NIVEL_FECHADO;
    startMove(calFechado, DEFAULT_MOVE_MS);
  } else if (requestedLevel == "alto") {
    timerRunning = false;
    level = NIVEL_ALTO;
    startMove(calAlto, DEFAULT_MOVE_MS);
  } else if (requestedLevel == "medio") {
    timerRunning = false;
    level = NIVEL_MEDIO;
    startMove(calMedio, DEFAULT_MOVE_MS);
  } else if (requestedLevel == "baixo") {
    timerRunning = false;
    level = NIVEL_BAIXO;
    startMove(calBaixo, DEFAULT_MOVE_MS);
  } else {
    server.send(400, "application/json", "{\"erro\":\"Nivel invalido\"}");
    return;
  }

  sendStatus();
}

void handleCalibrate() {
  if (!server.hasArg("level")) {
    server.send(400, "application/json", "{\"erro\":\"Use level e pos\"}");
    return;
  }

  int pos = currentDeg;
  if (server.hasArg("pos")) {
    readIntArg("pos", pos);
  }
  pos = clampInt(pos, 0, 180);

  String requestedLevel = server.arg("level");
  requestedLevel.toLowerCase();

  if (requestedLevel == "fechado") {
    calFechado = pos;
    level = NIVEL_FECHADO;
  } else if (requestedLevel == "alto") {
    calAlto = pos;
    level = NIVEL_ALTO;
  } else if (requestedLevel == "medio") {
    calMedio = pos;
    level = NIVEL_MEDIO;
  } else if (requestedLevel == "baixo") {
    calBaixo = pos;
    level = NIVEL_BAIXO;
  } else {
    server.send(400, "application/json", "{\"erro\":\"Nivel invalido\"}");
    return;
  }

  currentDeg = pos;
  targetDeg = pos;
  moving = false;
  rampActive = false;
  saveCalibration();
  sendStatus();
}

void handleTimer() {
  int minutes = 0;
  int seconds = 0;

  if (server.hasArg("min")) {
    readIntArg("min", minutes);
  }
  if (server.hasArg("sec")) {
    readIntArg("sec", seconds);
  }

  minutes = clampInt(minutes, 0, 720);
  seconds = clampInt(seconds, 0, 59);
  const uint32_t totalSeconds = (uint32_t)minutes * 60UL + (uint32_t)seconds;

  if (totalSeconds < 1UL) {
    server.send(400, "application/json", "{\"erro\":\"Use pelo menos 1 segundo\"}");
    return;
  }

  timerDurationMs = totalSeconds * 1000UL;
  timerStartMs = millis();
  timerRunning = true;
  sendStatus();
}

void handleSetServo() {
  int pos = 0;
  if (!readIntArg("pos", pos)) {
    server.send(400, "application/json", "{\"erro\":\"Parametro pos obrigatorio\"}");
    return;
  }

  timerRunning = false;
  targetDeg = clampInt(pos, 0, 180);
  level = targetDeg == calFechado ? NIVEL_FECHADO : NIVEL_MANUAL;
  // Rampa curta (300ms) para o slider: servo chega suavemente e libera sozinho
  startMove(targetDeg, 300UL);
  sendStatus();
}

void handleOpenAlias() {
  level = NIVEL_ALTO;
  currentDeg = calAlto;
  targetDeg = calAlto;
  sendStatus();
}

void handleNotFound() {
  server.send(404, "application/json", "{\"erro\":\"Rota nao encontrada\"}");
}

void configureRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, sendStatus);
  server.on("/fechar", HTTP_GET, handleClose);
  server.on("/abrir", HTTP_GET, handleOpenAlias);
  server.on("/setServo", HTTP_GET, handleSetServo);
  server.on("/timer", HTTP_POST, handleTimer);
  server.on("/setTimer", HTTP_POST, handleTimer);
  server.on("/mark", HTTP_POST, handleMark);
  server.on("/goto", HTTP_POST, handleGotoLevel);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/release", HTTP_POST, handleRelease);
  server.on("/cancelTimer", HTTP_POST, handleCancelTimer);
  server.onNotFound(handleNotFound);
}

void updateMotionAndTimer() {
  const uint32_t now = millis();

  if (rampActive) {
    const uint32_t elapsed = now - rampStartMs;
    if (elapsed >= rampDurationMs) {
      writeServoLogical(rampToDeg);
      rampActive = false;
      moving = false;
      releaseServo(); // Libera automaticamente ao fim de qualquer movimento
    } else {
      const int delta = rampToDeg - rampFromDeg;
      const int deg = rampFromDeg + (int)((int32_t)delta * (int32_t)elapsed / (int32_t)rampDurationMs);
      if (deg != currentDeg) {
        writeServoLogical(deg);
      }
      moving = true;
    }
  }

  if (timerRunning && remainingMs() == 0) {
    timerRunning = false;
    timerDoneSeq++;
    level = NIVEL_FECHADO;
    startMove(calFechado, DEFAULT_MOVE_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  loadCalibration();

  attachServoIfNeeded();
  writeServoLogical(calFechado);
  releaseServo();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  configureRoutes();
  server.begin();

  Serial.println();
  Serial.println("Servidor iniciado.");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  updateMotionAndTimer();

  if (millis() - lastSerialStatusMs > 30000) {
    lastSerialStatusMs = millis();
    Serial.print("Nivel: ");
    Serial.print(levelName(level));
    Serial.print(" | Posicao assumida: ");
    Serial.print(currentDeg);
    Serial.print(" | Restante: ");
    Serial.println((remainingMs() + 999UL) / 1000UL);
  }
}
