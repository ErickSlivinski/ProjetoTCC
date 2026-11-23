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
static const int POS_FOGO_BAIXO = 60;
static const uint32_t DEFAULT_MOVE_MS = 2500; // Movimento suave e lento para garantir torque maximo do servo

enum FlameLevel {
  NIVEL_DESCONHECIDO,
  NIVEL_FECHADO,
  NIVEL_ALTO,
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
bool holdActive = false;
uint32_t holdStartMs = 0;
static const uint32_t HOLD_TORQUE_MS = 1200UL; // ms mantendo torque antes de liberar
bool timerRunning = false;
uint32_t timerStartMs = 0;
uint32_t timerDurationMs = 10UL * 60UL * 1000UL;
uint32_t timerDoneSeq = 0;
uint32_t lastSerialStatusMs = 0;
FlameLevel level = NIVEL_FECHADO;
int calFechado = POS_FECHADO;
int calAlto = POS_FOGO_ALTO;
int calBaixo = POS_FOGO_BAIXO;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pt-BR" data-theme="claro">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Fogão — 1 manípulo</title>
  <style>
    /* ══════════════════════════════════════════
       SISTEMA DE TEMAS
       claro · escuro (padrao) · barbie · minecraft
       Sem fontes/imagens externas: o Wi-Fi do ESP32 nao tem internet.
    ══════════════════════════════════════════ */

    /* ---------- TEMA PADRAO: CLARO (visual original) ---------- */
    :root {
      --bg:#f8f8f6; --bg-solid:#ffffff; --bg-card:#ffffff; --bg-nested:#efeeeb;
      --bg-topbar:rgba(248,248,246,0.88);
      --body-bg: var(--bg);
      --text:#121212; --text-sec:#373734; --text-muted:#7b7974;
      --border:#e7e6e1; --border-fine:#b7b7b5;
      --accent:#d97757; --accent-2:#d97757;
      --accent-soft:rgba(217,119,87,0.12); --glow:rgba(217,119,87,0.30);
      --alarm: var(--accent);
      --btn-bg:#121212; --btn-text:#f8f8f6;
      --btn-sec-bg:#efeeeb; --btn-sec-text:#373734; --sec-hover:#e7e6e1;
      --input-bg:#ffffff; --input-text: var(--text);
      --shadow: rgba(0,0,0,0.04) 0 4px 20px 0;
      --danger:#c0392b; --danger-bd:#e5a09a; --danger-hover:rgba(192,57,43,0.08);

      --brand: var(--text);
      --label-color: var(--text-muted);
      --strong: var(--text-sec);
      --timer: var(--text);
      --deg-color: var(--text);
      --slot-value: var(--text); --slot-label: var(--text-muted); --slot-shadow: none;
      --metric-bd: transparent;
      --lang-text: var(--text-muted);
      --lang-active-bg: var(--bg-nested); --lang-active-text: var(--text);
      --lang-active-bd: var(--border); --lang-active-shadow: none;

      --warn-bg:rgba(217,119,87,0.08); --warn-bd:rgba(217,119,87,0.20);
      --warn-color: var(--text-sec); --warn-icon: var(--accent);
      --cal-bg: var(--bg-nested); --cal-color: var(--text-muted); --cal-bd: transparent;

      --font-serif: ui-serif, Georgia, "Times New Roman", serif;
      --font-sans:  ui-sans-serif, system-ui, -apple-system, "Segoe UI", Roboto, Arial, sans-serif;
      --font-mono:  ui-monospace, 'Cascadia Code', 'Fira Code', Consolas, monospace;

      --r-sm:8px; --r-md:16px; --r-btn:8px;
      --bw:1px; --blur:0px;
      --btn-border:none; --btn-sec-border:1px solid var(--border);
      --btn-shadow:none; --btn-shadow-hover:none; --sec-shadow:none; --btn-tshadow:none;
      --shine:0; --lift:0px; --hover-op:0.85;
      --fw-head:400; --fw-btn:500; --fw-strong:600;
      --head-align:left;
      --deco:none;
      --sparkle:none; --sp-size1:260px; --sp-size2:190px; --fx1:none; --fx2:none;
      --topbar-bg: var(--bg-topbar);
      --topbar-bw:1px; --topbar-image: linear-gradient(var(--border),var(--border));

      --track-bg:#dedcd5; --track-h:6px; --track-bw:0px; --track-r:999px; --track-bd:none; --track-shadow:none;
      --thumb-bg: var(--accent); --thumb-bd:2px solid #fff; --thumb-w:22px; --thumb-h:22px; --thumb-r:50%;
      --thumb-shadow:0 1px 5px rgba(0,0,0,0.30);

      --ease:0.2s ease;
    }

    /* ---------- TEMA PADRAO: ESCURO (visual original) ---------- */
    [data-theme="escuro"] {
      --bg:#1a1916; --bg-solid:#232320; --bg-card:#232320; --bg-nested:#2a2925;
      --bg-topbar:rgba(26,25,22,0.90);
      --text:#f0ede8; --text-sec:#c8c4bc; --text-muted:#8a8680;
      --border:#38362f; --border-fine:#4a4842;
      --accent-soft:rgba(217,119,87,0.16); --glow:rgba(217,119,87,0.35);
      --btn-bg:#f0ede8; --btn-text:#121212;
      --btn-sec-bg:#2e2d28; --btn-sec-text:#c8c4bc; --sec-hover:#38362f;
      --input-bg:#2a2925;
      --shadow: rgba(0,0,0,0.25) 0 4px 20px 0;
      --danger:#f87171; --danger-bd:#7f4040; --danger-hover:rgba(248,113,113,0.10);
      --warn-bg:rgba(217,119,87,0.10); --warn-bd:rgba(217,119,87,0.25);
      --track-bg:#3d3b35; --thumb-bd:2px solid #232320;
    }

    /* ---------- TEMA: ROSA GLITTER (Barbie) ---------- */
    [data-theme="barbie"] {
      --bg:#fff0f8; --bg-solid:#fff7fc; --bg-card:rgba(255,255,255,0.78);
      --bg-nested:linear-gradient(135deg,#ffe3f3,#f1e2ff);
      --bg-topbar:rgba(255,240,248,0.82);
      --body-bg:
        radial-gradient(520px 420px at 8% 4%,   rgba(255,122,193,0.38), transparent 70%),
        radial-gradient(560px 460px at 96% 12%, rgba(196,150,255,0.38), transparent 70%),
        radial-gradient(640px 520px at 50% 100%, rgba(255,190,230,0.55), transparent 70%),
        #fff0f8;
      --text:#4b1239; --text-sec:#7b2a66; --text-muted:#a8628f;
      --border:#f6bddf; --border-fine:#e79ccb;
      --accent:#e91e8c; --accent-2:#9b5de5;
      --accent-soft:rgba(233,30,140,0.12); --glow:rgba(255,90,175,0.45);
      --btn-bg:linear-gradient(135deg,#ff74c0 0%,#e91e8c 55%,#a65ee8 100%); --btn-text:#ffffff;
      --btn-sec-bg:rgba(255,255,255,0.85); --btn-sec-text:#b3197a; --sec-hover:rgba(233,30,140,0.12);
      --input-bg:#ffffff;
      --shadow:0 10px 30px rgba(233,30,140,0.16), 0 1px 0 rgba(255,255,255,0.9) inset;
      --danger:#c2185b; --danger-bd:#f08ab8; --danger-hover:rgba(233,30,99,0.10);

      --brand: var(--accent); --label-color: var(--accent); --strong: var(--accent);
      --timer: var(--accent); --deg-color: var(--accent);
      --metric-bd: var(--border);
      --lang-text: var(--text-muted);
      --lang-active-bg: var(--btn-bg); --lang-active-text:#fff; --lang-active-bd:transparent;
      --lang-active-shadow:0 2px 10px var(--glow);
      --warn-bg:rgba(166,94,232,0.10); --warn-bd:rgba(166,94,232,0.30); --warn-icon: var(--accent-2);
      --cal-color: var(--text-sec); --cal-bd: var(--border);

      --font-serif: ui-rounded, "SF Pro Rounded", "Nunito", "Trebuchet MS", ui-sans-serif, system-ui, sans-serif;
      --font-sans:  ui-rounded, "SF Pro Rounded", "Nunito", ui-sans-serif, system-ui, -apple-system, "Segoe UI", Roboto, Arial, sans-serif;

      --r-sm:12px; --r-md:22px; --r-btn:999px;
      --bw:1.5px; --blur:8px;
      --btn-sec-border:1.5px solid var(--border-fine);
      --btn-shadow:0 4px 14px var(--glow); --btn-shadow-hover:0 7px 20px var(--glow);
      --shine:1; --lift:-1px; --hover-op:1;
      --fw-head:800; --fw-btn:700; --fw-strong:800;
      --head-align:center;
      --deco:'✦';
      --sparkle:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='260' height='260'%3E%3Cg fill='%23ffffff'%3E%3Cpath transform='translate(30 40) scale(0.9)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(170 20) scale(0.55)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(110 120) scale(1.1)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(215 170) scale(0.7)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(50 200) scale(0.6)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3C/g%3E%3Cg fill='%23ff5cb8'%3E%3Cpath transform='translate(80 70) scale(0.5)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(190 95) scale(0.8)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(15 140) scale(0.7)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Ccircle cx='140' cy='225' r='2.5'/%3E%3Ccircle cx='235' cy='55' r='2'/%3E%3Ccircle cx='95' cy='20' r='2'/%3E%3C/g%3E%3Cg fill='%23c58cff'%3E%3Cpath transform='translate(140 175) scale(0.55)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Cpath transform='translate(235 130) scale(0.5)' d='M10 0Q11 9 20 10Q11 11 10 20Q9 11 0 10Q9 9 10 0Z'/%3E%3Ccircle cx='60' cy='115' r='2.5'/%3E%3Ccircle cx='170' cy='60' r='2'/%3E%3C/g%3E%3C/svg%3E");
      --fx1:twinkle 3.4s ease-in-out infinite; --fx2:twinkle 2.3s ease-in-out infinite 1.1s;
      --topbar-bw:2px; --topbar-image:linear-gradient(90deg,#ff74c0,#c58cff,#ff74c0);

      --track-bg:linear-gradient(90deg,#ffb3dc,#e91e8c,#a65ee8); --track-h:10px; --track-shadow:0 0 10px var(--glow);
      --thumb-bg:radial-gradient(circle at 35% 30%, #fff 0 18%, #ffd3ec 45%, #ff74c0 100%);
      --thumb-w:26px; --thumb-h:26px; --thumb-shadow:0 2px 10px var(--glow);
    }

    /* ---------- TEMA: MINECRAFT ---------- */
    [data-theme="minecraft"] {
      --dirt: url("data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20width='64'%20height='64'%20shape-rendering='crispEdges'%3E%3Cpath%20fill='%236b4a31'%20d='M0%200h4v4h-4zM28%200h4v4h-4zM32%208h4v4h-4zM4%2012h4v4h-4zM16%2012h4v4h-4zM52%2012h4v4h-4zM20%2016h4v4h-4zM48%2016h4v4h-4zM52%2016h4v4h-4zM56%2016h4v4h-4zM44%2020h4v4h-4zM56%2020h4v4h-4zM20%2028h4v4h-4zM28%2032h4v4h-4zM32%2032h4v4h-4zM48%2036h4v4h-4zM12%2040h4v4h-4zM40%2040h4v4h-4zM44%2040h4v4h-4zM28%2044h4v4h-4zM60%2044h4v4h-4zM32%2048h4v4h-4zM40%2048h4v4h-4zM60%2048h4v4h-4zM32%2052h4v4h-4zM60%2052h4v4h-4zM4%2056h4v4h-4zM8%2056h4v4h-4zM36%2056h4v4h-4zM56%2056h4v4h-4zM24%2060h4v4h-4z'/%3E%3Cpath%20fill='%23866043'%20d='M4%200h4v4h-4zM12%200h4v4h-4zM16%200h4v4h-4zM24%200h4v4h-4zM32%200h4v4h-4zM44%200h4v4h-4zM48%200h4v4h-4zM60%200h4v4h-4zM4%204h4v4h-4zM16%204h4v4h-4zM20%204h4v4h-4zM28%204h4v4h-4zM36%204h4v4h-4zM44%204h4v4h-4zM52%204h4v4h-4zM0%208h4v4h-4zM8%208h4v4h-4zM20%208h4v4h-4zM24%208h4v4h-4zM36%208h4v4h-4zM44%208h4v4h-4zM48%208h4v4h-4zM28%2012h4v4h-4zM36%2012h4v4h-4zM0%2016h4v4h-4zM4%2016h4v4h-4zM16%2016h4v4h-4zM24%2016h4v4h-4zM36%2016h4v4h-4zM40%2016h4v4h-4zM4%2020h4v4h-4zM8%2020h4v4h-4zM20%2020h4v4h-4zM24%2020h4v4h-4zM48%2020h4v4h-4zM60%2020h4v4h-4zM0%2024h4v4h-4zM8%2024h4v4h-4zM20%2024h4v4h-4zM40%2024h4v4h-4zM44%2024h4v4h-4zM0%2028h4v4h-4zM32%2028h4v4h-4zM36%2028h4v4h-4zM40%2028h4v4h-4zM44%2028h4v4h-4zM56%2028h4v4h-4zM0%2032h4v4h-4zM12%2032h4v4h-4zM16%2032h4v4h-4zM36%2032h4v4h-4zM44%2032h4v4h-4zM8%2036h4v4h-4zM20%2036h4v4h-4zM28%2036h4v4h-4zM40%2036h4v4h-4zM44%2036h4v4h-4zM52%2036h4v4h-4zM56%2036h4v4h-4zM60%2036h4v4h-4zM0%2040h4v4h-4zM8%2040h4v4h-4zM16%2040h4v4h-4zM20%2040h4v4h-4zM32%2040h4v4h-4zM52%2040h4v4h-4zM56%2040h4v4h-4zM16%2044h4v4h-4zM20%2044h4v4h-4zM24%2044h4v4h-4zM40%2044h4v4h-4zM48%2044h4v4h-4zM0%2048h4v4h-4zM8%2048h4v4h-4zM20%2048h4v4h-4zM36%2048h4v4h-4zM36%2052h4v4h-4zM40%2052h4v4h-4zM12%2056h4v4h-4zM20%2056h4v4h-4zM48%2056h4v4h-4zM60%2056h4v4h-4zM0%2060h4v4h-4zM16%2060h4v4h-4zM28%2060h4v4h-4zM44%2060h4v4h-4zM48%2060h4v4h-4zM52%2060h4v4h-4zM56%2060h4v4h-4zM60%2060h4v4h-4z'/%3E%3Cpath%20fill='%2394694b'%20d='M8%200h4v4h-4zM52%200h4v4h-4zM56%200h4v4h-4zM12%204h4v4h-4zM32%204h4v4h-4zM60%204h4v4h-4zM0%2012h4v4h-4zM12%2016h4v4h-4zM32%2016h4v4h-4zM40%2020h4v4h-4zM28%2024h4v4h-4zM32%2024h4v4h-4zM52%2024h4v4h-4zM4%2028h4v4h-4zM16%2028h4v4h-4zM24%2028h4v4h-4zM20%2032h4v4h-4zM56%2032h4v4h-4zM60%2032h4v4h-4zM0%2036h4v4h-4zM4%2036h4v4h-4zM16%2036h4v4h-4zM28%2040h4v4h-4zM12%2052h4v4h-4zM4%2060h4v4h-4zM20%2060h4v4h-4zM32%2060h4v4h-4zM40%2060h4v4h-4z'/%3E%3Cpath%20fill='%235e4129'%20d='M20%200h4v4h-4zM36%200h4v4h-4zM8%204h4v4h-4zM48%204h4v4h-4zM4%208h4v4h-4zM16%208h4v4h-4zM40%208h4v4h-4zM60%208h4v4h-4zM44%2012h4v4h-4zM8%2016h4v4h-4zM44%2016h4v4h-4zM56%2024h4v4h-4zM8%2028h4v4h-4zM24%2032h4v4h-4zM40%2032h4v4h-4zM52%2032h4v4h-4zM4%2040h4v4h-4zM44%2044h4v4h-4zM56%2044h4v4h-4zM4%2048h4v4h-4zM12%2048h4v4h-4zM28%2048h4v4h-4zM48%2048h4v4h-4zM52%2048h4v4h-4zM56%2048h4v4h-4zM24%2052h4v4h-4z'/%3E%3Cpath%20fill='%2379553a'%20d='M40%200h4v4h-4zM0%204h4v4h-4zM24%204h4v4h-4zM40%204h4v4h-4zM56%204h4v4h-4zM12%208h4v4h-4zM28%208h4v4h-4zM52%208h4v4h-4zM20%2012h4v4h-4zM24%2012h4v4h-4zM32%2012h4v4h-4zM40%2012h4v4h-4zM60%2012h4v4h-4zM12%2020h4v4h-4zM28%2020h4v4h-4zM36%2020h4v4h-4zM12%2024h4v4h-4zM16%2024h4v4h-4zM24%2024h4v4h-4zM60%2024h4v4h-4zM12%2028h4v4h-4zM28%2028h4v4h-4zM48%2028h4v4h-4zM52%2028h4v4h-4zM4%2032h4v4h-4zM8%2032h4v4h-4zM24%2036h4v4h-4zM32%2036h4v4h-4zM24%2040h4v4h-4zM36%2040h4v4h-4zM12%2044h4v4h-4zM32%2044h4v4h-4zM52%2044h4v4h-4zM16%2048h4v4h-4zM24%2048h4v4h-4zM44%2048h4v4h-4zM0%2052h4v4h-4zM4%2052h4v4h-4zM8%2052h4v4h-4zM16%2052h4v4h-4zM20%2052h4v4h-4zM44%2052h4v4h-4zM52%2052h4v4h-4zM56%2052h4v4h-4zM16%2056h4v4h-4zM24%2056h4v4h-4zM32%2056h4v4h-4zM40%2056h4v4h-4zM8%2060h4v4h-4z'/%3E%3Cpath%20fill='%237d5a3d'%20d='M56%208h4v4h-4zM8%2012h4v4h-4zM12%2012h4v4h-4zM48%2012h4v4h-4zM56%2012h4v4h-4zM28%2016h4v4h-4zM60%2016h4v4h-4zM0%2020h4v4h-4zM16%2020h4v4h-4zM32%2020h4v4h-4zM52%2020h4v4h-4zM4%2024h4v4h-4zM36%2024h4v4h-4zM48%2024h4v4h-4zM60%2028h4v4h-4zM48%2032h4v4h-4zM12%2036h4v4h-4zM36%2036h4v4h-4zM48%2040h4v4h-4zM60%2040h4v4h-4zM0%2044h4v4h-4zM4%2044h4v4h-4zM8%2044h4v4h-4zM36%2044h4v4h-4zM28%2052h4v4h-4zM48%2052h4v4h-4zM0%2056h4v4h-4zM28%2056h4v4h-4zM44%2056h4v4h-4zM52%2056h4v4h-4zM12%2060h4v4h-4zM36%2060h4v4h-4z'/%3E%3C/svg%3E");
      --grass: url("data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20width='64'%20height='56'%20shape-rendering='crispEdges'%3E%3Cpath%20fill='%234f8a30'%20d='M0%200h4v4h-4zM20%200h4v4h-4zM24%200h4v4h-4zM56%200h4v4h-4zM16%204h4v4h-4zM20%204h4v4h-4zM56%204h4v4h-4zM60%204h4v4h-4zM4%2012h4v4h-4zM8%2012h4v4h-4zM12%2012h4v4h-4zM40%2012h4v4h-4zM44%2012h4v4h-4zM60%2012h4v4h-4zM4%2016h4v4h-4zM8%2016h4v4h-4zM28%2016h4v4h-4zM32%2016h4v4h-4zM48%2016h4v4h-4zM8%2020h4v4h-4zM20%2020h4v4h-4zM24%2020h4v4h-4zM60%2020h4v4h-4zM0%2024h4v4h-4zM8%2024h4v4h-4zM12%2024h4v4h-4zM40%2024h4v4h-4zM44%2024h4v4h-4zM52%2024h4v4h-4zM56%2024h4v4h-4zM0%2028h4v4h-4zM36%2028h4v4h-4zM44%2028h4v4h-4zM0%2032h4v4h-4zM24%2032h4v4h-4zM32%2032h4v4h-4zM40%2032h4v4h-4zM44%2032h4v4h-4zM56%2032h4v4h-4zM8%2036h4v4h-4zM24%2036h4v4h-4zM60%2036h4v4h-4zM0%2040h4v4h-4zM16%2040h4v4h-4zM24%2040h4v4h-4zM28%2040h4v4h-4zM4%2044h4v4h-4zM8%2044h4v4h-4zM16%2044h4v4h-4zM28%2044h4v4h-4zM48%2044h4v4h-4zM0%2048h4v4h-4zM52%2048h4v4h-4zM56%2048h4v4h-4zM4%2052h4v4h-4zM28%2052h4v4h-4zM44%2052h4v4h-4zM48%2052h4v4h-4zM52%2052h4v4h-4z'/%3E%3Cpath%20fill='%23559434'%20d='M4%200h4v4h-4zM12%200h4v4h-4zM16%200h4v4h-4zM32%200h4v4h-4zM48%200h4v4h-4zM4%204h4v4h-4zM36%204h4v4h-4zM28%208h4v4h-4zM0%2012h4v4h-4zM20%2012h4v4h-4zM16%2016h4v4h-4zM36%2016h4v4h-4zM0%2020h4v4h-4zM16%2020h4v4h-4zM36%2020h4v4h-4zM32%2024h4v4h-4zM4%2028h4v4h-4zM40%2028h4v4h-4zM16%2032h4v4h-4zM60%2032h4v4h-4zM0%2036h4v4h-4zM28%2036h4v4h-4zM8%2040h4v4h-4zM20%2040h4v4h-4zM32%2044h4v4h-4zM44%2044h4v4h-4zM8%2048h4v4h-4zM56%2052h4v4h-4z'/%3E%3Cpath%20fill='%236aac44'%20d='M8%200h4v4h-4zM28%200h4v4h-4zM36%200h4v4h-4zM52%200h4v4h-4zM0%204h4v4h-4zM32%204h4v4h-4zM52%204h4v4h-4zM8%208h4v4h-4zM20%208h4v4h-4zM32%208h4v4h-4zM56%208h4v4h-4zM16%2012h4v4h-4zM28%2012h4v4h-4zM12%2016h4v4h-4zM44%2016h4v4h-4zM60%2016h4v4h-4zM20%2024h4v4h-4zM56%2028h4v4h-4zM4%2032h4v4h-4zM4%2036h4v4h-4zM32%2036h4v4h-4zM40%2036h4v4h-4zM12%2040h4v4h-4zM48%2040h4v4h-4zM0%2044h4v4h-4zM12%2044h4v4h-4zM24%2044h4v4h-4zM36%2044h4v4h-4zM12%2048h4v4h-4zM40%2048h4v4h-4zM60%2048h4v4h-4zM24%2052h4v4h-4zM60%2052h4v4h-4z'/%3E%3Cpath%20fill='%235d9b3a'%20d='M40%200h4v4h-4zM44%200h4v4h-4zM8%204h4v4h-4zM12%204h4v4h-4zM24%204h4v4h-4zM28%204h4v4h-4zM48%204h4v4h-4zM16%208h4v4h-4zM24%208h4v4h-4zM36%208h4v4h-4zM40%208h4v4h-4zM44%208h4v4h-4zM48%208h4v4h-4zM52%208h4v4h-4zM60%208h4v4h-4zM24%2012h4v4h-4zM32%2012h4v4h-4zM48%2012h4v4h-4zM56%2012h4v4h-4zM20%2016h4v4h-4zM40%2016h4v4h-4zM52%2016h4v4h-4zM56%2016h4v4h-4zM4%2020h4v4h-4zM12%2020h4v4h-4zM28%2020h4v4h-4zM32%2020h4v4h-4zM44%2020h4v4h-4zM48%2020h4v4h-4zM52%2020h4v4h-4zM56%2020h4v4h-4zM16%2024h4v4h-4zM28%2024h4v4h-4zM48%2024h4v4h-4zM60%2024h4v4h-4zM8%2028h4v4h-4zM12%2028h4v4h-4zM16%2028h4v4h-4zM24%2028h4v4h-4zM28%2028h4v4h-4zM32%2028h4v4h-4zM48%2028h4v4h-4zM60%2028h4v4h-4zM8%2032h4v4h-4zM20%2032h4v4h-4zM36%2032h4v4h-4zM48%2032h4v4h-4zM52%2032h4v4h-4zM12%2036h4v4h-4zM16%2036h4v4h-4zM20%2036h4v4h-4zM44%2036h4v4h-4zM48%2036h4v4h-4zM52%2036h4v4h-4zM56%2036h4v4h-4zM4%2040h4v4h-4zM32%2040h4v4h-4zM36%2040h4v4h-4zM40%2040h4v4h-4zM44%2040h4v4h-4zM56%2040h4v4h-4zM60%2040h4v4h-4zM20%2044h4v4h-4zM40%2044h4v4h-4zM52%2044h4v4h-4zM60%2044h4v4h-4zM4%2048h4v4h-4zM16%2048h4v4h-4zM24%2048h4v4h-4zM28%2048h4v4h-4zM44%2048h4v4h-4zM0%2052h4v4h-4zM8%2052h4v4h-4zM12%2052h4v4h-4zM16%2052h4v4h-4zM20%2052h4v4h-4zM32%2052h4v4h-4z'/%3E%3Cpath%20fill='%233f7a28'%20d='M60%200h4v4h-4zM40%204h4v4h-4zM44%204h4v4h-4zM0%208h4v4h-4zM4%208h4v4h-4zM12%208h4v4h-4zM36%2012h4v4h-4zM52%2012h4v4h-4zM0%2016h4v4h-4zM24%2016h4v4h-4zM40%2020h4v4h-4zM4%2024h4v4h-4zM24%2024h4v4h-4zM36%2024h4v4h-4zM20%2028h4v4h-4zM52%2028h4v4h-4zM12%2032h4v4h-4zM28%2032h4v4h-4zM36%2036h4v4h-4zM52%2040h4v4h-4zM56%2044h4v4h-4zM20%2048h4v4h-4zM32%2048h4v4h-4zM36%2048h4v4h-4zM48%2048h4v4h-4zM36%2052h4v4h-4zM40%2052h4v4h-4z'/%3E%3C/svg%3E");
      --bg:#866043; --bg-solid:#c6c6c6; --bg-card:#c6c6c6; --bg-nested:#5f5f5f;
      --bg-topbar:#4a7d2e;
      --body-bg: var(--dirt) 0 0 / 128px 128px repeat, #866043;
      --topbar-bg: var(--grass) 0 0 / 64px 56px repeat, #4a7d2e;
      --text:#2b2b2b; --text-sec:#3f3f3f; --text-muted:#505050;
      --border:#8b8b8b; --border-fine:#373737;
      --accent:#2f7a1e; --accent-2:#ffff55;
      --accent-soft:rgba(47,122,30,0.20); --glow:rgba(255,255,85,0.55);
      --alarm:#ff5555;
      --btn-bg:#5f9a3c; --btn-text:#ffffff;
      --btn-sec-bg:#707070; --btn-sec-text:#ffffff; --sec-hover:#868686;
      --input-bg:#161616; --input-text:#ffffff;
      --shadow:none;
      --danger:#ffffff; --danger-bd:#000000; --danger-hover:#c0392e;

      --brand:#ffffff; --label-color:#373737; --strong:#1e5a10;
      --timer:#2b2b2b; --deg-color:#2b2b2b;
      --slot-value:#ffffff; --slot-label:#e0e0e0; --slot-shadow:2px 2px 0 #1c1c1c;
      --metric-bd:#000000;
      --lang-text:#ffffff; --lang-active-bg:rgba(0,0,0,0.55); --lang-active-text:#ffff55; --lang-active-bd:#000;
      --warn-bg:#c9a66b; --warn-bd:#5a3d1c; --warn-color:#2b1b0a; --warn-icon:#5a3d1c;
      --cal-bg:#161616; --cal-color:#55ff55; --cal-bd:#000;

      --font-serif:"Minecraft","Courier New",ui-monospace,Consolas,monospace;
      --font-sans:"Minecraft","Courier New",ui-monospace,Consolas,monospace;

      --r-sm:0px; --r-md:0px; --r-btn:0px;
      --bw:3px; --blur:0px;
      --btn-border:2px solid #000; --btn-sec-border:2px solid #000;
      --btn-shadow:inset 2px 2px 0 rgba(255,255,255,0.40), inset -3px -3px 0 rgba(0,0,0,0.40);
      --btn-shadow-hover:inset 2px 2px 0 rgba(255,255,255,0.55), inset -3px -3px 0 rgba(0,0,0,0.40), 0 0 0 2px #fff;
      --sec-shadow:inset 2px 2px 0 rgba(255,255,255,0.40), inset -3px -3px 0 rgba(0,0,0,0.40);
      --btn-tshadow:2px 2px 0 rgba(0,0,0,0.55);
      --hover-op:1;
      --fw-head:700; --fw-btn:700; --fw-strong:700;
      --head-align:center;
      --topbar-bw:4px; --topbar-image:linear-gradient(#3a2a1a,#3a2a1a);

      --track-bg:#373737; --track-h:16px; --track-bw:2px; --track-r:0px; --track-bd:2px solid #000; --track-shadow:inset 2px 2px 0 #1a1a1a;
      --thumb-bg:#c6c6c6; --thumb-bd:2px solid #000; --thumb-w:16px; --thumb-h:30px; --thumb-r:0px;
      --thumb-shadow:inset 2px 2px 0 #fff, inset -2px -2px 0 #6b6b6b;
    }

    /* ══════════════ BASE ══════════════ */
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    html { font-size: 16px; }

    body {
      position: relative;
      font-family: var(--font-sans);
      font-size: 14px;
      line-height: 1.5;
      color: var(--text);
      background: var(--body-bg);
      background-attachment: fixed;
      min-height: 100vh;
      transition: color var(--ease);
      overflow-x: hidden;
    }
    [data-theme="minecraft"] body { image-rendering: pixelated; }

    /* camadas de brilho / fagulhas (so aparecem nos temas que definem --sparkle) */
    body::before, body::after {
      content: ''; position: fixed; inset: 0; pointer-events: none; z-index: 0;
      background-image: var(--sparkle);
    }
    body::before { background-size: var(--sp-size1) var(--sp-size1); animation: var(--fx1); }
    body::after  { background-size: var(--sp-size2) var(--sp-size2); background-position: 70px 40px; animation: var(--fx2); }

    @keyframes twinkle {
      0%, 100% { opacity: 0.25; filter: brightness(1); }
      50%      { opacity: 0.95; filter: brightness(1.25); }
    }
    @keyframes rise  { from { background-position: 0 0; }       to { background-position: 0 -260px; } }
    @keyframes rise2 { from { background-position: 70px 40px; } to { background-position: 70px -150px; } }
    @keyframes shimmer { to { background-position: -220% 0; } }
    @keyframes menuIn { from { opacity: 0; transform: translateY(-6px) scale(0.98); } to { opacity: 1; transform: none; } }

    #topbar, main { position: relative; z-index: 1; }

    #topbar {
      position: sticky; top: 0; z-index: 100;
      height: 56px;
      background: var(--topbar-bg);
      border-bottom: var(--topbar-bw) solid transparent;
      border-image: var(--topbar-image) 1;
      backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);
      display: flex; align-items: center; justify-content: space-between;
      padding: 0 20px; gap: 10px;
    }

    .topbar-brand {
      display: flex; align-items: center; gap: 8px;
      font-family: var(--font-serif); font-size: 16px; font-weight: var(--fw-head);
      color: var(--brand); letter-spacing: -0.01em; white-space: nowrap;
    }

    .clay-dot {
      width: 8px; height: 8px; border-radius: 50%;
      background: var(--accent); flex-shrink: 0;
      box-shadow: 0 0 8px var(--glow);
    }

    .topbar-controls { display: flex; align-items: center; gap: 4px; }

    .lang-btn {
      font-family: var(--font-sans); font-size: 12px; font-weight: var(--fw-btn);
      appearance: none;
      border: 1px solid transparent; border-radius: var(--r-btn);
      background: transparent; color: var(--lang-text);
      cursor: pointer; padding: 4px 10px; min-height: unset;
      box-shadow: none; text-shadow: none;
      transition: color var(--ease), background var(--ease), border-color var(--ease);
    }
    .lang-btn::after { display: none; }
    .lang-btn:hover { color: var(--accent); background: var(--accent-soft); opacity: 1; transform: none; box-shadow: none; }
    .lang-btn.active {
      color: var(--lang-active-text); background: var(--lang-active-bg);
      border-color: var(--lang-active-bd); box-shadow: var(--lang-active-shadow);
    }

    /* ══════════════ SELETOR DE TEMAS ══════════════ */
    .theme-picker { position: relative; margin-left: 8px; }

    .theme-trigger {
      display: inline-flex; align-items: center; gap: 8px;
      min-height: 34px; padding: 0 10px 0 8px;
      background: var(--bg-solid); color: var(--text);
      border: 1.5px solid var(--border-fine); border-radius: var(--r-btn);
      box-shadow: none; text-shadow: none;
      font-family: var(--font-sans); font-size: 12px; font-weight: var(--fw-btn);
      overflow: visible;
    }
    .theme-trigger::after { display: none; }
    .theme-trigger:hover { opacity: 1; transform: none; box-shadow: 0 0 0 3px var(--accent-soft); border-color: var(--accent); }
    .theme-trigger svg { transition: transform var(--ease); flex: none; }
    .theme-trigger[aria-expanded="true"] { border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-soft); }
    .theme-trigger[aria-expanded="true"] svg { transform: rotate(180deg); }

    .tp-swatch {
      width: 18px; height: 18px; border-radius: 50%; flex: none;
      border: 1.5px solid rgba(128,128,128,0.45);
    }
    [data-theme="minecraft"] .tp-swatch { border-radius: 0; border-color: #000; }

    .theme-menu {
      position: absolute; right: 0; top: calc(100% + 12px);
      width: min(310px, calc(100vw - 24px));
      max-height: min(78vh, 520px); overflow-y: auto;
      padding: 8px;
      background: var(--bg-solid);
      border: 1.5px solid var(--border-fine);
      border-radius: var(--r-md);
      box-shadow: 0 18px 50px rgba(0,0,0,0.28), var(--shadow);
      animation: menuIn 0.14s ease-out;
      z-index: 200;
    }
    .theme-menu[hidden] { display: none; }
    [data-theme="minecraft"] .theme-menu {
      border: 3px solid #000;
      box-shadow: inset 3px 3px 0 #fff, inset -3px -3px 0 #555;
    }

    .tp-group {
      padding: 8px 10px 4px;
      font-size: 10.5px; font-weight: 800; letter-spacing: 0.1em; text-transform: uppercase;
      color: var(--text-muted);
    }

    .tp-item {
      display: flex; align-items: center; gap: 12px; width: 100%;
      min-height: 54px; padding: 7px 10px;
      margin: 2px 0;
      background: transparent; color: var(--text);
      border: 1.5px solid transparent; border-radius: var(--r-sm);
      box-shadow: none; text-shadow: none; text-align: left;
      white-space: normal; overflow: visible; font-weight: var(--fw-btn);
    }
    .tp-item::after { display: none; }
    .tp-item:hover { opacity: 1; transform: none; box-shadow: none; background: var(--accent-soft); }
    .tp-item.active { border-color: var(--accent); background: var(--accent-soft); }
    .tp-item:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }

    .tp-prev {
      position: relative; width: 54px; height: 38px; flex: none;
      border-radius: 8px; overflow: hidden;
      border: 1px solid rgba(128,128,128,0.40);
    }
    .tp-prev i, .tp-prev b, .tp-prev u { position: absolute; display: block; }
    .tp-prev i { left: 6px; right: 6px; top: 6px; height: 14px; border-radius: 3px; }
    .tp-prev b { left: 6px; bottom: 6px; width: 22px; height: 8px; border-radius: 99px; }
    .tp-prev u { right: 6px; bottom: 6px; width: 11px; height: 8px; border-radius: 99px; }

    .tp-txt { display: flex; flex-direction: column; min-width: 0; flex: 1; }
    .tp-title { font-size: 14px; font-weight: var(--fw-strong); color: var(--text); line-height: 1.25; }
    .tp-sub { font-size: 11.5px; font-weight: 400; color: var(--text-muted); line-height: 1.3; }
    .tp-check { font-size: 15px; font-weight: 800; color: var(--accent); opacity: 0; }
    .tp-item.active .tp-check { opacity: 1; }

    /* ══════════════ CONTEUDO ══════════════ */
    main { width: min(760px, 100%); margin: 0 auto; padding: 40px 20px 64px; }

    header { margin-bottom: 30px; text-align: var(--head-align); }

    h1 {
      font-family: var(--font-serif);
      font-size: clamp(26px, 7vw, 40px);
      font-weight: var(--fw-head);
      line-height: 1.2; color: var(--text); letter-spacing: -0.02em;
    }
    h1::before {
      content: ''; display: inline-block;
      width: 10px; height: 10px; border-radius: 50%;
      background: var(--accent); margin-right: 10px;
      vertical-align: middle; position: relative; top: -2px;
    }

    .subtitle { font-size: 14px; color: var(--text-muted); margin-top: 8px; line-height: 1.5; }
    .subtitle strong { color: var(--strong); font-weight: 700; }

    .grid { display: grid; gap: 18px; }

    .panel {
      position: relative;
      background: var(--bg-card);
      border: var(--bw) solid var(--border);
      border-radius: var(--r-md);
      padding: 24px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(var(--blur)); -webkit-backdrop-filter: blur(var(--blur));
      transition: background var(--ease), border-color var(--ease), box-shadow var(--ease);
    }
    .panel::after {
      content: var(--deco);
      position: absolute; top: 10px; right: 16px;
      font-size: 13px; color: var(--accent-2);
      pointer-events: none; animation: twinkle 2.8s ease-in-out infinite;
    }

    @keyframes alarmPulse {
      0%, 100% { border-color: var(--border); }
      50% { border-color: var(--alarm); box-shadow: 0 0 0 4px var(--glow), 0 0 28px var(--glow); }
    }
    .alarm { animation: alarmPulse 0.6s linear infinite; }

    .section-label {
      display: block; font-size: 11px; font-weight: 800;
      letter-spacing: 0.1em; text-transform: uppercase;
      color: var(--label-color); margin-bottom: 12px;
    }

    .status { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }

    .metric {
      background: var(--bg-nested);
      border: 1px solid var(--metric-bd);
      border-radius: var(--r-sm);
      min-height: 84px; padding: 14px;
    }

    .label, .field-label {
      display: block; font-size: 11px; font-weight: 700;
      letter-spacing: 0.07em; text-transform: uppercase; color: var(--text-muted);
    }
    .label { margin-bottom: 8px; }
    .field-label { margin-bottom: 6px; }
    .metric .label { color: var(--slot-label); }

    .value {
      display: block; font-size: 22px; font-weight: var(--fw-strong);
      color: var(--text); overflow-wrap: anywhere;
    }
    .metric .value { color: var(--slot-value); text-shadow: var(--slot-shadow); }

    .divider { border: none; border-top: 1.5px dashed var(--border); margin: 16px 0; }

    .row { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; margin-top: 10px; }
    .row-label { width: 100%; margin-bottom: 0; }

    /* ---------- botoes ---------- */
    button {
      position: relative; overflow: hidden;
      appearance: none;
      border: var(--btn-border);
      border-radius: var(--r-btn);
      background: var(--btn-bg); color: var(--btn-text);
      cursor: pointer;
      font-family: var(--font-sans); font-size: 13px; font-weight: var(--fw-btn);
      min-height: 38px; padding: 0 18px;
      box-shadow: var(--btn-shadow); text-shadow: var(--btn-tshadow);
      transition: transform var(--ease), box-shadow var(--ease), background var(--ease), color var(--ease), opacity var(--ease);
      white-space: nowrap;
    }
    button::after {
      content: ''; position: absolute; top: 0; left: -80%; width: 50%; height: 100%;
      background: linear-gradient(100deg, transparent, rgba(255,255,255,0.55), transparent);
      transform: skewX(-20deg); pointer-events: none; opacity: var(--shine);
    }
    button:hover { opacity: var(--hover-op); transform: translateY(var(--lift)); box-shadow: var(--btn-shadow-hover); }
    button:hover::after { left: 130%; transition: left 0.6s ease; }
    button:active { transform: translateY(1px) scale(0.98); }
    button:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }

    button.secondary {
      background: var(--btn-sec-bg); color: var(--btn-sec-text);
      border: var(--btn-sec-border); box-shadow: var(--sec-shadow);
    }
    button.secondary:hover { opacity: 1; background: var(--sec-hover); box-shadow: var(--sec-shadow); }
    [data-theme="minecraft"] button.secondary:hover { box-shadow: var(--btn-shadow-hover); }

    button.danger {
      background: transparent; color: var(--danger);
      border: 1.5px solid var(--danger-bd); box-shadow: none;
    }
    button.danger:hover { background: var(--danger-hover); opacity: 1; box-shadow: none; }
    [data-theme="minecraft"] button.danger { background: #a8322b; border: 2px solid #000; box-shadow: var(--btn-shadow); }
    [data-theme="minecraft"] button.danger:hover { background: #c0392e; box-shadow: var(--btn-shadow-hover); }

    /* ---------- campos ---------- */
    input[type="number"] {
      width: 100px; min-height: 38px;
      border: 1.5px solid var(--border); border-radius: var(--r-sm);
      font-family: var(--font-sans); font-size: 14px; padding: 0 10px;
      background: var(--input-bg); color: var(--input-text);
      transition: background var(--ease), border-color var(--ease), color var(--ease);
    }
    input[type="number"]:focus { outline: none; border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-soft); }
    [data-theme="minecraft"] input[type="number"] { border: 2px solid #a0a0a0; box-shadow: inset 2px 2px 0 #000; }

    input[type="range"] {
      -webkit-appearance: none; appearance: none;
      width: 100%; height: 30px; background: transparent; cursor: pointer;
    }
    input[type="range"]::-webkit-slider-runnable-track {
      box-sizing: border-box; height: var(--track-h);
      border-radius: var(--track-r); background: var(--track-bg);
      border: var(--track-bd); box-shadow: var(--track-shadow);
    }
    input[type="range"]::-moz-range-track {
      box-sizing: border-box; height: var(--track-h);
      border-radius: var(--track-r); background: var(--track-bg);
      border: var(--track-bd); box-shadow: var(--track-shadow);
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none; box-sizing: border-box;
      width: var(--thumb-w); height: var(--thumb-h);
      margin-top: calc((var(--track-h) - 2 * var(--track-bw) - var(--thumb-h)) / 2);
      border-radius: var(--thumb-r); background: var(--thumb-bg);
      border: var(--thumb-bd); box-shadow: var(--thumb-shadow);
    }
    input[type="range"]::-moz-range-thumb {
      box-sizing: border-box;
      width: var(--thumb-w); height: var(--thumb-h);
      border-radius: var(--thumb-r); background: var(--thumb-bg);
      border: var(--thumb-bd); box-shadow: var(--thumb-shadow);
    }

    .timer-fields {
      display: grid;
      grid-template-columns: minmax(90px, 108px) minmax(90px, 108px) auto auto;
      gap: 10px; align-items: end; margin-top: 10px;
    }

    .timer-inline-status {
      display: flex; align-items: center; gap: 12px;
      margin-top: 14px; padding-top: 14px; border-top: 1.5px dashed var(--border);
    }
    .timer-inline-status .value { font-size: 28px; color: var(--timer); }

    .slider-line { display: grid; grid-template-columns: 1fr 64px; align-items: center; gap: 12px; margin-top: 10px; }
    .slider-deg {
      font-family: var(--font-serif); font-size: 20px; font-weight: var(--fw-strong);
      color: var(--deg-color); text-align: right;
    }

    .track-state { font-size: 12px; color: var(--text-muted); margin-top: 6px; }

    .cal-values {
      font-size: 12px; font-family: var(--font-mono); font-variant-numeric: tabular-nums;
      color: var(--cal-color); margin-top: 10px; padding: 10px 12px;
      background: var(--cal-bg); border: 1px solid var(--cal-bd); border-radius: var(--r-sm);
    }

    .warn-note {
      display: flex; align-items: flex-start; gap: 8px;
      margin-top: 12px; padding: 10px 12px;
      background: var(--warn-bg); border: 1px solid var(--warn-bd);
      border-radius: var(--r-sm);
      font-size: 12px; color: var(--warn-color); line-height: 1.5;
    }
    .warn-note::before { content: '⚠'; color: var(--warn-icon); font-size: 13px; flex-shrink: 0; margin-top: 1px; }

    .mono { font-family: var(--font-mono); font-variant-numeric: tabular-nums; }

    .desc { font-size: 13px; color: var(--text-muted); line-height: 1.55; margin-bottom: 14px; }
    .desc strong { color: var(--strong); }

    /* ══════════════ EXTRAS POR TEMA ══════════════ */

    /* titulo com degradê animado: barbie */
    [data-theme="barbie"] h1 {
      background-size: 220% 100%;
      -webkit-background-clip: text; background-clip: text;
      -webkit-text-fill-color: transparent; color: transparent;
      animation: shimmer 6s linear infinite;
      filter: drop-shadow(0 2px 8px var(--glow));
      background-image: linear-gradient(100deg,#ff4fa8 0%,#e91e8c 30%,#a65ee8 55%,#ff74c0 80%,#ff4fa8 100%);
    }

    /* barbie: brilhos */
    [data-theme="barbie"] .clay-dot { width: auto; height: auto; background: none; border-radius: 0; box-shadow: none; }
    [data-theme="barbie"] .clay-dot::before { content: '✦'; font-size: 16px; line-height: 1; color: var(--accent); text-shadow: 0 0 8px var(--glow); animation: twinkle 2s ease-in-out infinite; }
    [data-theme="barbie"] h1::before {
      content: '✦'; width: auto; height: auto; background: none; border-radius: 0;
      -webkit-text-fill-color: var(--accent); color: var(--accent);
      font-size: 0.55em; top: -4px; animation: twinkle 2s ease-in-out infinite;
    }
    [data-theme="barbie"] h1::after {
      content: '✦'; display: inline-block; margin-left: 10px;
      -webkit-text-fill-color: var(--accent-2); color: var(--accent-2);
      font-size: 0.4em; vertical-align: middle; position: relative; top: -10px;
      animation: twinkle 2.6s ease-in-out infinite 0.8s;
    }

    /* minecraft */
    [data-theme="minecraft"] .clay-dot {
      width: 20px; height: 20px; border-radius: 0; border: 2px solid #000;
      background: linear-gradient(#5d9b3a 0 34%, #866043 34% 100%);
      box-shadow: inset 2px 2px 0 rgba(255,255,255,0.25), inset -2px -2px 0 rgba(0,0,0,0.30);
    }
    [data-theme="minecraft"] .topbar-brand, [data-theme="minecraft"] .lang-btn { text-shadow: 2px 2px 0 rgba(0,0,0,0.55); }
    [data-theme="minecraft"] .lang-btn:hover { color: #ffff55; background: rgba(0,0,0,0.25); }
    [data-theme="minecraft"] h1 {
      color: #ffffff; letter-spacing: 0; text-shadow: 4px 4px 0 #3f3f3f;
    }
    [data-theme="minecraft"] h1::before {
      width: 34px; height: 34px; border-radius: 0; border: 3px solid #000; top: -3px; margin-right: 14px;
      background: linear-gradient(#5d9b3a 0 34%, #866043 34% 100%);
      box-shadow: inset 3px 3px 0 rgba(255,255,255,0.25), inset -3px -3px 0 rgba(0,0,0,0.30);
    }
    [data-theme="minecraft"] .subtitle { color: #ffffff; text-shadow: 2px 2px 0 #3f3f3f; }
    [data-theme="minecraft"] .subtitle strong { color: #ffff55; }
    [data-theme="minecraft"] .panel {
      border-color: #000;
      box-shadow: inset 3px 3px 0 #ffffff, inset -3px -3px 0 #555555;
    }
    [data-theme="minecraft"] .metric {
      border: 2px solid; border-color: #373737 #ffffff #ffffff #373737;
    }
    [data-theme="minecraft"] .divider, [data-theme="minecraft"] .timer-inline-status { border-color: #8b8b8b; }
    [data-theme="minecraft"] .cal-values { text-shadow: 1px 1px 0 #1c3a1c; }
    [data-theme="minecraft"] .theme-trigger { border: 2px solid #000; background: #c6c6c6; box-shadow: inset 2px 2px 0 #fff, inset -2px -2px 0 #555; }

    @media (max-width: 620px) {
      #topbar { padding: 0 12px; }
      .topbar-brand span:last-child { display: none; }
      .status { grid-template-columns: 1fr; }
      .timer-fields { grid-template-columns: 1fr 1fr; }
      button { flex: 1 1 130px; }
      .lang-btn, .theme-trigger { flex: 0 0 auto; }
      .tp-item { flex: 0 0 auto; }
    }
    @media (max-width: 460px) {
      .tp-name { display: none; }
      .theme-picker { margin-left: 4px; }
      .lang-btn { padding: 4px 7px; }
    }

    @media (prefers-reduced-motion: reduce) {
      *, *::before, *::after { animation: none !important; transition: none !important; }
    }
  </style>
</head>
<body>

  <div id="topbar">
    <div class="topbar-brand">
      <span class="clay-dot"></span>
      <span data-i18n="title">Fogão — 1 manípulo</span>
    </div>
    <div class="topbar-controls">
      <button class="lang-btn" id="btn-pt" onclick="setLang('pt')">PT</button>
      <button class="lang-btn" id="btn-en" onclick="setLang('en')">EN</button>
      <button class="lang-btn" id="btn-es" onclick="setLang('es')">ES</button>
      <div class="theme-picker" id="themePicker">
        <button class="theme-trigger" id="themeTrigger" onclick="toggleThemeMenu()" aria-haspopup="listbox" aria-expanded="false" aria-controls="themeMenu" aria-label="Tema">
          <span class="tp-swatch" id="tpSwatch"></span>
          <span class="tp-name" id="tpName">Claro</span>
          <svg width="10" height="6" viewBox="0 0 10 6" aria-hidden="true"><path d="M1 1l4 4 4-4" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>
        </button>
        <div class="theme-menu" id="themeMenu" role="listbox" hidden></div>
      </div>
    </div>
  </div>

  <main>
    <header>
      <h1 data-i18n="title">Fogão — 1 manípulo</h1>
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
          <button class="secondary" onclick="marcar('baixo')"  data-i18n="marcar_baixo">Marcar Baixo</button>
          <button class="secondary" onclick="liberarServo()"   data-i18n="liberar_btn">🔓 Liberar servo</button>
        </div>

        <hr class="divider">

        <span class="field-label row-label" data-i18n="girar">Girar manípulo com o servo:</span>
        <div class="row">
          <button onclick="irPara('alto')"  data-i18n="ir_alto">Ir para Alto</button>
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
          <button class="secondary" onclick="salvarCalibracao('baixo')"   data-i18n="salvar_baixo">Salvar baixo</button>
        </div>
        <div class="cal-values" id="calValues">Fechado: -- | Alto: -- | Baixo: --</div>
        <div class="warn-note" data-i18n="aviso">
          Não force o manípulo contra o servo energizado. O servo é liberado automaticamente ao fim de cada movimento; só gire na mão com o servo liberado.
        </div>
      </div>

    </section>
  </main>

  <script>
    /* TEMAS */
    const htmlEl = document.documentElement;

    function store(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
    function load(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }

    // c = [fundo, cartao, destaque, detalhe] usados no mini-preview
    const THEMES = [
      { id: "claro",     g: 0, c: ["#f8f8f6", "#ffffff", "#d97757", "#121212"] },
      { id: "escuro",    g: 0, c: ["#1a1916", "#2a2925", "#d97757", "#f0ede8"] },
      { id: "barbie",    g: 1, c: ["#ffd6ee", "#ffffff", "#e91e8c", "#a65ee8"] },
      { id: "minecraft", g: 1, c: ["#866043", "#c6c6c6", "#5f9a3c", "#ffff55"] }
    ];

    function currentTheme() { return htmlEl.getAttribute("data-theme") || "claro"; }

    function renderThemeMenu() {
      const menu = document.getElementById("themeMenu");
      if (!menu) return;
      const t = LANGS[currentLang];
      const cur = currentTheme();
      let h = "", g = -1;
      THEMES.forEach(th => {
        if (th.g !== g) {
          g = th.g;
          h += `<div class="tp-group">${g === 0 ? t.tema_padrao : t.tema_especiais}</div>`;
        }
        const info = t.themes[th.id];
        const on = th.id === cur;
        h += `<button class="tp-item${on ? " active" : ""}" role="option" aria-selected="${on}" onclick="pickTheme('${th.id}')">` +
             `<span class="tp-prev" style="background:${th.c[0]}"><i style="background:${th.c[1]}"></i><b style="background:${th.c[2]}"></b><u style="background:${th.c[3]}"></u></span>` +
             `<span class="tp-txt"><span class="tp-title">${info[0]}</span><span class="tp-sub">${info[1]}</span></span>` +
             `<span class="tp-check">✓</span></button>`;
      });
      menu.innerHTML = h;
      const c = THEMES.find(x => x.id === cur) || THEMES[0];
      document.getElementById("tpName").textContent = t.themes[c.id][0];
      document.getElementById("tpSwatch").style.background = `linear-gradient(135deg, ${c.c[0]} 0 50%, ${c.c[2]} 50% 100%)`;
      document.getElementById("themeTrigger").setAttribute("aria-label", t.tema + ": " + t.themes[c.id][0]);
    }

    function toggleThemeMenu(open) {
      const menu = document.getElementById("themeMenu");
      const trg = document.getElementById("themeTrigger");
      const willOpen = open === undefined ? menu.hidden : open;
      menu.hidden = !willOpen;
      trg.setAttribute("aria-expanded", willOpen ? "true" : "false");
      if (willOpen) {
        const it = menu.querySelector(".tp-item.active") || menu.querySelector(".tp-item");
        if (it) it.focus();
      }
    }

    function applyTheme(id) {
      if (!THEMES.some(x => x.id === id)) id = "claro";
      htmlEl.setAttribute("data-theme", id);
      store("theme", id);
      renderThemeMenu();
    }

    function pickTheme(id) {
      applyTheme(id);
      toggleThemeMenu(false);
      document.getElementById("themeTrigger").focus();
    }

    document.addEventListener("click", e => {
      if (!e.target.closest("#themePicker")) toggleThemeMenu(false);
    });
    document.addEventListener("keydown", e => {
      const menu = document.getElementById("themeMenu");
      if (!menu || menu.hidden) return;
      if (e.key === "Escape") {
        toggleThemeMenu(false);
        document.getElementById("themeTrigger").focus();
      } else if (e.key === "ArrowDown" || e.key === "ArrowUp") {
        e.preventDefault();
        const items = Array.from(menu.querySelectorAll(".tp-item"));
        let i = items.indexOf(document.activeElement);
        i = e.key === "ArrowDown" ? (i + 1) % items.length : (i - 1 + items.length) % items.length;
        items[i].focus();
      }
    });

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
        marcar_baixo:    "Marcar Baixo",
        liberar_btn:     "🔓 Liberar servo",
        girar:           "Girar manípulo com o servo:",
        ir_alto:         "Ir para Alto",
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
        salvar_baixo:    "Salvar baixo",
        aguardando:      "Aguardando comando.",
        movendo:         "Movendo servo...",
        fechando:        "Fechando automaticamente...",
        concluido:       "Movimento concluído.",
        aviso:           'Não force o manípulo contra o servo energizado. Para girar na mão, clique em "Liberar servo" antes.',
        liberado:        "liberado",
        lvl_Fechado:     "Fechado",
        lvl_FogoAlto:    "Fogo alto",
        lvl_FogoBaixo:   "Fogo baixo",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Desconhecido",
        tema: "Tema", tema_padrao: "Padrão", tema_especiais: "Especiais",
        themes: {
          claro:     ["Claro", "Creme, o visual original"],
          escuro:    ["Escuro", "Grafite quente, o original"],
          barbie:    ["Rosa glitter", "Rosa, lilás e brilhos"],
          minecraft: ["Minecraft", "Terra, grama e blocos"]
        },
        cal: (f,a,b) => `Fechado: ${f}° | Alto: ${a}° | Baixo: ${b}°`,
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
        marcar_baixo:    "Mark Low",
        liberar_btn:     "🔓 Release servo",
        girar:           "Turn knob with servo:",
        ir_alto:         "Go to High",
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
        salvar_baixo:    "Save low",
        aguardando:      "Waiting for command.",
        movendo:         "Moving servo...",
        fechando:        "Closing automatically...",
        concluido:       "Movement complete.",
        aviso:           'Do not force the knob against an energized servo. To turn by hand, click "Release servo" first.',
        liberado:        "released",
        lvl_Fechado:     "Closed",
        lvl_FogoAlto:    "High flame",
        lvl_FogoBaixo:   "Low flame",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Unknown",
        tema: "Theme", tema_padrao: "Default", tema_especiais: "Special",
        themes: {
          claro:     ["Light", "Cream, the original look"],
          escuro:    ["Dark", "Warm graphite, the original"],
          barbie:    ["Pink glitter", "Pink, lilac and sparkles"],
          minecraft: ["Minecraft", "Dirt, grass and blocks"]
        },
        cal: (f,a,b) => `Closed: ${f}° | High: ${a}° | Low: ${b}°`,
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
        marcar_baixo:    "Marcar Bajo",
        liberar_btn:     "🔓 Liberar servo",
        girar:           "Girar perilla con el servo:",
        ir_alto:         "Ir a Alto",
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
        salvar_baixo:    "Guardar bajo",
        aguardando:      "Esperando comando.",
        movendo:         "Moviendo servo...",
        fechando:        "Cerrando automáticamente...",
        concluido:       "Movimiento completado.",
        aviso:           'No fuerce la perilla contra el servo energizado. Para girar a mano, haga clic en "Liberar servo" antes.',
        liberado:        "liberado",
        lvl_Fechado:     "Cerrado",
        lvl_FogoAlto:    "Llama alta",
        lvl_FogoBaixo:   "Llama baja",
        lvl_Manual:      "Manual",
        lvl_Desc:        "Desconocido",
        tema: "Tema", tema_padrao: "Estándar", tema_especiais: "Especiales",
        themes: {
          claro:     ["Claro", "Crema, el diseño original"],
          escuro:    ["Oscuro", "Grafito cálido, el original"],
          barbie:    ["Rosa glitter", "Rosa, lila y brillos"],
          minecraft: ["Minecraft", "Tierra, césped y bloques"]
        },
        cal: (f,a,b) => `Cerrado: ${f}° | Alto: ${a}° | Bajo: ${b}°`,
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
      renderThemeMenu();
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

    /* INICIALIZA TEMA (migra valores antigos "light"/"dark") */
    (function initTheme() {
      let s = load("theme");
      if (s === "light") s = "claro";
      if (s === "dark") s = "escuro";
      if (!THEMES.some(x => x.id === s)) {
        s = matchMedia("(prefers-color-scheme: dark)").matches ? "escuro" : "claro";
      }
      applyTheme(s);
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
      calValuesEl.textContent = t.cal(lastCal.fechado, lastCal.alto, lastCal.baixo);
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
  servoMotor.attach(SERVO_PIN, 450, 2550);
  servoMotor.write(logicalToServoDeg(currentDeg));
  servoAttached = true;
}

void releaseServo() {
  if (!servoAttached) {
    return;
  }

  servoMotor.detach();
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  gpio_set_pull_mode((gpio_num_t)SERVO_PIN, GPIO_PULLDOWN_ONLY);
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
  holdActive = false; // cancela qualquer hold pendente
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
  calBaixo = preferences.getInt("baixo", POS_FOGO_BAIXO);
}

void saveCalibration() {
  preferences.putInt("fechado", calFechado);
  preferences.putInt("alto", calAlto);
  preferences.putInt("baixo", calBaixo);
}

void sendStatus() {
  String json;
  json.reserve(240);
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

  // Se já estiver na posição de fechado ou muito próximo (<= 2 graus),
  // energiza e aplica torque direto em calFechado para garantir assentamento completo
  if (abs(currentDeg - calFechado) <= 2) {
    currentDeg = calFechado;
    targetDeg = calFechado;
    rampActive = false;
    holdActive = false;
    attachServoIfNeeded();
    writeServoLogical(calFechado);
    moving = false;
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
    server.send(400, "application/json", "{\"erro\":\"Use level=fechado, alto ou baixo\"}");
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
    server.send(400, "application/json", "{\"erro\":\"Use level=fechado, alto ou baixo\"}");
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

  // Calculo de rampa suave proporcional a distancia (15ms por grau, min 450ms, max 2500ms)
  // Elimina trancos e movimentos bruscos ao clicar ou arrastar o slider
  int diffDeg = abs(targetDeg - currentDeg);
  uint32_t moveDuration = (uint32_t)diffDeg * 15UL;
  if (moveDuration < 450UL) moveDuration = 450UL;
  if (moveDuration > 2500UL) moveDuration = 2500UL;

  startMove(targetDeg, moveDuration);
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
      // Rampa concluída: escreve e trava posição final, servo permanece energizado
      writeServoLogical(rampToDeg);
      rampActive = false;
      holdActive = false;
      moving = false;
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
