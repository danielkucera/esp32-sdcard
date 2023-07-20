Import("env")

env.Execute("~/.platformio/packages/tool-wizio-pico/linux_x86_64/pioasm src/sd_rx.pio include/sd_rx.pio.h")