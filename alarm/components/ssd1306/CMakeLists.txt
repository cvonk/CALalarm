idf_component_register(SRCS "src/ssd1306.c" "src/ssd1306_i2c.c" "src/ssd1306_spi.c" "src/font8x8_basic.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver
)