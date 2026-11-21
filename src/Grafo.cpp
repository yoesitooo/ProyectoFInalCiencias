#include "Grafo.h"

Grafo::Grafo() : indiceNombresVia(1021), totalAristas(0) {}

Grafo::~Grafo() {
    for (int i = 0; i < vertices.tamano(); i++) {
        Arista* actual = vertices[i].primera;
        while (actual != nullptr) {
            Arista* siguiente = actual->siguiente;
            delete actual;
            actual = siguiente;
        }
    }
}

int Grafo::agregarVertice(const std::string& nombre, Punto pos, TipoVertice tipo) {
    Vertice v;
    v.nombre = nombre;
    v.pos = pos;
    v.tipo = tipo;
    v.activo = true;
    v.nucleoCarro = true;
    v.nucleoBici = true;
    v.nucleoPie = true;
    v.primera = nullptr;
    vertices.agregar(v);
    return vertices.tamano() - 1;
}

Arista Grafo::crearArista(int destino, double metros, TipoVia tipo) {
    Arista a;
    a.destino = destino;
    a.metros = metros;
    a.tipo = tipo;
    a.permiteCarro = false;
    a.permiteBici = false;
    a.permitePie = false;
    a.esTransMilenio = false;
    a.ciclorruta = false;
    a.via = -1;
    a.inicioTrazado = 0;
    a.puntosTrazado = 0;
    a.cerrada = false;
    a.contingencia = NINGUNA;
    a.factorTrafico = 1.0;
    a.penalizacion = 1.0;
    a.siguiente = nullptr;
    return a;
}

Arista* Grafo::agregarArista(int origen, const Arista& datos) {
    Arista* nueva = new Arista(datos);
    nueva->siguiente = vertices[origen].primera;
    vertices[origen].primera = nueva;
    totalAristas++;
    return nueva;
}

Arista* Grafo::buscarArista(int origen, int destino) const {
    for (Arista* a = vertices[origen].primera; a != nullptr; a = a->siguiente) {
        if (a->destino == destino) return a;
    }
    return nullptr;
}

bool Grafo::eliminarArista(int origen, int destino) {
    Arista* anterior = nullptr;
    for (Arista* a = vertices[origen].primera; a != nullptr; a = a->siguiente) {
        if (a->destino == destino) {
            if (anterior == nullptr) vertices[origen].primera = a->siguiente;
            else anterior->siguiente = a->siguiente;
            delete a;
            totalAristas--;
            return true;
        }
        anterior = a;
    }
    return false;
}

int Grafo::agregarTrazado(const Arreglo<Punto>& trazado) {
    int inicio = puntos.tamano();
    for (int i = 0; i < trazado.tamano(); i++) {
        puntos.agregar(trazado[i]);
    }
    return inicio;
}

Punto Grafo::punto(int i) const {
    return puntos[i];
}

int Grafo::indiceVia(const std::string& nombre) {
    if (nombre.empty()) return -1;
    int i = indiceNombresVia.buscar(nombre);
    if (i != -1) return i;
    nombresVia.agregar(nombre);
    indiceNombresVia.insertar(nombre, nombresVia.tamano() - 1);
    return nombresVia.tamano() - 1;
}

int Grafo::buscarVia(const std::string& nombre) const {
    return indiceNombresVia.buscar(nombre);
}

const std::string& Grafo::nombreVia(int indice) const {
    static const std::string sinNombre = "vía sin nombre";
    if (indice < 0 || indice >= nombresVia.tamano()) return sinNombre;
    return nombresVia[indice];
}

void Grafo::restaurar() {
    for (int i = 0; i < vertices.tamano(); i++) {
        vertices[i].activo = true;
        for (Arista* a = vertices[i].primera; a != nullptr; a = a->siguiente) {
            a->cerrada = false;
            a->contingencia = NINGUNA;
            a->factorTrafico = 1.0;
            a->penalizacion = 1.0;
        }
    }
}

int Grafo::numVertices() const {
    return vertices.tamano();
}

int Grafo::numAristas() const {
    return totalAristas;
}

Vertice& Grafo::vertice(int i) {
    return vertices[i];
}

const Vertice& Grafo::vertice(int i) const {
    return vertices[i];
}
