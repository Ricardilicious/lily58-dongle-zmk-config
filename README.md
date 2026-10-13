# Lily58 Pro + nice!nano dongle (ZMK)

- **Dongle** (3rd nice!nano + 1.3" SH1106 OLED) = central, runs the keymap
- **Left / right halves** = peripherals

Firmware is built by GitHub Actions — download from the Actions tab.

Flash order: `settings_reset` on all 3 → dongle → left → right.

OLED wiring (dongle): GND→GND, VCC→VCC, SDA→D2 (P0.17), SCL→D3 (P0.20).

Edit keys in `config/lily58_dongle.keymap`.
