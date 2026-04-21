cmake_minimum_required(VERSION 4.0)
project(proyectoLyrics)

set(CMAKE_CXX_STANDARD 20)

# 1. Buscamos el paquete de CURL.
# vcpkg se encarga de que find_package lo encuentre gracias al Toolchain.
find_package(CURL REQUIRED)

add_executable(proyectoLyrics main.cpp)

# 2. Vinculamos la librería al ejecutable.
# Usamos CURL::libcurl que es el nombre oficial del "target" en CMake.
target_link_libraries(proyectoLyrics PRIVATE CURL::libcurl)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(proyectoLyrics PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)