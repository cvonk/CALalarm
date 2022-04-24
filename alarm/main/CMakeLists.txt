idf_component_register( SRCS
                            "main.c"
                           
                            "display_task.c"
                            "buzzer_task.c"
                            "httpd/httpd.c"
                            "httpd/httpd_google_push.c"
                            "http/https_client_task.c"
                        INCLUDE_DIRS
                            "."
                            "../components/factory_reset_task/include"
                            "../components/ota_update_task/include"
                            "../components/wifi_connect/include"
                            "../components/ssd1306/include"
)
