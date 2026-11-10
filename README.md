# Projeto Cooktop Inteligente IoT (FogaoServoWeb)

Sistema de automação e controle remoto seguro para cooktops a gás, desenvolvido com microcontrolador ESP (ESP8266 / ESP32) e servomotor de alto torque **MG996R**.

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
  - O aplicativo gerencia com precisão o controle entre **Chama Alta**, **Chama Baixa** e **Desligamento/Fechamento Automático**.
  - Por segurança contra liberação indevida de gás, o acendimento inicial a partir do fechado é feito presencialmente pelo usuário.

---

## 📁 Estrutura de Arquivos Principais

- `MontagemGeralFinal.stl` — Montagem 3D completa de todo o conjunto.
- `BaseComManipulo.stl` — Subconjunto da base de comando com o manípulo do fogão.
- `Base996r.stl` — Suporte mecânico estrutural do servo MG996R.
- `Eng22DenteServo.stl` & `Eng25DentesBase.stl` — Par de engrenagens de transmissão.
- `Caixa_ESP32_base.stl` & `Caixa_ESP32_tampa.stl` — Case protetor da eletrônica.
- `FogaoServoWeb.ino` — Código de controle com interface Web responsiva.
- `DOSSIE_TECNICO_PROJETO.md` — Documentação técnica completa com todas as medidas, cálculos e esquemas elétricos.
