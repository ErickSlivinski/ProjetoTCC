# Projeto Cooktop Inteligente IoT (FogaoServoWeb)

Firmware Arduino para ESP32 e sistema de automação e controle remoto seguro para cooktops a gás, desenvolvido com microcontrolador ESP32 e servomotor de alto torque **MG996R**.

---

## 🚀 Novidades da Versão 2.0 (Montagem Atualizada)

- **Servo Motor MG996R:** Substituição do micro servo SG90 pelo MG996R com engrenagens de metal e torque de até **13 kgf.cm**, eliminando perda de força e garantindo rotação firme contra a mola da válvula.
- **Transmissão Externa por Engrenagens Paralelas:**
  - **Engrenagem do Servo:** `Eng22DenteServo.stl` (22 dentes, Ø 34.5 mm)
  - **Engrenagem da Base da Válvula:** `Eng25DentesBase.stl` (25 dentes, Ø 36.5 mm, acoplamento D-shaft)
  - **Distância entre Centros:** ~34.0 mm
  - **Relação de Transmissão:** $i = 25/22 \approx 1.136$ (+13.6% de multiplicação de torque).
- **Operação Híbrida e Segurança:**
  - O manípulo pode ser operado manualmente a qualquer momento.
  - O aplicativo gerencia com precisão o controle entre **Chama Alta**, **Chama Média**, **Chama Baixa** e **Desligamento/Fechamento Automático**.
  - Por segurança contra liberação indevida de gás, o acendimento inicial a partir do fechado é feito presencialmente pelo usuário.

---

## ⚙️ Recursos

- ESP32 em modo Access Point com SSID `FOGAO-ESP32`.
- Página web responsiva em português servida em `http://192.168.4.1`.
- Fluxo pensado para abertura manual e fechamento automático.
- Calibração pela própria tela: ajuste o slider e salve o ângulo como fechado, alto, médio ou baixo.
- Botões para ir diretamente para os valores calibrados: fogo alto, médio ou baixo.
- Timer com minutos e segundos que fecha automaticamente a válvula ao expirar.
- Animação visual do slider durante o fechamento automático ou acionamento por botão.
- Slider manual de 0 a 180 graus para calibração fina.
- Rota `/status` em JSON para atualização dinâmica em tempo real.

---

## 📚 Bibliotecas Necessárias

Instale pela Arduino IDE:
- `ESP32Servo`
- Core de placas `esp32` para Arduino

*(As bibliotecas `WiFi.h` e `WebServer.h` já fazem parte nativa do core ESP32).*

---

## 🔌 Ligação Sugerida

- **Sinal do servo (fio laranja/amarelo):** GPIO26 do ESP32.
- **VCC do servo (fio vermelho):** Fonte 5V a 6V estável e dedicada (mínimo 2.5A a 3A para torque máximo do MG996R).
- **GND do servo (fio marrom/preto):** GND da fonte externa e GND do ESP32 interligados (**GND comum obrigatório**).

> [!WARNING]
> Use fonte externa dedicada com corrente suficiente para o servo MG996R. Nunca alimente o servo diretamente pelo pino 5V/3.3V do ESP32 ou pela porta USB. Teste os ângulos com o fogão desligado antes de acoplar ao manípulo.

---

## 🔄 Funcionamento e Modo Standby (Auto-Release)

Um servo comum de hobby não envia a posição real de volta para o ESP32. O firmware gerencia o movimento de forma inteligente e segura:

1. **Auto-Release Transparente:** Após QUALQUER comando de movimento (seja botões de nível, slider ou fechamento automático por timer), o servo se move de forma suave e **é liberado automaticamente (`releaseServo() / detach`)** logo ao final do percurso. Isso zera o consumo elétrico em repouso e permite girar o manípulo manualmente a qualquer momento sem forçar as engrenagens.
2. **Acendimento Manual:** O usuário pressiona e gira o manípulo para acender o cooktop.
3. **Controle e Calibração:** Pelo web app, o usuário pode navegar pelos níveis calibrados (alto, médio, baixo) ou ajustar o slider.
4. **Fechamento Automático / Segurança:** Ao fechar pelo botão "Fechar agora" ou ao término do timer, o ESP32 acopla o servo, executa a rampa de fechamento até a posição calibrada como fechada e, em seguida, libera o servo novamente.

*Nota:* Os valores calibrados pela tela ficam gravados na memória permanente do ESP32 usando `Preferences`.

---

## 🛠️ Ajustes Importantes no Firmware

No arquivo `FogaoServoWeb.ino`, calibre estes valores para o seu mecanismo:

```cpp
static const bool INVERT_SERVO_DIRECTION = true;
static const int POS_FECHADO = 0;
static const int POS_FOGO_ALTO = 70;
static const int POS_FOGO_MEDIO = 115;
static const int POS_FOGO_BAIXO = 160;
```

Se quiser trocar a rede criada pelo ESP32:

```cpp
static const char *AP_SSID = "FOGAO-ESP32";
static const char *AP_PASSWORD = "12345678";
```

---

## 🌐 Rotas HTTP

- `GET /`: Interface web.
- `GET /status`: Retorna estado, timer, posição assumida e se o servo está anexado (JSON).
- `GET /fechar`: Fecha o manípulo e cancela timer.
- `GET /abrir`: Marca fogo alto sem mover o servo, para compatibilidade.
- `GET /setServo?pos=90`: Move manualmente o servo para calibração e cancela timer.
- `POST /timer?min=10&sec=30`: Inicia timer.
- `POST /setTimer?min=10&sec=30`: Alias de timer.
- `POST /goto?level=alto`: Move para o valor calibrado de fogo alto.
- `POST /goto?level=medio`: Move para o valor calibrado de fogo médio.
- `POST /goto?level=baixo`: Move para o valor calibrado de fogo baixo.
- `POST /calibrate?level=alto&pos=80`: Salva um ângulo calibrado na memória do ESP32.
- `POST /mark?level=fechado|alto|medio|baixo`: Sincroniza a posição assumida sem acionar o motor.
- `POST /release`: Libera/desanexa o servo para mover o manípulo manualmente.
- `POST /cancelTimer`: Cancela o timer sem fechar.

---

## 📖 Passo a Passo para o Uso

### 1. Preparar o Computador
1. Instale a Arduino IDE.
2. No gerenciador de placas, instale o core `esp32`.
3. No gerenciador de bibliotecas, instale `ESP32Servo`.
4. Abra o arquivo `FogaoServoWeb.ino` (ou `CódigoWeb.ino`).
5. Selecione a placa ESP32 correta e a porta USB correspondente.

### 2. Preparar a Montagem
1. Conecte o sinal do servo no GPIO26.
2. Alimente o servo com fonte estável de 5V-6V (2.5A+).
3. Ligue o GND da fonte do servo ao GND do ESP32.
4. Não acople o servo ao manípulo com força antes de testar os ângulos.
5. Faça os primeiros testes com o fogão desligado e, se possível, com o gás fechado.

### 3. Gravar e Conectar
1. Grave o firmware no ESP32 pela Arduino IDE.
2. Aguarde o ESP32 iniciar.
3. No celular ou computador, conecte-se ao Wi-Fi `FOGAO-ESP32` (senha `12345678`).
4. Abra o navegador no endereço `http://192.168.4.1`.

### 4. Calibrar na Bancada
1. Clique em `Liberar servo`.
2. Use o slider para mover o servo devagar.
3. Encontre o ângulo que deixa o manípulo fechado e clique em `Salvar fechado`.
4. Encontre o ângulo do fogo alto e clique em `Salvar alto`.
5. Encontre o ângulo do fogo médio e clique em `Salvar médio`.
6. Encontre o ângulo do fogo baixo e clique em `Salvar baixo`.
7. Teste os botões `Ir para alto`, `Ir para médio`, `Ir para baixo` e `Fechar agora`.

### 5. Inversão de Sentido de Giro
Se o servo girar no sentido contrário ao da válvula, altere em `FogaoServoWeb.ino`:
```cpp
static const bool INVERT_SERVO_DIRECTION = false; // ou true
```

---

## 📁 Estrutura de Arquivos Principais

- `MontagemGeralFinal.stl` — Montagem 3D completa de todo o conjunto.
- `BaseComManipulo.stl` — Subconjunto da base de comando com o manípulo do fogão.
- `Base996r.stl` — Suporte mecânico estrutural do servo MG996R.
- `Eng22DenteServo.stl` & `Eng25DentesBase.stl` — Par de engrenagens de transmissão (relação 22:25).
- `Caixa_ESP32_base.stl` & `Caixa_ESP32_tampa.stl` — Case protetor da eletrônica.
- `FogaoServoWeb.ino` / `CódigoWeb.ino` — Código de controle com interface Web responsiva.
- `DOSSIE_TECNICO_PROJETO.md` — Documentação técnica completa com todas as medidas, cálculos e esquemas elétricos.
