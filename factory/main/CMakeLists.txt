idf_component_register( SRCS 
                            "main.c"
                        INCLUDE_DIRS
                            "."
                            "../components/factory_ble_prov/include"
                            "../../alarm/components/coredump_to_server/include"
                            "../../alarm/components/factory_reset_task/include"
                            "../../alarm/components/ota_update_task/include"
                            "../../alarm/components/wifi_connect/include"
)