# satsmeter
Medidor de energia (ESP32 + INA219) reporta consumo via MQTT; o backend converte mWh em sats e liquida do saldo do consumidor para o produtor. Saldo esgotou → carência de 3 leituras → relé corta o circuito. Recarregou acima do mínimo → religa sozinho. Tudo com garantia de que nada se perde e nada cobra em dobro, mesmo com quedas de rede.
