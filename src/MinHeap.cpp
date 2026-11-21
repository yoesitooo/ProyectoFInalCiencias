#include "MinHeap.h"

MinHeap::MinHeap(int capacidad) : cantidad(0), capacidad(capacidad) {
    datos = new ElementoHeap[capacidad];
    pos = new int[capacidad];
    for (int v = 0; v < capacidad; v++) {
        pos[v] = -1;
    }
}

MinHeap::~MinHeap() {
    delete[] datos;
    delete[] pos;
}

void MinHeap::intercambiar(int i, int j) {
    ElementoHeap temporal = datos[i];
    datos[i] = datos[j];
    datos[j] = temporal;
    // Mantener el indice al dia
    pos[datos[i].vertice] = i;
    pos[datos[j].vertice] = j;
}

// Sube el elemento i mientras sea menor que su padre
void MinHeap::subir(int i) {
    while (i > 0) {
        int padre = (i - 1) / 2;
        if (datos[i].clave >= datos[padre].clave) break;
        intercambiar(i, padre);
        i = padre;
    }
}

// Baja el elemento i mientras alguno de sus hijos sea menor
void MinHeap::bajar(int i) {
    while (true) {
        int izquierdo = 2 * i + 1;
        int derecho = 2 * i + 2;
        int menor = i;
        if (izquierdo < cantidad && datos[izquierdo].clave < datos[menor].clave) menor = izquierdo;
        if (derecho < cantidad && datos[derecho].clave < datos[menor].clave) menor = derecho;
        if (menor == i) break;
        intercambiar(i, menor);
        i = menor;
    }
}

void MinHeap::insertar(int vertice, double clave) {
    if (vertice < 0 || vertice >= capacidad) return;
    if (pos[vertice] != -1) {          // ya esta: solo se mejora su clave
        disminuirClave(vertice, clave);
        return;
    }
    datos[cantidad].vertice = vertice;
    datos[cantidad].clave = clave;
    pos[vertice] = cantidad;
    cantidad++;
    subir(cantidad - 1);
}

ElementoHeap MinHeap::extraerMin() {
    ElementoHeap minimo = datos[0];
    pos[minimo.vertice] = -1;
    cantidad--;
    if (cantidad > 0) {
        datos[0] = datos[cantidad];    // el ultimo pasa a la raiz y se hunde
        pos[datos[0].vertice] = 0;
        bajar(0);
    }
    return minimo;
}

void MinHeap::disminuirClave(int vertice, double nuevaClave) {
    int i = pos[vertice];
    if (i == -1 || nuevaClave >= datos[i].clave) return;
    datos[i].clave = nuevaClave;
    subir(i);
}

bool MinHeap::contiene(int vertice) const {
    return vertice >= 0 && vertice < capacidad && pos[vertice] != -1;
}

bool MinHeap::vacio() const {
    return cantidad == 0;
}

int MinHeap::tamano() const {
    return cantidad;
}

void MinHeap::vaciar() {
    for (int i = 0; i < cantidad; i++) {
        pos[datos[i].vertice] = -1;
    }
    cantidad = 0;
}
