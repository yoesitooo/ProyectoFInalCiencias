#ifndef HORARIO_H
#define HORARIO_H

#include <string>
#include "Arreglo.h"
#include "Enrutador.h"

// ---------------------------------------------------------------------------
// Horario de clases (datos/horario.txt). Una clase por linea:
//     dia; inicio; fin; sede; materia
//     lunes; 07:00; 09:00; Sede Calle 40 Ingenieria; Ciencias de la Computacion I
//
// Para cada par de clases seguidas del mismo dia en sedes distintas se calcula el
// traslado: se sale al terminar la primera clase (con el trafico y las contingencias
// de esa hora) y se compara el tiempo de cada modo con el tiempo disponible.
// ---------------------------------------------------------------------------

struct Clase {
    int dia;              // 0 = lunes ... 5 = sabado
    int inicio;           // minutos desde la medianoche
    int fin;
    int sede;             // vertice de la sede en el grafo
    std::string materia;
};

struct Traslado {
    int claseOrigen;      // indices en el arreglo de clases
    int claseDestino;
    int disponible;       // minutos entre el fin de una clase y el inicio de la otra
    double minutos[NUM_MODOS];   // INFINITO si no hay ruta en ese modo
    int mejorModo;        // el modo mas rapido
};

const char* nombreDia(int dia);

// Lee el archivo y deja las clases ordenadas por dia y hora. Devuelve cuantas leyo (-1 si no existe).
int cargarHorario(const std::string& archivo, const Enrutador& e, Arreglo<Clase>& clases, std::string& informe);

// Calcula los traslados entre clases seguidas. No cambia la hora actual del enrutador.
Arreglo<Traslado> calcularTraslados(Enrutador& e, const Arreglo<Clase>& clases);

#endif
