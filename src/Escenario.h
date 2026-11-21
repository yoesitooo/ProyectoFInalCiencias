#ifndef ESCENARIO_H
#define ESCENARIO_H

#include <string>
#include "Enrutador.h"

// ---------------------------------------------------------------------------
// Archivo de escenario (datos/escenario.txt): texto plano, una orden por linea,
// campos separados por ';'. Se puede editar con el programa abierto.
//
//   hora;            HH:MM                         (hora con la que arranca la simulacion)
//   marcha;          <lugar>; <radio en metros> [; franja]
//   bloqueo;         <lugar>; <radio en metros> [; franja]
//   cerrar_via;      <nombre de la via> [; franja]
//   congestion;      <nombre de la via>; <factor> [; franja]   (3 = el triple de tiempo en carro/moto)
//   cerrar_estacion; <nombre de la estacion o sede> [; franja]
//
// <lugar> es el nombre de una sede o estacion, o coordenadas del mapa "x,y".
// [franja] es opcional, p. ej. "10:00-13:00": el evento solo existe en esas horas.
// ---------------------------------------------------------------------------

// Lee el archivo, reemplaza las contingencias que venian del archivo por las nuevas y
// recalcula el estado de la red. Devuelve cuantas ordenes se aplicaron (-1 si no existe).
// En 'informe' deja un resumen y los errores encontrados (linea por linea).
int cargarEscenario(const std::string& archivo, Enrutador& e, std::string& informe);

// Fecha de la ultima modificacion del archivo (para saber si hay que recargarlo). -1 si no existe.
long long fechaModificacion(const std::string& archivo);

#endif
