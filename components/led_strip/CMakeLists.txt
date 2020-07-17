set(component_srcs "src/led_strip_rmt_ws2812.c")
set(COMPONENT_PRIV_REQUIRES rmt)

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES "driver"
                       REQUIRES "")

