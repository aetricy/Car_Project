# Disable PlatformIO automatic source scanning and adopt standard ESP-IDF structure
FILE(GLOB_RECURSE SOURCES "main.c" "main.cpp" "*.c" "*.cpp")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
)
