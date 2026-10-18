# Fogao Servo Web

Firmware Arduino para ESP32 controlar 1 servo responsavel pelo manipulo do fogao por uma interface web local.

## Recursos

- ESP32 em modo Access Point com SSID `FOGAO-ESP32`.
- Pagina web responsiva em portugues servida em `http://192.168.4.1`.
- Fluxo pensado para abertura manual e fechamento automatico.
- Calibracao pela propria tela: ajuste o slider e salve o angulo como fechado, alto, medio ou baixo.
- Botoes para ir para os valores calibrados: fogo alto, medio ou baixo.
- Timer com minutos e segundos que fecha automaticamente quando expira.
- Animacao visual do slider durante o fechamento automatico ou pelo botao.
- Slider manual de 0 a 180 graus para calibracao.
- Rota `/status` em JSON para atualizacao dinamica.

## Bibliotecas

Instale pela Arduino IDE:

- `ESP32Servo`
- Core de placas `esp32` para Arduino

As bibliotecas `WiFi.h` e `WebServer.h` ja fazem parte do core ESP32.

## Ligacao sugerida

- Sinal do servo (fio laranja/amarelo): GPIO26 do ESP32.
- VCC do servo (fio vermelho): fonte 6V estavel (ex: 6V @ 3A para torque maximo do MG90S).
- GND do servo (fio marrom/preto): GND da fonte 6V e GND do ESP32 interligados (GND comum).

Use fonte externa dedicada com corrente suficiente para o servo. Teste os angulos com o fogao desligado antes de acoplar ao manipulo.

## Sobre o funcionamento e modo Standby (Auto-Release)

Um servo comum de hobby nao envia a posicao real de volta para o ESP32. O firmware gerencia o movimento de forma inteligente e segura:

1. **Auto-Release Transparente:** Apos QUALQUER comando de movimento (seja botoes de nivel, slider ou fechamento automatico por timer), o servo se move de forma suave e **e liberado automaticamente (`releaseServo() / detach`)** logo ao final do percurso. Isso zera o consumo eletrico em repouso e permite girar o manipulo manualmente a qualquer momento sem forcar as engrenagens.
2. **Acendimento Manual:** O usuario pressiona e gira o manipulo para acender o cooktop.
3. **Controle e Calibracao:** Pelo web app, o usuario pode navegar pelos niveis calibrados (alto, medio, baixo) ou ajustar o slider.
4. **Fechamento Automatico / Seguranca:** Ao fechar pelo botao "Fechar agora" ou ao término do timer, o ESP32 acopla o servo, executa a rampa de fechamento ate a posicao de 0° (calibrada como fechada) e, em seguida, libera o servo novamente.

Para leitura real seria necessario adicionar um sensor, por exemplo potenciometro acoplado ao eixo, encoder ou fim de curso.

Importante: os valores salvos pela tela ficam gravados na memoria permanente do ESP32 usando `Preferences`. No `preview.html`, os valores ficam salvos no navegador usando `localStorage`.

## Ajustes importantes

No arquivo `FogaoServoWeb.ino`, calibre estes valores para o seu mecanismo:

```cpp
static const bool INVERT_SERVO_DIRECTION = true;
static const int POS_FECHADO = 0;
static const int POS_FOGO_ALTO = 70;
static const int POS_FOGO_MEDIO = 115;
static const int POS_FOGO_BAIXO = 160;
```

Se quiser trocar a rede criada pelo ESP32, altere:

```cpp
static const char *AP_SSID = "FOGAO-ESP32";
static const char *AP_PASSWORD = "12345678";
```

## Rotas HTTP

- `GET /`: interface web.
- `GET /status`: retorna estado, timer, posicao assumida e se o servo esta anexado.
- `GET /fechar`: fecha o manipulo e cancela timer.
- `GET /abrir`: marca fogo alto sem mover o servo, para compatibilidade.
- `GET /setServo?pos=90`: move manualmente o servo para calibracao e cancela timer.
- `POST /timer?min=10&sec=30`: inicia timer.
- `POST /setTimer?min=10&sec=30`: alias de timer.
- `POST /goto?level=alto`: move para o valor calibrado de fogo alto.
- `POST /goto?level=medio`: move para o valor calibrado de fogo medio.
- `POST /goto?level=baixo`: move para o valor calibrado de fogo baixo.
- `POST /calibrate?level=alto&pos=80`: salva um angulo calibrado na memoria do ESP32.
- `POST /mark?level=fechado|alto|medio|baixo`: sincroniza a posicao assumida sem acionar o motor.
- `POST /release`: libera/desanexa o servo para mover o manipulo manualmente.
- `POST /cancelTimer`: cancela o timer sem fechar.

## Como gravar

1. Abra `FogaoServoWeb/FogaoServoWeb.ino` na Arduino IDE.
2. Selecione a placa ESP32 correta.
3. Instale `ESP32Servo`.
4. Grave no ESP32.
5. Conecte o celular ou computador ao Wi-Fi `FOGAO-ESP32`.
6. Acesse `http://192.168.4.1`.

## Passo a passo para a apresentacao

### 1. Preparar o computador

1. Instale a Arduino IDE.
2. No gerenciador de placas, instale o core `esp32`.
3. No gerenciador de bibliotecas, instale `ESP32Servo`.
4. Abra o arquivo `FogaoServoWeb/FogaoServoWeb.ino`.
5. Selecione a placa ESP32 correta e a porta USB correta.

### 2. Preparar a montagem

1. Conecte o sinal do servo no GPIO26.
2. Alimente o servo com 5V estavel.
3. Ligue o GND da fonte do servo ao GND do ESP32.
4. Nao acople o servo ao manipulo com forca antes de testar os angulos.
5. Faca os primeiros testes com o fogao desligado e, se possivel, com o gas fechado.

### 3. Gravar e conectar

1. Grave o firmware no ESP32 pela Arduino IDE.
2. Aguarde o ESP32 iniciar.
3. No celular ou notebook, conecte-se ao Wi-Fi `FOGAO-ESP32`.
4. Use a senha `12345678`.
5. Abra o navegador em `http://192.168.4.1`.

### 4. Calibrar na bancada

1. Clique em `Liberar servo`.
2. Use o slider para mover o servo devagar.
3. Encontre o angulo que deixa o manipulo fechado e clique em `Salvar fechado`.
4. Encontre o angulo do fogo alto e clique em `Salvar alto`.
5. Encontre o angulo do fogo medio e clique em `Salvar medio`.
6. Encontre o angulo do fogo baixo e clique em `Salvar baixo`.
7. Teste os botoes `Ir para alto`, `Ir para medio`, `Ir para baixo` e `Fechar agora`.

Os valores salvos ficam gravados no ESP32. Se outro celular ou computador acessar o ESP32 depois, ele deve usar a mesma calibracao.

### 5. Testar o timer

1. Defina poucos segundos, por exemplo `0` minutos e `10` segundos.
2. Clique em `Iniciar timer`.
3. Aguarde o fim da contagem.
4. Confira se o servo fecha o manipulo ate o ponto salvo em `Salvar fechado`.

### 6. Se girar para o lado errado

No arquivo `FogaoServoWeb.ino`, altere:

```cpp
static const bool INVERT_SERVO_DIRECTION = true;
```

para:

```cpp
static const bool INVERT_SERVO_DIRECTION = false;
```

Depois grave novamente e refaca a calibracao.

