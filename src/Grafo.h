#ifndef GRAFO_H
#define GRAFO_H

#include <string>
#include "Arreglo.h"
#include "TablaHash.h"

// ---------------------------------------------------------------------------
// Grafo dirigido ponderado con listas de adyacencia.
//   - Vertice: un cruce entre vias principales, una sede o una estacion de TransMilenio.
//   - Arista:  un tramo de calle (o de TransMilenio) entre dos vertices. Un tramo de
//              doble sentido se guarda como dos aristas.
// Las aristas de cada vertice forman una lista enlazada (nodos creados con new y
// liberados en el destructor). Espacio O(V + E); recorrer los vecinos de v: O(grado(v)).
// ---------------------------------------------------------------------------

enum TipoVia {
    AUTOPISTA, TRONCAL, PRINCIPAL, SECUNDARIA, TERCIARIA, PEATONAL, CICLORRUTA, CONECTOR,
    ACCESO_SEDE,   // union entre una sede y la via principal mas cercana
    TM_BUS,        // trayecto en un servicio de TransMilenio entre dos estaciones vecinas
    TM_ENTRADA,    // caminar desde la calle hasta la estacion
    TM_SALIDA,     // salir de la estacion a la calle
    TM_ABORDAR,    // subirse a un servicio en una estacion (cuesta la espera del bus)
    TM_BAJAR,      // bajarse del servicio en una estacion
    NUM_TIPOS_VIA
};

enum TipoVertice { CRUCE, SEDE, ESTACION_TM };

enum Contingencia { NINGUNA, MARCHA, BLOQUEO };

// Coordenada en pixeles sobre la imagen datos/mapa.png
struct Punto {
    float x;
    float y;
};

struct Arista {
    int destino;
    double metros;
    TipoVia tipo;
    bool permiteCarro;      // carro y moto
    bool permiteBici;
    bool permitePie;
    bool esTransMilenio;
    bool ciclorruta;        // la via tiene ciclorruta o carril bici
    int via;                // indice del nombre de la via (-1 = sin nombre)
    int inicioTrazado;      // primer punto de la forma de la calle en Grafo::puntos
    int puntosTrazado;      // cuantos puntos tiene esa forma

    // Estado: lo modifican las contingencias
    bool cerrada;
    Contingencia contingencia;
    double factorTrafico;   // 1 = normal, 3 = la via esta congestionada (triple de tiempo)
    double penalizacion;    // > 1 solo mientras se buscan rutas alternativas

    Arista* siguiente;      // siguiente arista en la lista de adyacencia
};

struct Vertice {
    std::string nombre;     // vacio en los cruces
    Punto pos;
    TipoVertice tipo;
    bool activo;            // false = estacion o sede cerrada
    bool nucleoCarro;       // el cruce esta en la parte bien conectada de la red (ver nodos.csv)
    bool nucleoBici;
    bool nucleoPie;
    Arista* primera;        // cabeza de la lista de adyacencia
};

class Grafo {
public:
    Grafo();
    ~Grafo();

    int agregarVertice(const std::string& nombre, Punto pos, TipoVertice tipo);

    // Arista con valores por defecto (sin permisos, sin nombre, sin contingencia)
    static Arista crearArista(int destino, double metros, TipoVia tipo);
    // Copia 'datos' en un nodo nuevo al inicio de la lista de 'origen': O(1)
    Arista* agregarArista(int origen, const Arista& datos);
    Arista* buscarArista(int origen, int destino) const;
    bool eliminarArista(int origen, int destino);

    // Guarda la forma de una calle y devuelve la posicion de su primer punto
    int agregarTrazado(const Arreglo<Punto>& trazado);
    Punto punto(int i) const;

    // Nombres de vias sin repetir ("Avenida Caracas" -> 12) con una tabla hash
    int indiceVia(const std::string& nombre);          // lo agrega si es nuevo
    int buscarVia(const std::string& nombre) const;    // -1 si no existe
    const std::string& nombreVia(int indice) const;

    // Quita cierres, congestion y contingencias de todas las aristas y reabre los vertices
    void restaurar();

    int numVertices() const;
    int numAristas() const;
    Vertice& vertice(int i);
    const Vertice& vertice(int i) const;

private:
    Arreglo<Vertice> vertices;
    Arreglo<Punto> puntos;
    Arreglo<std::string> nombresVia;
    TablaHash indiceNombresVia;
    int totalAristas;

    Grafo(const Grafo&);
    Grafo& operator=(const Grafo&);
};

#endif
