idf_component_register( SRCS 
                            "main.c"
                        INCLUDE_DIRS
                            "."
                            "../components/factory_ble_prov/include"
                            "../../clock/components/coredump_to_server/include"
                            "../../clock/components/factory_reset_task/include"
                            "../../clock/components/ota_update_task/include"
                            "../../clock/components/wifi_connect/include"
)