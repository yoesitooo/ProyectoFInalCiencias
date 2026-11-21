#ifndef MAPA_H
#define MAPA_H

#include <string>
#include "Enrutador.h"

// Abre la ventana con el mapa de Bogota (raylib). raylib solo dibuja y lee el mouse:
// todos los calculos los hace el Enrutador con las estructuras propias.
// Si 'captura' no esta vacio, guarda una imagen de la ventana con ese nombre y se cierra.
void ejecutarMapa(Enrutador& e, const std::string& carpetaDatos, const std::string& captura);

#endif
