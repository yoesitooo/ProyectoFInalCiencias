#ifndef ENRUTADOR_H
#define ENRUTADOR_H

#include <string>
#include "Arreglo.h"
#include "Grafo.h"
#include "MinHeap.h"
#include "TablaHash.h"

const double INFINITO = 1e18;
const double METROS_POR_PIXEL = 4.76;   // escala de datos/mapa.png (zoom 15 a la latitud de Bogota)
const int MAX_RUTAS = 3;                // ruta optima + 2 alternativas

enum Modo { MODO_CARRO, MODO_MOTO, MODO_BICI, MODO_PIE, MODO_TM, NUM_MODOS };

struct Paso {
    int desde;
    int hasta;
    Arista* arista;
    double minutos;
};

struct Ruta {
    bool encontrada;
    Modo modo;
    int origen;
    int destino;
    double minutos;
    double metros;
    Arreglo<Paso> pasos;

    Ruta() : encontrada(false), modo(MODO_CARRO), origen(-1), destino(-1), minutos(0), metros(0) {}
};

struct Indicacion {
    std::string texto;
    double metros;
    double minutos;
    TipoVia tipo;
};

// Contingencia reportada en datos/escenario.txt o con un clic en el mapa
enum TipoEvento { EV_MARCHA, EV_BLOQUEO, EV_CERRAR_VIA, EV_CONGESTION, EV_CERRAR_LUGAR };

struct Evento {
    TipoEvento tipo;
    Punto centro;          // marcha y bloqueo: centro de la zona
    double radioMetros;    // marcha y bloqueo: radio de la zona
    int via;               // cerrar_via y congestion: indice del nombre de la via
    double factor;         // congestion: multiplicador del tiempo
    int vertice;           // cerrar_estacion: estacion o sede cerrada
    int desde;             // franja horaria en minutos del dia (desde <= hora < hasta);
    int hasta;             //   desde = -1 significa "todo el dia"
    bool desdeArchivo;     // true = viene de escenario.txt, false = clic en el mapa
};

const char* nombreModo(Modo m);
std::string textoHora(int minutoDelDia);   // 450 -> "07:30"

class Enrutador {
public:
    Enrutador();
    ~Enrutador();

    // ---- Carga de datos (Carga.cpp) ----
    bool cargar(const std::string& carpeta);

    // ---- Consultas ----
    int buscar(const std::string& nombre) const;   // sede o estacion -> vertice (tabla hash); -1 si no existe
    const Grafo& red() const { return grafo; }
    int numSedes() const { return sedes.tamano(); }
    int sede(int i) const { return sedes[i]; }
    const std::string& direccion(int i) const { return direcciones[i]; }
    int numEstaciones() const { return estaciones.tamano(); }
    int estacion(int i) const { return estaciones[i]; }

    // ---- Reloj de la simulacion ----
    int hora() const { return minutoDelDia; }
    void cambiarHora(int minuto);             // cambia la hora y aplica los eventos de esa franja
    double factorTrafico() const;             // 1 = vias libres, 0.5 = el carro va a la mitad de velocidad
    bool transmilenioOpera() const;           // TransMilenio funciona de 4:30 a 23:00

    // ---- Rutas (Enrutador.cpp) ----
    bool permitida(const Arista& a, Modo m) const;
    double minutosArista(const Arista& a, Modo m, bool conPenalizacion) const;
    Ruta calcularRuta(int origen, int destino, Modo m);
    int calcularAlternativas(int origen, int destino, Modo m, Ruta rutas[], int maximo);
    Arreglo<Indicacion> indicaciones(const Ruta& r) const;
    double minutosRutaALas(int origen, int destino, Modo m, int minuto);   // sin cambiar la hora actual

    // ---- Contingencias (Contingencias.cpp) ----
    void agregarEvento(const Evento& e);      // agrega y recalcula el estado de la red
    void quitarEvento(int i);
    void quitarEventos(bool delArchivo);      // quita los del archivo o los del mapa
    int numEventos() const { return eventos.tamano(); }
    const Evento& evento(int i) const { return eventos[i]; }
    int zonaEn(Punto p) const;                // evento marcha/bloqueo que contiene a p, o -1
    bool eventoActivo(const Evento& e) const; // segun su franja horaria
    int aplicarEventos();                     // reconstruye el estado; devuelve tramos afectados

    // Para las pruebas: construir un grafo pequeno a mano
    Grafo& redEditable() { return grafo; }
    void registrarLugar(const std::string& nombre, int vertice) { indiceLugares.insertar(nombre, vertice); }
    void prepararMemoria();

private:
    Grafo grafo;
    TablaHash indiceLugares;        // "Sede Calle 40 Ingenieria" -> vertice, "Ricaurte" -> vertice, ...
    Arreglo<int> sedes;
    Arreglo<std::string> direcciones;
    Arreglo<int> estaciones;
    Arreglo<Evento> eventos;
    int minutoDelDia;               // hora simulada: 0 = 00:00, 450 = 07:30

    // Memoria de trabajo de Dijkstra (se reserva una sola vez)
    double* dist;
    int* pred;
    Arista** predArista;
    MinHeap* heap;
    int capacidadMemoria;

    bool dijkstra(int origen, int destino, Modo m, bool conPenalizacion);
    Ruta reconstruir(int origen, int destino, Modo m);
    void quitarPenalizaciones();
    void aplicarZona(const Evento& e);

    // Carga.cpp
    int cargarNodos(const std::string& archivo);
    int cargarTramos(const std::string& archivo);
    int cargarSedes(const std::string& archivo);
    int cargarEstaciones(const std::string& archivo);
    int cargarTramosTM(const std::string& archivo);
    int verticeServicio(int estacion, const std::string& servicio);
    void moverSalidasAServicios(int estacion);
    void conectarALaRed(int v, int cantidad, Modo modo, TipoVia ida, TipoVia vuelta, int numCruces);

    Enrutador(const Enrutador&);
    Enrutador& operator=(const Enrutador&);
};

#endif
