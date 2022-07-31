idf_component_register(SRCS "retrostore_test_main.cpp"
                            "wifi.cpp"

                       REQUIRES main
                                nvs_flash
                                retrostore-c-sdk)
