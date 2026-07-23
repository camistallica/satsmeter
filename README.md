# SatsMeter

**Infraestrutura comunitária que cobra sozinha — em Bitcoin, sem banco, sem intermediário.**

Medidor de energia (ESP32 + INA219) reporta consumo via MQTT; o backend converte
mWh em sats e liquida do saldo do consumidor para o produtor. Saldo esgotou →
carência de 3 leituras → relé corta o circuito. Recarregou acima do mínimo →
religa sozinho. Tudo com garantia de que nada se perde e nada cobra em dobro
(tags + idempotência), mesmo com quedas de rede.


## Fiação

**INA219 (I2C + série com a carga):**
```
INA219 VCC  -> 3V3        INA219 SDA -> GPIO 21
INA219 GND  -> GND        INA219 SCL -> GPIO 22
Fonte 12V + -> VIN+ do INA219
VIN- do INA219 -> COM do relé
NO do relé  -> + da fita LED     (− da fita -> GND da fonte)
```

**Relé:** `GPIO 26 -> IN`, `VIN(5V) -> VCC`, `GND -> GND`.
Contato **NA**: relé sem comando = circuito aberto. Ajuste a regra
(NA vs NF) conforme a política de falha desejada — ver seção fail-safe.


## Subir o ambiente

```bash
docker compose up -d                 # Mosquitto + LNbits
cd backend && npm install
npm run dev      # dashboard: http://localhost:8000 · console: "dev01 200" / "extrato"
python3 tools/simulador.py           # desenvolve sem hardware
```

Firmware: escolha o modo no topo de `firmware/medidor/src/main.cpp`
(`MEDICAO_INA219` já é o padrão, com fallback automático se o sensor faltar),
configure Wi-Fi e IP do broker, e `pio run -t upload -t monitor`.


## Regras de negócio

- **Tarifa**: `SATS_POR_MWH` (padrão 0.5 — calibrado p/ demo com carga de ~1W)
- **Histerese**: corta só após `CARENCIA_LEITURAS` (3) no_funds consecutivos
- **Anti-flapping**: religa só com saldo ≥ `SALDO_MINIMO_RELIGA` (50 sats)
- **Fail-safe**: silêncio do medidor NUNCA causa corte; só saldo insuficiente
  confirmado. Queda de rede ≠ inadimplência.
- **Idempotência**: tag repetida devolve o ACK anterior sem nova cobrança
- **Extrato**: histórico auditável de débitos/créditos/cortes (`extrato` no console)


## Licença

MIT — open-source.