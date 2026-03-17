idf_component_register(
    SRCS "drv_gpio_switch.c" "drv_neopixel.c"
    INCLUDE_DIRS "include"
    REQUIRES driver led_strip)
