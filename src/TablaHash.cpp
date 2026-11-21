#include "TablaHash.h"

static const double FACTOR_CARGA_MAXIMO = 0.75;

TablaHash::TablaHash(int cubetasIniciales) : cubetasTotales(cubetasIniciales), cantidad(0) {
    if (cubetasTotales < 1) cubetasTotales = 31;
    cubetas = new Nodo*[cubetasTotales];
    for (int i = 0; i < cubetasTotales; i++) {
        cubetas[i] = nullptr;
    }
}

TablaHash::~TablaHash() {
    for (int i = 0; i < cubetasTotales; i++) {
        Nodo* actual = cubetas[i];
        while (actual != nullptr) {
            Nodo* siguiente = actual->siguiente;
            delete actual;
            actual = siguiente;
        }
    }
    delete[] cubetas;
}

// Hash polinomial: trata el texto como un numero en base 31
unsigned long TablaHash::hash(const std::string& clave) const {
    unsigned long h = 0;
    for (unsigned char c : clave) {
        h = h * 31 + c;
    }
    return h;
}

void TablaHash::insertar(const std::string& clave, int valor) {
    int indice = (int)(hash(clave) % cubetasTotales);

    // Si la clave ya existe solo se actualiza su valor
    for (Nodo* n = cubetas[indice]; n != nullptr; n = n->siguiente) {
        if (n->clave == clave) {
            n->valor = valor;
            return;
        }
    }

    // Clave nueva: se inserta al inicio de la lista de esa cubeta
    Nodo* nuevo = new Nodo;
    nuevo->clave = clave;
    nuevo->valor = valor;
    nuevo->siguiente = cubetas[indice];
    cubetas[indice] = nuevo;
    cantidad++;

    if (factorCarga() > FACTOR_CARGA_MAXIMO) {
        rehash();
    }
}

int TablaHash::buscar(const std::string& clave) const {
    int indice = (int)(hash(clave) % cubetasTotales);
    for (Nodo* n = cubetas[indice]; n != nullptr; n = n->siguiente) {
        if (n->clave == clave) return n->valor;
    }
    return -1;
}

bool TablaHash::eliminar(const std::string& clave) {
    int indice = (int)(hash(clave) % cubetasTotales);
    Nodo* anterior = nullptr;
    for (Nodo* n = cubetas[indice]; n != nullptr; n = n->siguiente) {
        if (n->clave == clave) {
            if (anterior == nullptr) cubetas[indice] = n->siguiente;
            else anterior->siguiente = n->siguiente;
            delete n;
            cantidad--;
            return true;
        }
        anterior = n;
    }
    return false;
}

// Duplica las cubetas y reubica cada nodo segun su nuevo indice (los nodos no se copian, se mueven)
void TablaHash::rehash() {
    int viejasCubetas = cubetasTotales;
    Nodo** viejas = cubetas;

    cubetasTotales = viejasCubetas * 2 + 1;
    cubetas = new Nodo*[cubetasTotales];
    for (int i = 0; i < cubetasTotales; i++) {
        cubetas[i] = nullptr;
    }

    for (int i = 0; i < viejasCubetas; i++) {
        Nodo* actual = viejas[i];
        while (actual != nullptr) {
            Nodo* siguiente = actual->siguiente;
            int indice = (int)(hash(actual->clave) % cubetasTotales);
            actual->siguiente = cubetas[indice];
            cubetas[indice] = actual;
            actual = siguiente;
        }
    }
    delete[] viejas;
}

int TablaHash::tamano() const {
    return cantidad;
}

int TablaHash::numeroCubetas() const {
    return cubetasTotales;
}

double TablaHash::factorCarga() const {
    return (double)cantidad / cubetasTotales;
}
