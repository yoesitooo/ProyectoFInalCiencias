#ifndef MINHEAP_H
#define MINHEAP_H

// ---------------------------------------------------------------------------
// MinHeap indexado: arbol binario completo guardado en un arreglo.
//   - hijos del nodo i: 2i+1 y 2i+2;  padre: (i-1)/2
//   - pos[v] guarda en que posicion del arreglo esta el vertice v (-1 si no esta).
//     Gracias a pos[] se puede hacer disminuirClave en O(log V) sin buscar.
// Dijkstra lo usa para sacar siempre el vertice con menor tiempo acumulado.
// ---------------------------------------------------------------------------

struct ElementoHeap {
    int vertice;
    double clave;   // tiempo acumulado en minutos
};

class MinHeap {
public:
    explicit MinHeap(int capacidad);   // capacidad = numero de vertices del grafo
    ~MinHeap();

    void insertar(int vertice, double clave);            // O(log V)
    ElementoHeap extraerMin();                           // O(log V)
    void disminuirClave(int vertice, double nuevaClave); // O(log V)
    bool contiene(int vertice) const;                    // O(1)
    bool vacio() const;
    int tamano() const;
    void vaciar();                                       // deja el heap listo para otra busqueda

private:
    ElementoHeap* datos;
    int* pos;
    int cantidad;
    int capacidad;

    void subir(int i);
    void bajar(int i);
    void intercambiar(int i, int j);

    MinHeap(const MinHeap&);
    MinHeap& operator=(const MinHeap&);
};

#endif
