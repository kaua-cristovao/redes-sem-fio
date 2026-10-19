cmake_minimum_required(VERSION 3.10.0)
project(redes-sem-fio VERSION 0.1.0 LANGUAGES C CXX)

add_executable(redes-sem-fio main.cpp)

set_target_properties(redes-sem-fio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set_target_properties(redes-sem-fio PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_SOURCE_DIR}"
    )
endforeach()
