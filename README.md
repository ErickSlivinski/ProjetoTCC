# Projeto Cooktop Inteligente IoT (FogaoServoWeb)

Firmware Arduino para ESP32 e sistema de automação e controle remoto seguro para cooktops a gás, desenvolvido com microcontrolador ESP32 e servomotor de alto torque **MG996R**.

---

## Versão Atual — Funcionalidades

- **Servo Motor MG996R:** Engrenagens de metal, torque de até **13 kgf.cm**, garantindo rotação firme contra a mola da válvula do fogão.
- **Transmissão por Engrenagens Paralelas:**
  - **Engrenagem do Servo:** `Eng22DenteServo.stl` (22 dentes, Ø 34.5 mm)
  - **Engrenagem da Base da Válvula:** `Eng25DentesBase.stl` (25 dentes, Ø 36.5 mm, acoplamento D-shaft)
  - **Relação de Transmissão:** $i = 25/22 \approx 1.136$ (+13.6% de multiplicação de torque)
- **Interface Web Responsiva** com 4 temas visuais (claro, escuro, Barbie, Minecraft)
- **Controle de 3 níveis:** Fogo Alto, Fogo Baixo e Fechado (sem fogo médio)
- **Movimento suave:** Rampa proporcional à distância (15 ms/grau, mín. 450 ms, máx. 2500 ms)
- **Timer automático** com minutos e segundos — fecha a válvula ao expirar
- **Calibração pela tela:** ângulos salvos na memória permanente (`Preferences`)
- **Internacionalização:** interface em Português, Inglês e Espanhol
- **Operação híbrida:** botão **🔓 Liberar servo** no app desacopla o motor para operação manual sem forçar as engrenagens

---

## Recursos do App

- ESP32 em modo Access Point — SSID `FOGAO-ESP32`, senha `12345678`
- Página web servida em `http://192.168.4.1`
- Slider manual de 0 a 180° para calibração fina
- Botões de nível: **Fogo Alto**, **Fogo Baixo**, **Fechar agora**
- Rota `/status` em JSON para atualização em tempo real (polling a cada 500 ms)
- Sincronização de posição via botão **Marcar posição atual** (sem acionar o motor)

---

## Bibliotecas Necessárias

Instale pela Arduino IDE:
- `ESP32Servo`
- Core de placas `esp32` para Arduino

*(As bibliotecas `WiFi.h`, `WebServer.h` e `Preferences.h` já fazem parte do core ESP32.)*

---

## Comportamento do Servo

O MG996R **não reporta posição** de volta ao ESP32. O firmware gerencia o estado de forma segura:

1. **Movimento:** Ao acionar qualquer botão ou slider, o servo se acopla automaticamente e executa a rampa suave até o destino.
2. **Hold:** Ao chegar na posição, o servo permanece energizado e travado, mantendo o manípulo no lugar.
3. **Liberar manualmente:** O botão **🔓 Liberar servo** no app chama `releaseServo()` — o motor desacopla e o manípulo pode ser girado à mão.
4. **Sincronizar após ajuste manual:** Use **Marcar posição atual** para informar ao sistema onde o manípulo está sem acionar o motor.
5. **Fechamento pelo Timer:** Ao expirar, o servo acopla e executa o fechamento automaticamente.

> **Segurança:** O acendimento inicial (do fechado para a chama) é sempre feito presencialmente. O sistema foi projetado apenas para fechar ou reduzir a chama remotamente.

*Os ângulos calibrados ficam gravados na flash do ESP32 e sobrevivem a reinicializações.*

---

## Ajustes no Firmware

No arquivo `FogaoTemas.ino`, configure:

```cpp
static const int SERVO_PIN = 26;              // GPIO do sinal do servo
static const bool INVERT_SERVO_DIRECTION = true; // inverte o sentido de giro

// Posições padrão (sobrescritas pela calibração salva)
static const int POS_FECHADO    = 0;
static const int POS_FOGO_ALTO  = 100;
static const int POS_FOGO_BAIXO = 180;
```

Para trocar a rede Wi-Fi criada pelo ESP32:

```cpp
static const char *AP_SSID     = "FOGAO-ESP32";
static const char *AP_PASSWORD = "12345678";
```

---

## Rotas HTTP

| Método | Rota | Descrição |
|--------|------|-----------|
| `GET` | `/` | Interface web |
| `GET` | `/status` | Estado atual em JSON (nível, posição, timer, servo anexado) |
| `GET` | `/fechar` | Fecha o manípulo e cancela o timer |
| `GET` | `/abrir` | Atalho: marca fogo alto (compatibilidade) |
| `GET` | `/setServo?pos=90` | Move para posição específica (0–180°) |
| `POST` | `/timer?min=10&sec=30` | Inicia timer |
| `POST` | `/setTimer?min=10&sec=30` | Alias de `/timer` |
| `POST` | `/goto?level=alto` | Move para nível calibrado (`alto` ou `baixo`) |
| `POST` | `/goto?level=baixo` | Move para nível calibrado de fogo baixo |
| `POST` | `/calibrate?level=alto&pos=80` | Salva ângulo calibrado na memória |
| `POST` | `/mark?level=fechado\|alto\|baixo` | Sincroniza posição assumida sem mover o motor |
| `POST` | `/release` | Libera/desacopla o servo para operação manual |
| `POST` | `/cancelTimer` | Cancela o timer sem fechar |

---

## Passo a Passo para Uso

### 1. Preparar o Ambiente
1. Instale a [Arduino IDE](https://www.arduino.cc/en/software).
2. No gerenciador de placas, instale o core `esp32`.
3. No gerenciador de bibliotecas, instale `ESP32Servo`.
4. Abra o arquivo `FogaoTemas.ino`.
5. Selecione a placa ESP32 correta e a porta USB correspondente.

### 2. Montar o Hardware
1. Conecte o sinal do servo no **GPIO26**.
2. Alimente o servo com fonte externa estável de **6 V / 2.5 A+** (não pelo ESP32).
3. Ligue o **GND** da fonte do servo ao **GND** do ESP32.
4. Não fixe o servo ao manípulo com força antes de testar os ângulos.
5. Faça os primeiros testes com o gás fechado.

### 3. Gravar e Conectar
1. Grave o firmware no ESP32 pela Arduino IDE.
2. Conecte ao Wi-Fi `FOGAO-ESP32` (senha `12345678`).
3. Acesse `http://192.168.4.1` no navegador.

### 4. Calibrar
1. Clique em **🔓 Liberar servo**.
2. Gire o manípulo manualmente até a posição **fechado** e clique em **Marcar posição atual → Fechado**.
3. Gire até **fogo alto** e marque.
4. Gire até **fogo baixo** e marque.
5. Use o slider para ajuste fino e clique em **Salvar** em cada nível.
6. Teste os botões **Fogo Alto**, **Fogo Baixo** e **Fechar agora**.

### 5. Inversão de Sentido de Giro
Se o servo girar ao contrário, altere em `FogaoTemas.ino`:
```cpp
static const bool INVERT_SERVO_DIRECTION = false; // ou true
```

---

## Estrutura de Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `FogaoTemas.ino` | Firmware principal — controle, interface web e temas |
| `CódigoWeb.ino` | Versão anterior do firmware (referência) |
| `Base996r.stl` | Suporte mecânico estrutural do servo MG996R |
| `BaseComManipulo.stl` | Base de comando com encaixe do manípulo do fogão |
| `MontagemFinal1.stl` | Montagem 3D completa do conjunto |
| `Eng22DenteServo.stl` | Engrenagem do servo (22 dentes) |
| `Eng25DentesBase.stl` | Engrenagem da base da válvula (25 dentes) |
| `Cobertura_Ajuste3.3mf` | Cobertura protetora do mecanismo |
| `EsquemaConexões.png` | Diagrama elétrico das conexões |
| `esquema_eletrico_visual.html` | Esquema elétrico interativo |
| `DOSSIE_TECNICO_PROJETO.md` | Documentação técnica completa |
