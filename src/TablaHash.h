#ifndef TABLAHASH_H
#define TABLAHASH_H

#include <string>

// ---------------------------------------------------------------------------
// Tabla hash con encadenamiento separado: texto -> entero.
//   - cada cubeta es una lista enlazada con las claves que caen en ella
//   - funcion hash polinomial: h = h * 31 + caracter
//   - si el factor de carga (claves / cubetas) pasa de 0.75 se duplica el
//     numero de cubetas y se reubican todas las claves (rehash)
// Busqueda e insercion: O(1) en promedio, O(n) en el peor caso.
//
// En el proyecto traduce "Sede Calle 40 Ingenieria" o "Ricaurte" a su indice
// en el grafo, y "Avenida Caracas" a su indice en la lista de nombres de vias.
// ---------------------------------------------------------------------------
class TablaHash {
public:
    explicit TablaHash(int cubetasIniciales = 31);
    ~TablaHash();

    void insertar(const std::string& clave, int valor);  // si ya existe, actualiza el valor
    int buscar(const std::string& clave) const;          // -1 si no existe
    bool eliminar(const std::string& clave);

    int tamano() const;
    int numeroCubetas() const;
    double factorCarga() const;

private:
    struct Nodo {
        std::string clave;
        int valor;
        Nodo* siguiente;
    };

    Nodo** cubetas;
    int cubetasTotales;
    int cantidad;

    unsigned long hash(const std::string& clave) const;
    void rehash();

    TablaHash(const TablaHash&);
    TablaHash& operator=(const TablaHash&);
};

#endif
