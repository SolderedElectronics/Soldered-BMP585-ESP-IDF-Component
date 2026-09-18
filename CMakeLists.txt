idf_component_register(SRCS "src/soldered_bmp585.c"
                            "src/bmp5_api/bmp5.c"
                    INCLUDE_DIRS "include" "include/bmp5_api"
                    REQUIRES esp_driver_i2c)
