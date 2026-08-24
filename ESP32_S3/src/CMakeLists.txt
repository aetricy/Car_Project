# PlatformIO otomatik kaynak taramasını devre dışı bırakıp standart ESP-IDF yapısına geçiyoruz
FILE(GLOB_RECURSE SOURCES "main.c" "main.cpp" "*.c" "*.cpp")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
)
