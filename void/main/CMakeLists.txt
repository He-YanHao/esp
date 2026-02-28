idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES esp_system esp_pm spi_flash nvs_flash
                    INCLUDE_DIRS ".")
