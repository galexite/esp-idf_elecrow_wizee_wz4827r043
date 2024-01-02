# Elecrow Wizee-ESP32 WZ4827R043

An ESP-IDF demo project for the Wizee ESP32-S3 board with a 4.3" TFT display and
resistive touchscreen. Unlike the ESP-IDF/PlatformIO examples that exist, this
uses ESP-IDF directly, rather than through Arduino as a component.

This board uses an NV3047 TFT controller over an RGB565 parallel interface,
which does not have its own GRAM. The displayed framebuffer is held in PSRAM.

This project is based on the `examples/peripherals/lcd/rgb_panel` example from
ESP-IDF 5.1.2. It is available under the public domain via the CC0-1.0 deed,
as was the original example. Please see `LICENSE` files in imported components
for their individual licensing conditions.
