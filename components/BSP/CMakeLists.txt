set(src_dirs
            HTML
            SERVER
            WIFISTA
            LED
            MYIIC
            XL9555
            MYSPI
            SPILCD)

set(include_dirs
            HTML
            SERVER
            WIFISTA
            LED
            MYIIC
            XL9555
            MYSPI
            SPILCD)

set(requires
            esp_http_server
            esp_wifi
            driver
            esp_lcd
            spiffs
            json)

# 敏感配置保存在 WIFISTA/wifi_secrets.h（已被 .gitignore 忽略），
# 首次编译时若不存在，则从 wifi_secrets.h.example 自动生成一份模板。
# 注意：本文件在 ESP-IDF 需求扫描阶段也会被 include，那时 CMAKE_CURRENT_SOURCE_DIR 为空，
# 因此这里用 CMAKE_CURRENT_LIST_DIR 定位路径。
set(_secrets "${CMAKE_CURRENT_LIST_DIR}/WIFISTA/wifi_secrets.h")
if(NOT EXISTS "${_secrets}")
    message(STATUS "wifi_secrets.h not found, generating from wifi_secrets.h.example")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/WIFISTA/wifi_secrets.h.example"
                   "${_secrets}" COPYONLY)
endif()

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)