# Motor de Rutas Inter-Sedes - Universidad Distrital (C++17 + raylib)
#   make            compila
#   make pruebas    compila y ejecuta las pruebas de las estructuras
#   make memoria    compila con AddressSanitizer (Linux/WSL) y corre las pruebas
#   make datos      regenera datos/ desde OpenStreetMap (requiere Python)
# Si raylib no esta en las rutas del compilador: make RAYLIB_DIR=/ruta/a/raylib

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
FUENTES  = src/main.cpp src/Grafo.cpp src/MinHeap.cpp src/TablaHash.cpp src/Utilidades.cpp \
           src/Carga.cpp src/Enrutador.cpp src/Contingencias.cpp src/Escenario.cpp src/Horario.cpp src/Pruebas.cpp src/Mapa.cpp

ifdef RAYLIB_DIR
    RAYLIB = -I$(RAYLIB_DIR)/include -L$(RAYLIB_DIR)/lib
endif

ifeq ($(OS),Windows_NT)
    EXE  = enrutador.exe
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
else
    EXE  = enrutador
    LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

.PHONY: all pruebas memoria datos clean

all:
	$(CXX) $(CXXFLAGS) $(FUENTES) $(RAYLIB) -o $(EXE) $(LIBS)

pruebas: all
	./$(EXE) --pruebas

memoria:
	$(CXX) -std=c++17 -g -fsanitize=address,undefined $(FUENTES) $(RAYLIB) -o $(EXE) $(LIBS)
	./$(EXE) --pruebas

datos:
	python3 herramientas/preparar_datos.py

clean:
	rm -f $(EXE)
