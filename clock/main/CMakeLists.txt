
idf_component_register( SRCS
                            main.c
                            mqtt_task.c
                            display_task.c
                            httpd/httpd.c
                            httpd/httpd_google_push.c
                            http/https_client_task.c
                        INCLUDE_DIRS
                            .
                            ../../components/coredump_to_server/include
                            ../../components/factory_reset_task/include
                            ../../components/ota_update_task/include
                            ../../components/wifi_connect/include
                            $ENV{IDF_PATH}/examples/common_components/led_strip/include
                        PRIV_REQUIRES
                            esp_http_server 
                            esp_http_client
                            esp_https_ota
                            nvs_flash
                            app_update 
                            json
                            mqtt
                            mdns
                            espcoredump)
