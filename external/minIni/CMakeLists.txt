add_library(minIni STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/minIni/dev/minIni.c
)

target_include_directories(minIni
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/minIni/dev
)