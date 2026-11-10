# 📖 Dossiê Técnico Atualizado (V2.0): Projeto FogaoServoWeb / Cooktop IoT (MG996R)

Este documento reflete a **nova arquitetura mecânica, cinemática, cálculos e especificações eletrônicas** implementadas após a transição para o servo motor de alto torque **MG996R** e o novo sistema de transmissão por engrenagens externas.

---

## 1. Nova Arquitetura do Sistema e Funcionamento

### 1.1 O que mudou em relação à versão anterior (SG90)
- **Substituição do Servo Motor:** O micro servo SG90 (torque fraco de ~1.8 kgf.cm e engrenagens plásticas) foi substituído pelo **MG996R** (engrenagens metálicas, torque de **9.4 a 13.0 kgf.cm**).
- **Novo Esquema de Giro (Engrenagens Externas Paralelas):**
  - **Antes:** O servo ficava dentro do manípulo girando em órbita sobre uma cremalheira circular fixa no vidro (engrenamento planetário interno que sofria com desalinhamento e perda de contato).
  - **Agora:** O servo **MG996R fica fixo** em seu suporte na base do cooktop (`Base996r.stl` / `MontagemGeralFinal.stl`).
  - O servo aciona diretamente a **engrenagem motora** (`Eng22DenteServo.stl` de 22 dentes).
  - Essa engrenagem aciona a **engrenagem movida da base do manípulo** (`Eng25DentesBase.stl` de 25 dentes com furo em D).
  - A engrenagem base gira o eixo metálico em D da válvula de gás do cooktop.
  - A base do manípulo agora é móvel e acoplada ao eixo metálico, permitindo tanto o giro automatizado quanto o giro manual.

```
                  ┌──────────────────────────────┐
                  │   SERVO MG996R (FIXO)        │
                  │   Torque: ~11-13 kgf.cm      │
                  └──────────────┬───────────────┘
                                 │ Eixo estriado 25T
                                 ▼
                  ┌──────────────────────────────┐
                  │ Eng22DenteServo (Z=22)       │  <-- Engrenagem Motora (Pinhão)
                  │ Diâmetro Externo: ~34.5 mm   │
                  └──────────────┬───────────────┘
                                 │
                     Engrenamento Externo Paralelo
                     (Distância entre Centros: ~34 mm)
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │ Eng25DentesBase (Z=25)       │  <-- Engrenagem Movida (Base)
                  │ Diâmetro Externo: ~36.5 mm   │
                  │ Encaixe D-Shaft no centro    │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │ EIXO EM 'D' DA VÁLVULA       │  <-- Válvula de Gás do Cooktop
                  │ + Manípulo Superior (Manual) │      Giro seguro: Alto <-> Baixo <-> Fechado
                  └──────────────────────────────┘
```

---

## 2. Parâmetros Cinemáticos e Cálculos Mecânicos

### 2.1 Relação de Transmissão ($i$)
- Número de dentes da engrenagem do servo ($Z_1$): **22 dentes**
- Número de dentes da engrenagem da base/válvula ($Z_2$): **25 dentes**
- **Relação de Transmissão:**
  $$i = \frac{Z_2}{Z_1} = \frac{25}{22} \approx 1,136$$
- **Multiplicação de Torque:** O torque entregue na válvula do fogão é aumentado em $\approx 13,6\%$, garantindo facilidade para vencer a rigidez da mola da válvula.
- **Relação de Ângulo:** Para a válvula girar um ângulo $\theta_{valvula}$, o servo precisa girar:
  $$\theta_{servo} = i \times \theta_{valvula} = 1,136 \times \theta_{valvula}$$
  - Para curso de 90° na válvula: Servo gira $\approx 102,3^\circ$
  - Para curso de 180° na válvula: Servo gira $\approx 204,5^\circ$

### 2.2 Medições Exatas dos STLs Atuais
1. **`Eng22DenteServo.stl`:**
   - Dentes: $22$
   - Raio da Raiz ($r_{root}$): $\approx 15,40\text{ mm}$ ($\varnothing\ 30,8\text{ mm}$)
   - Raio Primitivo ($r_{pitch}$): $\approx 16,3\text{ mm}$ ($\varnothing\ 32,6\text{ mm}$)
   - Raio da Ponta ($r_{tip}$): $\approx 17,26\text{ mm}$ ($\varnothing\ 34,53\text{ mm}$)
   - Altura total dos dentes: $8,15\text{ mm}$

2. **`Eng25DentesBase.stl`:**
   - Dentes: $25$
   - Raio Primitivo ($r_{pitch}$): $\approx 17,7\text{ mm}$ ($\varnothing\ 35,4\text{ mm}$)
   - Raio da Ponta ($r_{tip}$): $\approx 18,28\text{ mm}$ ($\varnothing\ 36,56\text{ mm}$)
   - Encaixe central: Furo com chanfro D-Shaft do fogão
   - Altura total: $17,97\text{ mm}$

3. **Distância entre Centros Nominal ($C$):**
   $$C = r_{pitch1} + r_{pitch2} \approx 16,3\text{ mm} + 17,7\text{ mm} \approx \mathbf{34,0\text{ mm}}$$

---

## 3. Lógica de Segurança e Operação

### 3.1 Operação Segura do Cooktop (Apresentação / TCC)
- **Fechamento Automático:** O sistema pode fechar o gás a qualquer momento com total segurança e força imediata.
- **Transição Alto $\leftrightarrow$ Baixo:** O servo atua suavemente entre as posições de chama alta e chama baixa.
- **Partida do Zero (Segurança):** Como abrir o gás a partir da posição "Fechado" exige apertar a válvula para baixo para liberar a trava de segurança e acionar a faísca (ou fósforo), **o sistema foi configurado para não realizar a abertura do zero via app**, evitando vazamento acidental de gás sem chama.
- **Uso Manual / Híbrido:** O usuário pode ligar o fogão manualmente girando o manípulo. O usuário pode indicar no app a posição atual (ou sensores de fim de curso/posição informam o app), e a partir daí o app assume o controle entre fogo alto, fogo baixo e desligamento programado (timer).

---

## 4. Requisitos Elétricos do MG996R

> [!WARNING]
> O servo MG996R consome picos de corrente entre **1.5A a 2.5A sob carga máxima** em 5V-6V. Ele **nunca** deve ser alimentado diretamente pelos pinos de 5V ou 3.3V do ESP8266/ESP32 ou pela porta USB do computador.

- **Fonte de Alimentação Recomendada:** Fonte DC 5V/6V com capacidade mínima de **2.5A a 3A**.
- **Conexão GND:** GND da fonte do servo obrigatoriamente interligado ao GND do ESP32/ESP8266.
- **Capacitor de Filtro:** Recomenda-se um capacitor eletrolítico de $1000\,\mu\text{F} \times 16\text{V}$ em paralelo com a alimentação do servo para absorver transientes.

---

## 5. Arquivos STL Principais da Nova Montagem

- `MontagemGeralFinal.stl` — Montagem completa do conjunto (suporte, servo MG996R, engrenagens e manípulo).
- `BaseComManipulo.stl` — Conjunto integrado da base do manípulo acoplada ao eixo de comando.
- `Eng22DenteServo.stl` — Pinhão de 22 dentes montado no servo MG996R.
- `Eng25DentesBase.stl` — Engrenagem de 25 dentes com acoplamento D-shaft.
- `Base996r.stl` — Suporte e berço do servo motor MG996R.
- `Caixa_ESP32_base.stl` e `Caixa_ESP32_tampa.stl` — Case de proteção para o microcontrolador e cabos.
