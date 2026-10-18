# 📊 Diagramas do Projeto — FogãoServoWeb

---

## 1. Diagrama para Leigos — "Como funciona o sistema?"

```mermaid
flowchart TD
    subgraph USUARIO["👤 Usuário"]
        A["🔥 Acende o fogão\nmanualmente"]
        B["📱 Abre o celular / notebook\nConecta no Wi-Fi:\n'FOGAO-ESP32'"]
        C["🌐 Abre o navegador\nEndereço: 192.168.4.1"]
    end

    subgraph INTERFACE["🖥️ Interface Web no Celular"]
        D["⏱️ Define um Timer\nexemplo: 30 minutos"]
        E["🔆 Escolhe o nível da chama\nAlto / Médio / Baixo"]
        F["🚨 Aperta 'Fechar Agora'\nse precisar desligar na hora"]
    end

    subgraph ESP32["🧠 ESP32 — O Cérebro do Sistema"]
        G["Recebe o comando\npela rede Wi-Fi"]
        H["Faz a contagem\nregressiva do Timer"]
        I["Calcula o ângulo\ncorreto para o servo"]
    end

    subgraph SERVO["⚙️ Servo Motor — O Músculo"]
        J["Recebe o sinal\nde ângulo"]
        K["Gira e move\nas engrenagens"]
        L["Engrenagens giram\no manípulo do fogão"]
    end

    subgraph RESULTADO["✅ Resultado Final"]
        M["🔒 Fogão Fechado\nAutomaticamente"]
        N["📳 Alarme no celular\n+ pisca na tela"]
    end

    A --> B --> C
    C --> D & E & F
    D & E & F --> G
    G --> H --> I
    I --> J --> K --> L --> M
    M --> N
```

---

## 2. Diagrama de Blocos — "Arquitetura do Sistema"

```mermaid
block-beta
    columns 3

    block:ALIMENTACAO["⚡ Alimentação"]:1
        PB["Power Bank\n5V USB"]
        FONTE["Fonte Dedicada\n6V @ 3A"]
    end

    block:CONTROLE["🧠 Controle"]:1
        ESP["ESP32\n(Microcontrolador)"]
        WIFI["Wi-Fi SoftAP\nFOGAO-ESP32\n192.168.4.1"]
    end

    block:ATUACAO["⚙️ Atuação"]:1
        SERVO["Servo Motor\nGPIO 26 / PWM"]
        ENG["Engrenagens\n3D Impressas"]
        MANI["Manípulo\ndo Cooktop"]
    end

    PB --> ESP
    FONTE --> SERVO
    ESP --> WIFI
    ESP --> SERVO
    SERVO --> ENG --> MANI
```

---

## 3. Diagrama Elétrico — "Ligação dos Componentes"

```mermaid
graph LR
    subgraph PB["🔋 Power Bank (5V USB)"]
        USB["USB 5V OUT"]
    end

    subgraph ESP["🟦 ESP32 DevKit"]
        V5IN["5V PIN"]
        GND_ESP["GND"]
        GPIO26["GPIO 26\nPWM Servo"]
        GPIO0["3.3V / outros"]
    end

    subgraph PROTO["🟩 Protoboard 400pts"]
        RAIL_POS["Rail + (6V)"]
        RAIL_GND["Rail GND (Comum)"]
    end

    subgraph FONTE["⚡ Fonte 6V 3A"]
        F_POS["+ 6V"]
        F_GND["GND"]
    end

    subgraph SERVO["⚙️ Servo Motor"]
        S_VCC["VCC (Fio Vermelho)"]
        S_GND["GND (Fio Marrom/Preto)"]
        S_SIG["SINAL (Fio Laranja/Amarelo)"]
    end

    USB -->|"Cabo USB"| V5IN
    USB -->|"Cabo USB"| GND_ESP

    F_POS --> RAIL_POS
    F_GND --> RAIL_GND
    GND_ESP -->|"⚠️ Terra Comum\nObrigatório!"| RAIL_GND

    RAIL_POS -->|"6V"| S_VCC
    RAIL_GND --> S_GND
    GPIO26 -->|"PWM\n50Hz 500-2500µs"| S_SIG

    style RAIL_GND fill:#ff6b6b,color:#fff,stroke:#c0392b
    style GND_ESP fill:#ff6b6b,color:#fff,stroke:#c0392b
    style S_GND fill:#ff6b6b,color:#fff,stroke:#c0392b
    style RAIL_POS fill:#f39c12,color:#fff,stroke:#d68910
    style F_POS fill:#f39c12,color:#fff,stroke:#d68910
    style S_VCC fill:#f39c12,color:#fff,stroke:#d68910
    style GPIO26 fill:#27ae60,color:#fff,stroke:#1e8449
    style S_SIG fill:#27ae60,color:#fff,stroke:#1e8449
```

---

## 4. Diagrama de Sequência — "Uma sessão de cozimento completa"

```mermaid
sequenceDiagram
    actor Usuario as 👤 Usuário
    participant Celular as 📱 Navegador
    participant ESP32 as 🧠 ESP32
    participant Servo as ⚙️ Servo

    Note over Usuario,Servo: 🔥 Usuário acende o fogão manualmente (sem automação)

    Usuario->>Celular: Conecta no Wi-Fi FOGAO-ESP32
    Usuario->>Celular: Abre 192.168.4.1
    Celular->>ESP32: GET /status
    ESP32-->>Celular: {"nivel":"Fechado","servoAnexado":false}

    Usuario->>Celular: Define timer 20 min e toca Iniciar
    Celular->>ESP32: POST /timer?min=20&sec=0
    ESP32-->>Celular: {"timerAtivo":true,"tempoRestanteSeg":1200}

    loop A cada 500ms (polling automático)
        Celular->>ESP32: GET /status
        ESP32-->>Celular: Tempo restante atualizado
    end

    Note over ESP32: ⏰ Timer expira!

    ESP32->>Servo: Attach + Rampa de fechamento (900ms)
    Servo-->>ESP32: Manípulo girado para 0° (Fechado)
    ESP32->>Celular: timerDoneSeq incrementado
    Celular-->>Usuario: 🔔 Alarme sonoro + pisca na tela

    ESP32->>Servo: Detach (Servo Livre / Standby)
    Note over Servo: Sem força, sem consumo desnecessário
```

---

## 5. Resumo Visual dos Estados do Sistema

```mermaid
stateDiagram-v2
    [*] --> Standby : Liga o ESP32

    Standby : 💤 Standby\nServo Liberado\nSem Torque

    Standby --> Calibrando : Slider ou\nBotão Salvar

    Calibrando : 🔧 Calibração\nServo Ativo\nSlider 0-180°

    Calibrando --> Standby : Valor Salvo\nna Memória NVS

    Standby --> ControleCozimento : Usuário\nInicia Timer ou\nEscolhe Nível

    ControleCozimento : 🍳 Cozimento\nTimer Ativo\nNível Definido

    ControleCozimento --> Fechando : Timer = 0\nou "Fechar Agora"

    Fechando : 🔒 Fechando\nServo Engaja\nRampa 900ms

    Fechando --> Standby : Servo Chegou\nna Posição Fechado\n+ Alarme Disparado

    ControleCozimento --> Standby : Cancelar Timer
    Standby --> Standby : "Liberar Servo"\n(já está livre)
```

---

> [!NOTE]
> **Próximos refinamentos previstos:** adicionar o conjunto de engrenagens 3D (pinhão + coroa) no diagrama mecânico e o diagrama de montagem física no cooktop após definição dos parâmetros das engrenagens.
