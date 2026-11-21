#ifndef PILA_H
#define PILA_H

// ---------------------------------------------------------------------------
// Pila<T>: pila LIFO implementada con una lista simplemente enlazada.
// push, pop y top son O(1). Se usa para invertir la cadena de predecesores
// de Dijkstra (destino -> origen) y obtener la ruta en orden (origen -> destino).
// ---------------------------------------------------------------------------
template <typename T>
class Pila {
public:
    Pila() : tope(nullptr), cantidad(0) {}

    ~Pila() {
        while (!vacia()) {
            pop();
        }
    }

    void push(const T& valor) {
        Nodo* nuevo = new Nodo;
        nuevo->valor = valor;
        nuevo->siguiente = tope;
        tope = nuevo;
        cantidad++;
    }

    // Precondicion: la pila no esta vacia
    T pop() {
        Nodo* viejo = tope;
        T valor = viejo->valor;
        tope = viejo->siguiente;
        delete viejo;
        cantidad--;
        return valor;
    }

    // Precondicion: la pila no esta vacia
    const T& top() const { return tope->valor; }

    bool vacia() const { return tope == nullptr; }
    int tamano() const { return cantidad; }

private:
    struct Nodo {
        T valor;
        Nodo* siguiente;
    };

    Nodo* tope;
    int cantidad;

    // No se permite copiar una pila (evita liberar dos veces los mismos nodos)
    Pila(const Pila&);
    Pila& operator=(const Pila&);
};

#endif
