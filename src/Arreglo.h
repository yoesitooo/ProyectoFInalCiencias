#ifndef ARREGLO_H
#define ARREGLO_H

// ---------------------------------------------------------------------------
// Arreglo<T>: arreglo dinamico propio (no se usa std::vector).
//
// Guarda los elementos en un bloque contiguo creado con new[]. Cuando se llena,
// reserva un bloque del doble de tamano y copia los elementos: por eso agregar
// cuesta O(1) amortizado. Acceso por indice: O(1).
// ---------------------------------------------------------------------------
template <typename T>
class Arreglo {
public:
    Arreglo() : datos(nullptr), cantidad(0), capacidad(0) {}

    ~Arreglo() {
        delete[] datos;
    }

    // Copia profunda: la copia tiene su propia memoria (regla de los tres).
    Arreglo(const Arreglo& otro) : datos(nullptr), cantidad(0), capacidad(0) {
        copiarDe(otro);
    }

    Arreglo& operator=(const Arreglo& otro) {
        if (this != &otro) {
            delete[] datos;
            datos = nullptr;
            cantidad = 0;
            capacidad = 0;
            copiarDe(otro);
        }
        return *this;
    }

    void agregar(const T& valor) {
        if (cantidad == capacidad) {
            crecer();
        }
        datos[cantidad] = valor;
        cantidad++;
    }

    // Quita el elemento i corriendo los siguientes una posicion: O(n)
    void quitar(int i) {
        for (int k = i; k + 1 < cantidad; k++) {
            datos[k] = datos[k + 1];
        }
        cantidad--;
    }

    void limpiar() { cantidad = 0; }

    T& operator[](int i) { return datos[i]; }
    const T& operator[](int i) const { return datos[i]; }

    int tamano() const { return cantidad; }
    bool vacio() const { return cantidad == 0; }

private:
    T* datos;
    int cantidad;
    int capacidad;

    void crecer() {
        int nuevaCapacidad = (capacidad == 0) ? 8 : capacidad * 2;
        T* nuevos = new T[nuevaCapacidad];
        for (int i = 0; i < cantidad; i++) {
            nuevos[i] = datos[i];
        }
        delete[] datos;
        datos = nuevos;
        capacidad = nuevaCapacidad;
    }

    void copiarDe(const Arreglo& otro) {
        if (otro.cantidad == 0) return;
        datos = new T[otro.cantidad];
        capacidad = otro.cantidad;
        for (int i = 0; i < otro.cantidad; i++) {
            datos[i] = otro.datos[i];
        }
        cantidad = otro.cantidad;
    }
};

#endif
