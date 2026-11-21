cmake_minimum_required(VERSION 3.16)
project(MotorRutasInterSedesUD CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

add_executable(enrutador
    src/main.cpp src/Grafo.cpp src/MinHeap.cpp src/TablaHash.cpp src/Utilidades.cpp
    src/Carga.cpp src/Enrutador.cpp src/Contingencias.cpp src/Escenario.cpp src/Horario.cpp src/Pruebas.cpp src/Mapa.cpp)

find_package(raylib REQUIRED)
target_link_libraries(enrutador PRIVATE raylib)

# El ejecutable busca la carpeta datos/ en ., .. y ../..
