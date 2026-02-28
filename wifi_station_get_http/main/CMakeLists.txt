idf_component_register(SRCS "main.c"
                    # 这里的的组件依赖不会传递
                    PRIV_REQUIRES esp_system esp_pm spi_flash esp_wifi nvs_flash station
                    # 这里的的组件依赖会传递
                    REQUIRES
                    INCLUDE_DIRS ".")