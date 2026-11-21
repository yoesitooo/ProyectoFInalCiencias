#ifndef UTILIDADES_H
#define UTILIDADES_H

#include <string>
#include "Arreglo.h"
#include "Grafo.h"

// Divide "a;b;c" en sus campos (acepta campos vacios)
Arreglo<std::string> separar(const std::string& linea, char separador);

// Quita espacios al inicio y al final
std::string recortar(const std::string& texto);

// Convierte texto a numero. Devuelve false si el texto no es un numero valido.
bool aNumero(const std::string& texto, double& resultado);

// "07:30" -> 450 (minutos desde la medianoche). Acepta tambien "7" o "7:5". false si no es una hora valida.
bool leerHora(const std::string& texto, int& minuto);

// "10:00-13:00" -> desde = 600, hasta = 780
bool leerFranja(const std::string& texto, int& desde, int& hasta);

// true si la linea esta vacia o es un comentario (#)
bool esComentario(const std::string& linea);

// Distancias en pixeles del mapa (1 pixel = METROS_POR_PIXEL metros)
double distancia(Punto a, Punto b);
double distanciaASegmento(Punto p, Punto a, Punto b);

#endif
