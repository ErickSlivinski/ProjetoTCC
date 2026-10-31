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
<html lang="pt-BR" data-theme="light">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Fogão - 1 manípulo</title>
  <style>
    /* ══════════════════════════════════════════
       TOKENS
       (sem fontes externas: o Wi-Fi do ESP32 nao tem internet)
    ══════════════════════════════════════════ */
    :root {
      --color-bone-parchment: #f8f8f6;
      --color-paper-white:    #ffffff;
      --color-soft-stone:     #efeeeb;
      --color-carbon-ink:     #121212;
      --color-graphite:       #373734;
      --color-ashen:          #7b7974;
      --color-pebble:         #9c9a92;
      --color-mist:           #b7b7b5;
      --color-chalk:          #e7e6e1;
      --color-obsidian:       #000000;
      --color-clay:           #d97757;

      --bg:          var(--color-bone-parchment);
      --bg-card:     var(--color-paper-white);
      --bg-nested:   var(--color-soft-stone);
      --bg-topbar:   rgba(248,248,246,0.88);
      --text:        var(--color-carbon-ink);
      --text-sec:    var(--color-graphite);
      --text-muted:  var(--color-ashen);
      --border:      var(--color-chalk);
      --border-fine: var(--color-mist);
      --btn-bg:      var(--color-carbon-ink);
      --btn-text:    var(--color-bone-parchment);
      --btn-sec-bg:  var(--color-soft-stone);
      --btn-sec-text:var(--color-graphite);
      --input-bg:    var(--color-paper-white);
      --shadow:      rgba(0,0,0,0.04) 0px 4px 20px 0px;

      --font-serif: ui-serif, Georgia, "Times New Roman", serif;
      --font-sans:  ui-sans-serif, system-ui, -apple-system, "Segoe UI", Roboto, Arial, sans-serif;

      --r-sm:   8px;
      --r-md:   16px;
      --r-lg:   24px;

      --ease: 0.2s ease;
    }

    [data-theme="dark"] {
      --bg:          #1a1916;
      --bg-card:     #232320;
      --bg-nested:   #2a2925;
      --bg-topbar:   rgba(26,25,22,0.9);
      --text:        #f0ede8;
      --text-sec:    #c8c4bc;
      --text-muted:  #8a8680;
      --border:      #38362f;
      --border-fine: #2e2d28;
      --btn-bg:      #f0ede8;
      --btn-text:    #121212;
      --btn-sec-bg:  #2e2d28;
      --btn-sec-text:#c8c4bc;
      --input-bg:    #2a2925;
      --shadow:      rgba(0,0,0,0.25) 0px 4px 20px 0px;
    }

    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    html { font-size: 16px; }

    body {
      font-family: var(--font-sans);
      font-size: 14px;
      line-height: 1.5;
      color: var(--text);
      background: var(--bg);
      min-height: 100vh;
      transition: background var(--ease), color var(--ease);
    }

    #topbar {
      position: sticky;
      top: 0;
      z-index: 100;
      height: 52px;
      background: var(--bg-topbar);
      border-bottom: 1px solid var(--border);
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0 24px;
      gap: 12px;
      transition: background var(--ease), border-color var(--ease);
    }

    .topbar-brand {
      display: flex;
      align-items: center;
      gap: 8px;
      font-family: var(--font-serif);
      font-size: 16px;
      font-weight: 400;
      color: var(--text);
      letter-spacing: -0.01em;
    }

    .clay-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--color-clay);
      flex-shrink: 0;
    }

    .topbar-controls {
      display: flex;
      align-items: center;
      gap: 4px;
    }

    .lang-btn {
      font-family: var(--font-sans);
      font-size: 12px;
      font-weight: 500;
      appearance: none;
      border: 1px solid transparent;
      border-radius: var(--r-sm);
      background: transparent;
      color: var(--text-muted);
      cursor: pointer;
      padding: 4px 10px;
      transition: color var(--ease), background var(--ease), border-color var(--ease);
      min-height: unset;
    }

    .lang-btn:hover { color: var(--text-sec); background: var(--bg-nested); }

    .lang-btn.active {
      color: var(--text);
      background: var(--bg-nested);
      border-color: var(--border);
    }

    #themeBtn {
      font-size: 15px;
      appearance: none;
      border: 1px solid var(--border);
      border-radius: var(--r-sm);
      background: transparent;
      cursor: pointer;
      padding: 4px 8px;
      line-height: 1.4;
      min-height: unset;
      margin-left: 6px;
      color: var(--text);
      transition: background var(--ease), border-color var(--ease);
    }

    #themeBtn:hover { background: var(--bg-nested); }

    main {
      width: min(760px, 100%);
      margin: 0 auto;
      padding: 40px 20px 64px;
    }

    header { margin-bottom: 32px; }

    h1 {
      font-family: var(--font-serif);
      font-size: clamp(26px, 7vw, 38px);
      font-weight: 400;
      line-height: 1.2;
      color: var(--text);
      letter-spacing: -0.02em;
    }

    h1::before {
      content: '';
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: var(--color-clay);
      margin-right: 10px;
      vertical-align: middle;
      position: relative;
      top: -2px;
    }

    .subtitle {
      font-size: 14px;
      color: var(--text-muted);
      margin-top: 8px;
      line-height: 1.5;
    }

    .subtitle strong { color: var(--text-sec); font-weight: 600; }

    .grid { display: grid; gap: 16px; }

    .panel {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: var(--r-md);
      padding: 24px;
      box-shadow: var(--shadow);
      transition: background var(--ease), border-color var(--ease), box-shadow var(--ease);
    }

    @keyframes alarmPulse {
      0%, 100% { border-color: var(--border); }
      50% { border-color: var(--color-clay); box-shadow: 0 0 0 3px rgba(217,119,87,0.18); }
    }

    .alarm { animation: alarmPulse 0.6s linear infinite; }

    .section-label {
      display: block;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      color: var(--text-muted);
      margin-bottom: 12px;
    }

    .section-title {
      font-family: var(--font-serif);
      font-size: 20px;
      font-weight: 400;
      color: var(--text);
      line-height: 1.3;
      margin-bottom: 8px;
    }

    .status {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }

    .metric {
      background: var(--bg-nested);
      border-radius: var(--r-sm);
      min-height: 80px;
      padding: 14px;
      transition: background var(--ease);
    }

    .label {
      display: block;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      color: var(--text-muted);
      margin-bottom: 8px;
    }

    .value {
      display: block;
      font-size: 22px;
      font-weight: 600;
      color: var(--text);
      overflow-wrap: anywhere;
    }

    .divider {
      border: none;
      border-top: 1px solid var(--border);
      margin: 16px 0;
    }

    .field-label {
      display: block;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      color: var(--text-muted);
      margin-bottom: 6px;
    }

    .row {
      display: flex;
      align-items: center;
      flex-wrap: wrap;
      gap: 8px;
      margin-top: 10px;
    }

    .row-label {
      width: 100%;
      margin-bottom: 0;
    }

    button {
      appearance: none;
      border: none;
      border-radius: var(--r-sm);
      background: var(--btn-bg);
      color: var(--btn-text);
      cursor: pointer;
      font-family: var(--font-sans);
      font-size: 13px;
      font-weight: 500;
      min-height: 36px;
      padding: 0 16px;
      transition: opacity var(--ease), background var(--ease), color var(--ease);
      white-space: nowrap;
    }

    button:hover { opacity: 0.82; }

    button.secondary {
      background: var(--btn-sec-bg);
      color: var(--btn-sec-text);
      border: 1px solid var(--border);
    }

    button.secondary:hover { opacity: 1; background: var(--border); }

    button.danger {
      background: transparent;
      color: #c0392b;
      border: 1px solid #e5a09a;
    }

    [data-theme="dark"] button.danger { color: #f87171; border-color: #7f4040; }
    button.danger:hover { background: rgba(192,57,43,0.08); opacity: 1; }

    input[type="number"] {
      width: 100px;
      min-height: 36px;
      border: 1px solid var(--border);
      border-radius: var(--r-sm);
      font-family: var(--font-sans);
      font-size: 14px;
      padding: 0 10px;
      background: var(--input-bg);
      color: var(--text);
      transition: background var(--ease), border-color var(--ease), color var(--ease);
    }

    input[type="number"]:focus {
      outline: none;
      border-color: var(--color-clay);
      box-shadow: 0 0 0 2px rgba(217,119,87,0.15);
    }

    input[type="range"] {
      width: 100%;
      accent-color: var(--color-clay);
    }

    .timer-fields {
      display: grid;
      grid-template-columns: minmax(90px, 108px) minmax(90px, 108px) auto auto;
      gap: 10px;
      align-items: end;
      margin-top: 10px;
    }

    .timer-inline-status {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-top: 14px;
      padding-top: 14px;
      border-top: 1px solid var(--border);
    }

    .timer-inline-status .value { font-size: 28px; }

    .slider-line {
      display: grid;
      grid-template-columns: 1fr 64px;
      align-items: center;
      gap: 12px;
      margin-top: 10px;
    }

    .slider-deg {
      font-family: var(--font-serif);
      font-size: 18px;
      font-weight: 400;
      color: var(--text);
      text-align: right;
    }

    .track-state {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 6px;
    }

    .cal-values {
      font-size: 12px;
      font-family: ui-monospace, 'Cascadia Code', 'Fira Code', monospace;
      font-variant-numeric: tabular-nums;
      color: var(--text-muted);
      margin-top: 10px;
      padding: 10px 12px;
      background: var(--bg-nested);
      border-radius: var(--r-sm);
    }

    .warn-note {
      display: flex;
      align-items: flex-start;
      gap: 8px;
      margin-top: 12px;
      padding: 10px 12px;
      background: rgba(217,119,87,0.08);
      border: 1px solid rgba(217,119,87,0.2);
      border-radius: var(--r-sm);
      font-size: 12px;
      color: var(--text-sec);
      line-height: 1.5;
    }

    .warn-note::before {
      content: '⚠';
      color: var(--color-clay);
      font-size: 13px;
      flex-shrink: 0;
      margin-top: 1px;
    }

    .mono {
      font-family: ui-monospace, 'Cascadia Code', 'Fira Code', monospace;
      font-variant-numeric: tabular-nums;
    }

    .desc {
      font-size: 13px;
      color: var(--text-muted);
      line-height: 1.55;
      margin-bottom: 14px;
    }

    @media (max-width: 620px) {
      .status { grid-template-columns: 1fr; }
      .timer-fields { grid-template-columns: 1fr 1fr; }
      button { flex: 1 1 130px; }
      .lang-btn, #themeBtn { flex: 0 0 auto; }
    }
  </style>
</head>
<body>

  <div id="topbar">
    <div class="topbar-brand">
      <span class="clay-dot"></span>
      <span data-i18n="title">Fogão - 1 manípulo</span>
    </div>
    <div class="topbar-controls">
      <button class="lang-btn" id="btn-pt" onclick="setLang('pt')">PT</button>
      <button class="lang-btn" id="btn-en" onclick="setLang('en')">EN</button>
      <button class="lang-btn" id="btn-es" onclick="setLang('es')">ES</button>
      <button id="themeBtn" onclick="toggleTheme()" aria-label="Alternar tema">🌙</button>
    </div>
  </div>

  <main>
    <header>
      <h1 data-i18n="title">Fogão - 1 manípulo</h1>
      <p class="subtitle" data-i18n="subtitle">
        Conecte-se ao Wi-Fi <strong>FOGAO-ESP32</strong> e acesse <strong>192.168.4.1</strong>.
      </p>
    </header>

    <section class="grid">

      <div class="panel status" id="statusPanel" aria-live="polite">
        <div class="metric">
          <span class="label" data-i18n="estado">Estado assumido</span>
          <span class="value" id="nivel">--</span>
        </div>
        <div class="metric">
          <span class="label" data-i18n="tempo">Tempo restante</span>
          <span class="value mono" id="tempo">--:--</span>
        </div>
        <div class="metric">
          <span class="label" data-i18n="servo">Servo</span>
          <span class="value" id="servo">--</span>
        </div>
      </div>

      <div class="panel">
        <span class="section-label" data-i18n="controle_titulo">Controle &amp; Sincronização Manual</span>
        <p class="desc" data-i18n="controle_desc">
          Use quando girar o manípulo na mão. Clique em <strong>Marcar</strong> para dizer ao ESP32 onde o manípulo está sem mover o motor.
        </p>

        <span class="field-label row-label" data-i18n="sincronizar">Sincronizar posição atual (sem mover motor):</span>
        <div class="row">
          <button class="secondary" onclick="marcar('fechado')" data-i18n="marcar_fechado">✔ Marcar Fechado</button>
          <button class="secondary" onclick="marcar('alto')"   data-i18n="marcar_alto">Marcar Alto</button>
          <button class="secondary" onclick="marcar('medio')"  data-i18n="marcar_medio">Marcar Médio</button>
          <button class="secondary" onclick="marcar('baixo')"  data-i18n="marcar_baixo">Marcar Baixo</button>
        </div>

        <hr class="divider">

        <span class="field-label row-label" data-i18n="girar">Girar manípulo com o servo:</span>
        <div class="row">
          <button onclick="irPara('alto')"  data-i18n="ir_alto">Ir para Alto</button>
          <button onclick="irPara('medio')" data-i18n="ir_medio">Ir para Médio</button>
          <button onclick="irPara('baixo')" data-i18n="ir_baixo">Ir para Baixo</button>
          <button class="danger" onclick="fechar()" data-i18n="fechar_agora">Fechar agora</button>
        </div>
      </div>

      <div class="panel">
        <span class="section-label" data-i18n="timer_titulo">Timer de cozimento</span>
        <div class="timer-fields">
          <label>
            <span class="field-label" data-i18n="minutos">Minutos</span>
            <input id="timerMin" type="number" min="0" max="720" value="0">
          </label>
          <label>
            <span class="field-label" data-i18n="segundos">Segundos</span>
            <input id="timerSec" type="number" min="0" max="59" value="5">
          </label>
          <button onclick="iniciarTimer()" data-i18n="iniciar_timer">Iniciar timer</button>
          <button class="secondary" onclick="cancelarTimer()" data-i18n="cancelar_timer">Cancelar timer</button>
        </div>
        <div class="timer-inline-status">
          <span class="label" style="margin:0" data-i18n="tempo">Tempo restante</span>
          <span class="value mono" id="tempoInline">--:--</span>
        </div>
      </div>

      <div class="panel">
        <span class="section-label" data-i18n="calib_titulo">Calibração e teste do servo</span>
        <p class="desc" data-i18n="calib_desc">
          Ajuste o slider com o fogão desligado. Quando o ângulo bater com a posição desejada, salve esse valor no botão correspondente.
        </p>
        <div class="slider-line">
          <input id="servoSlider" type="range" min="0" max="180" value="0"
                 oninput="sliderInput(this.value)" aria-label="Ângulo do servo">
          <div class="slider-deg"><span id="posLabel">0</span>°</div>
        </div>
        <div class="track-state" id="trackState" data-i18n="aguardando">Aguardando comando.</div>
        <div class="row" style="margin-top:12px;">
          <button class="secondary" onclick="salvarCalibracao('fechado')" data-i18n="salvar_fechado">Salvar fechado</button>
          <button class="secondary" onclick="salvarCalibracao('alto')"    data-i18n="salvar_alto">Salvar alto</button>
          <button class="secondary" onclick="salvarCalibracao('medio')"   data-i18n="salvar_medio">Salvar médio</button>
          <button class="secondary" onclick="salvarCalibracao('baixo')"   data-i18n="salvar_baixo">Salvar baixo</button>
        </div>
        <div class="cal-values" id="calValues">Fechado: -- | Alto: -- | Médio: -- | Baixo: --</div>
        <div class="warn-note" data-i18n="aviso">
          Não force o manípulo contra o servo energizado. O servo é liberado automaticamente ao fim de cada movimento; só gire na mão com o servo liberado.
        </div>
      </div>

    </section>
  </main>

  <script>
    /* TEMA */
    const htmlEl   = document.documentElement;
    const themeBtn = document.getElementById("themeBtn");

    function store(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
    function load(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }

    function applyTheme(t) {
      htmlEl.setAttribute("data-theme", t);
      themeBtn.textContent = t === "dark" ? "☀️" : "🌙";
      store("theme", t);
    }

    function toggleTheme() {
      applyTheme(htmlEl.getAttribute("data-theme") === "dark" ? "light" : "dark");
    }

    (function initTheme() {
      const saved = load("theme");
      if (saved) { applyTheme(saved); return; }
      applyTheme(matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
    })();

    /* I18N */
    const LANGS = {
      pt: {
        title:           "Fogão — 1 manípulo",
        subtitle:        'Conecte-se ao Wi-Fi <strong>FOGAO-ESP32</strong> e acesse <strong>192.168.4.1</strong>.',
        estado:          "Estado assumido",
        tempo:           "Tempo restante",
        servo:           "Servo",
        controle_titulo: "Controle & Sincronização Manual",
        controle_desc:   'Use quando girar o manípulo na mão. Clique em <strong>Marcar</strong> para dizer ao ESP32 onde o manípulo está sem mover o motor.',
        sincronizar:     "Sincronizar posição atual (sem mover motor):",
        marcar_fechado:  "✔ Marcar Fechado",
        marcar_alto:     "Marcar Alto",
        marcar_medio:    "Marcar Médio",
        marcar_baixo:    "Marcar Baixo",
        girar:           "Girar manípulo com o servo:",
        ir_alto:         "Ir para Alto",
        ir_medio:        "Ir para Médio",
        ir_baixo:        "Ir para Baixo",
        fechar_agora:    "Fechar agora",
        timer_titulo:    "Timer de cozimento",
        minutos:         "Minutos",
        segundos:        "Segundos",
        iniciar_timer:   "Iniciar timer",
        cancelar_timer:  "Cancelar timer",
        calib_titulo:    "Calibração e teste do servo",
        calib_desc:      "Ajuste o slider com o fogão desligado. Quando o ângulo bater com a posição desejada, salve esse valor no botão correspondente.",
        salvar_fechado:  "Salvar fechado",
        salvar_alto:     "Salvar alto",
        salvar_medio:    "Salvar médio",
        salvar_baixo:    "Salvar baixo",
        aguardando:      "Aguardando comando.",
        movendo:         "Movendo servo...",
        fechando:        "Fechando automaticamente...",
        concluido:       "Movimento concluído.",
        aviso:           'Não force o manípulo contra o servo energizado. O servo é liberado automaticamente ao fim de cada movimento; só gire na mão com o servo liberado.',
        liberado:        "liberado",
        lvl_Fechado:     "Fechado",
        lvl_FogoAlto:    "Fogo alto",
        lvl_FogoMedio:   "Fogo médio",
        lvl_FogoBaixo:   "Fogo baixo",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Desconhecido",
        cal: (f,a,m,b) => `Fechado: ${f}° | Alto: ${a}° | Médio: ${m}° | Baixo: ${b}°`,
      },
      en: {
        title:           "Stove — 1 knob",
        subtitle:        'Connect to Wi-Fi <strong>FOGAO-ESP32</strong> and open <strong>192.168.4.1</strong>.',
        estado:          "Assumed state",
        tempo:           "Remaining time",
        servo:           "Servo",
        controle_titulo: "Control & Manual Sync",
        controle_desc:   'Use when turning the knob by hand. Click <strong>Mark</strong> to tell the ESP32 where the knob is without moving the motor.',
        sincronizar:     "Sync current position (without moving motor):",
        marcar_fechado:  "✔ Mark Closed",
        marcar_alto:     "Mark High",
        marcar_medio:    "Mark Medium",
        marcar_baixo:    "Mark Low",
        girar:           "Turn knob with servo:",
        ir_alto:         "Go to High",
        ir_medio:        "Go to Medium",
        ir_baixo:        "Go to Low",
        fechar_agora:    "Close now",
        timer_titulo:    "Cooking timer",
        minutos:         "Minutes",
        segundos:        "Seconds",
        iniciar_timer:   "Start timer",
        cancelar_timer:  "Cancel timer",
        calib_titulo:    "Servo calibration & test",
        calib_desc:      "Adjust the slider with the stove off. When the angle matches the desired position, save that value using the corresponding button.",
        salvar_fechado:  "Save closed",
        salvar_alto:     "Save high",
        salvar_medio:    "Save medium",
        salvar_baixo:    "Save low",
        aguardando:      "Waiting for command.",
        movendo:         "Moving servo...",
        fechando:        "Closing automatically...",
        concluido:       "Movement complete.",
        aviso:           'Do not force the knob against an energized servo. The servo is released automatically after each movement; only turn by hand while it is released.',
        liberado:        "released",
        lvl_Fechado:     "Closed",
        lvl_FogoAlto:    "High flame",
        lvl_FogoMedio:   "Medium flame",
        lvl_FogoBaixo:   "Low flame",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Unknown",
        cal: (f,a,m,b) => `Closed: ${f}° | High: ${a}° | Medium: ${m}° | Low: ${b}°`,
      },
      es: {
        title:           "Cocina — 1 perilla",
        subtitle:        'Conéctese al Wi-Fi <strong>FOGAO-ESP32</strong> y acceda a <strong>192.168.4.1</strong>.',
        estado:          "Estado asumido",
        tempo:           "Tiempo restante",
        servo:           "Servo",
        controle_titulo: "Control y sincronización manual",
        controle_desc:   'Úsalo cuando gires la perilla a mano. Haz clic en <strong>Marcar</strong> para indicar al ESP32 dónde está sin mover el motor.',
        sincronizar:     "Sincronizar posición actual (sin mover motor):",
        marcar_fechado:  "✔ Marcar Cerrado",
        marcar_alto:     "Marcar Alto",
        marcar_medio:    "Marcar Medio",
        marcar_baixo:    "Marcar Bajo",
        girar:           "Girar perilla con el servo:",
        ir_alto:         "Ir a Alto",
        ir_medio:        "Ir a Medio",
        ir_baixo:        "Ir a Bajo",
        fechar_agora:    "Cerrar ahora",
        timer_titulo:    "Temporizador de cocción",
        minutos:         "Minutos",
        segundos:        "Segundos",
        iniciar_timer:   "Iniciar temporizador",
        cancelar_timer:  "Cancelar temporizador",
        calib_titulo:    "Calibración y prueba del servo",
        calib_desc:      "Ajuste el control deslizante con la cocina apagada. Cuando el ángulo coincida, guarde ese valor con el botón correspondiente.",
        salvar_fechado:  "Guardar cerrado",
        salvar_alto:     "Guardar alto",
        salvar_medio:    "Guardar medio",
        salvar_baixo:    "Guardar bajo",
        aguardando:      "Esperando comando.",
        movendo:         "Moviendo servo...",
        fechando:        "Cerrando automáticamente...",
        concluido:       "Movimiento completado.",
        aviso:           'No fuerce la perilla contra el servo energizado. El servo se libera automáticamente al final de cada movimiento; gire a mano solo con el servo liberado.',
        liberado:        "liberado",
        lvl_Fechado:     "Cerrado",
        lvl_FogoAlto:    "Llama alta",
        lvl_FogoMedio:   "Llama media",
        lvl_FogoBaixo:   "Llama baja",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Desconocido",
        cal: (f,a,m,b) => `Cerrado: ${f}° | Alto: ${a}° | Medio: ${m}° | Bajo: ${b}°`,
      }
    };

    let currentLang = "pt";

    function applyI18n() {
      const t = LANGS[currentLang];
      document.querySelectorAll("[data-i18n]").forEach(el => {
        const key = el.getAttribute("data-i18n");
        if (typeof t[key] === "string") el.innerHTML = t[key];
      });
      document.title = t.title;
      ["pt","en","es"].forEach(l => {
        document.getElementById("btn-" + l).classList.toggle("active", l === currentLang);
      });
    }

    function setLang(lang) {
      currentLang = lang;
      store("lang", lang);
      applyI18n();
      updateCalDisplay();
    }

    (function initLang() {
      const saved = load("lang");
      if (saved && LANGS[saved]) currentLang = saved;
      applyI18n();
    })();

    /* LOGICA */
    const nivelEl       = document.getElementById("nivel");
    const tempoEl       = document.getElementById("tempo");
    const tempoInlineEl = document.getElementById("tempoInline");
    const servoEl       = document.getElementById("servo");
    const slider        = document.getElementById("servoSlider");
    const posLabel      = document.getElementById("posLabel");
    const statusPanel   = document.getElementById("statusPanel");
    const trackState    = document.getElementById("trackState");
    const calValuesEl   = document.getElementById("calValues");

    let lastDoneSeq = 0;
    let started = false;
    let audioCtx = null;
    let sliderFrame = 0;
    let visualAnimatingUntil = 0;
    let lastCal = null;

    async function call(url, options = {}) {
      const r = await fetch(url, options);
      if (!r.ok) throw new Error(await r.text());
      await updateStatus();
    }

    function formatSeconds(s) {
      const m = Math.floor(s / 60);
      const sec = s % 60;
      return `${String(m).padStart(2,"0")}:${String(sec).padStart(2,"0")}`;
    }

    function formatLevel(level) {
      const t = LANGS[currentLang];
      const map = {
        "Fogo medio":   t.lvl_FogoMedio,
        "Desconhecido": t.lvl_Desc,
        "Fechado":      t.lvl_Fechado,
        "Fogo alto":    t.lvl_FogoAlto,
        "Fogo baixo":   t.lvl_FogoBaixo,
        "Manual":       t.lvl_Manual
      };
      return map[level] || level;
    }

    function setSliderVisual(value) {
      const v = Math.round(Number(value));
      slider.value = v;
      posLabel.textContent = v;
    }

    function animateSliderTo(target, duration = 2500) {
      cancelAnimationFrame(sliderFrame);
      const from  = Number(slider.value);
      const to    = Number(target);
      const start = performance.now();
      visualAnimatingUntil = start + duration + 80;
      trackState.textContent = LANGS[currentLang].fechando;

      function step(now) {
        const p     = Math.min(1, (now - start) / duration);
        const eased = 1 - Math.pow(1 - p, 3);
        setSliderVisual(from + (to - from) * eased);
        if (p < 1) {
          sliderFrame = requestAnimationFrame(step);
        } else {
          trackState.textContent = LANGS[currentLang].concluido;
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
          const osc  = audioCtx.createOscillator();
          const gain = audioCtx.createGain();
          const s    = audioCtx.currentTime + i * 0.22;
          osc.frequency.value = 880;
          gain.gain.setValueAtTime(0.0001, s);
          gain.gain.exponentialRampToValueAtTime(0.18, s + 0.01);
          gain.gain.exponentialRampToValueAtTime(0.0001, s + 0.14);
          osc.connect(gain);
          gain.connect(audioCtx.destination);
          osc.start(s);
          osc.stop(s + 0.18);
        }
      } catch(e) {}
      statusPanel.classList.add("alarm");
      setTimeout(() => statusPanel.classList.remove("alarm"), 6000);
    }

    document.addEventListener("pointerdown", () => {
      try { ensureAudio(); } catch(e) {}
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
      posLabel.textContent = value;
      sliderPendingValue = value;
      const now = Date.now();
      const elapsed = now - sliderLastSent;
      if (elapsed >= 80) {
        clearTimeout(sliderThrottleTimer);
        sliderThrottleTimer = null;
        sliderLastSent = now;
        call(`/setServo?pos=${encodeURIComponent(value)}`).catch(() => {});
      } else if (!sliderThrottleTimer) {
        sliderThrottleTimer = setTimeout(() => {
          sliderThrottleTimer = null;
          sliderLastSent = Date.now();
          call(`/setServo?pos=${encodeURIComponent(sliderPendingValue)}`).catch(() => {});
        }, 80 - elapsed);
      }
    }

    function updateCalDisplay() {
      if (!lastCal) return;
      const t = LANGS[currentLang];
      calValuesEl.textContent = t.cal(lastCal.fechado, lastCal.alto, lastCal.medio, lastCal.baixo);
    }

    async function updateStatus() {
      const r    = await fetch("/status", { cache: "no-store" });
      const data = await r.json();

      nivelEl.textContent       = formatLevel(data.nivel);
      tempoEl.textContent       = formatSeconds(data.tempoRestanteSeg);
      tempoInlineEl.textContent = formatSeconds(data.tempoRestanteSeg);
      servoEl.textContent       = data.servoAnexado
        ? `${data.posicao}\u00b0`
        : LANGS[currentLang].liberado;

      if (data.movendo && data.alvo === 0 && performance.now() > visualAnimatingUntil) {
        animateSliderTo(0);
      } else if (performance.now() > visualAnimatingUntil) {
        setSliderVisual(data.posicao);
        trackState.textContent = data.movendo
          ? LANGS[currentLang].movendo
          : LANGS[currentLang].aguardando;
      }

      if (data.calibracao) {
        lastCal = data.calibracao;
        updateCalDisplay();
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
    setInterval(() => { updateStatus().catch(() => {}); }, 500);
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
