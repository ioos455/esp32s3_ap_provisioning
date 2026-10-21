idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

# 将html网页挂载到spiffs目录下
spiffs_create_partition_image(html ../components/BSP/HTML FLASH_IN_PROJECT)