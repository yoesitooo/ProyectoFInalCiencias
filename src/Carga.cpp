// Lectura de los archivos de datos (CSV separados por ';') y construccion del grafo.
#include <fstream>
#include <iostream>
#include <sstream>
#include "Enrutador.h"
#include "Utilidades.h"

static TipoVia tipoDesdeTexto(const std::string& t) {
    if (t == "autopista") return AUTOPISTA;
    if (t == "troncal") return TRONCAL;
    if (t == "principal") return PRINCIPAL;
    if (t == "secundaria") return SECUNDARIA;
    if (t == "terciaria") return TERCIARIA;
    if (t == "peatonal") return PEATONAL;
    if (t == "ciclorruta") return CICLORRUTA;
    return CONECTOR;
}

// "CBP" -> carro y moto (C), bicicleta (B), peaton (P)
static void ponerPermisos(Arista& a, const std::string& letras) {
    a.permiteCarro = letras.find('C') != std::string::npos;
    a.permiteBici = letras.find('B') != std::string::npos;
    a.permitePie = letras.find('P') != std::string::npos;
}

// Lee "x,y x,y ..." y agrega los puntos al final de 'trazado'
static void leerTrazado(const std::string& texto, Arreglo<Punto>& trazado) {
    std::istringstream entrada(texto);
    std::string par;
    while (entrada >> par) {
        size_t coma = par.find(',');
        double x, y;
        if (coma != std::string::npos && aNumero(par.substr(0, coma), x) && aNumero(par.substr(coma + 1), y)) {
            Punto p = {(float)x, (float)y};
            trazado.agregar(p);
        }
    }
}

// nodos.csv: id;x;y;nucleo_carro;nucleo_bici;nucleo_pie
int Enrutador::cargarNodos(const std::string& archivo) {
    std::ifstream f(archivo);
    if (!f) return -1;
    std::string linea;
    int n = 0;
    while (std::getline(f, linea)) {
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        double x, y;
        if (c.tamano() < 6 || !aNumero(c[1], x) || !aNumero(c[2], y)) continue;
        Punto p = {(float)x, (float)y};
        int v = grafo.agregarVertice("", p, CRUCE);
        grafo.vertice(v).nucleoCarro = (c[3] == "1");
        grafo.vertice(v).nucleoBici = (c[4] == "1");
        grafo.vertice(v).nucleoPie = (c[5] == "1");
        indiceLugares.insertar(c[0], v);   // el id de OpenStreetMap tambien va a la tabla hash
        n++;
    }
    return n;
}

// tramos.csv: origen;destino;metros;tipo;ciclorruta;ida;vuelta;nombre;trazado
int Enrutador::cargarTramos(const std::string& archivo) {
    std::ifstream f(archivo);
    if (!f) return -1;
    std::string linea;
    int n = 0;
    while (std::getline(f, linea)) {
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        if (c.tamano() < 9) continue;
        int a = indiceLugares.buscar(c[0]);
        int b = indiceLugares.buscar(c[1]);
        double metros;
        if (a == -1 || b == -1 || !aNumero(c[2], metros)) continue;

        // Forma completa de la calle: extremo a + puntos intermedios + extremo b
        Arreglo<Punto> trazado;
        trazado.agregar(grafo.vertice(a).pos);
        leerTrazado(c[8], trazado);
        trazado.agregar(grafo.vertice(b).pos);

        Arista datos = Grafo::crearArista(b, metros, tipoDesdeTexto(c[3]));
        datos.ciclorruta = (c[4] == "1");
        datos.via = grafo.indiceVia(c[7]);
        datos.inicioTrazado = grafo.agregarTrazado(trazado);
        datos.puntosTrazado = trazado.tamano();

        ponerPermisos(datos, c[5]);                       // sentido a -> b
        if (c[5] != "-") grafo.agregarArista(a, datos);

        datos.destino = a;                                // sentido b -> a (misma forma)
        ponerPermisos(datos, c[6]);
        if (c[6] != "-") grafo.agregarArista(b, datos);
        n++;
    }
    return n;
}

// sedes.csv: nombre;direccion;x;y
int Enrutador::cargarSedes(const std::string& archivo) {
    std::ifstream f(archivo);
    if (!f) return -1;
    std::string linea;
    while (std::getline(f, linea)) {
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        double x, y;
        if (c.tamano() < 4 || !aNumero(c[2], x) || !aNumero(c[3], y)) continue;
        std::string nombre = recortar(c[0]);
        if (indiceLugares.buscar(nombre) != -1) {
            std::cout << "[Aviso] Sede repetida: " << nombre << "\n";
            continue;
        }
        Punto p = {(float)x, (float)y};
        int v = grafo.agregarVertice(nombre, p, SEDE);
        indiceLugares.insertar(nombre, v);
        sedes.agregar(v);
        direcciones.agregar(recortar(c[1]));
    }
    return sedes.tamano();
}

// tm_estaciones.csv: nombre;x;y
int Enrutador::cargarEstaciones(const std::string& archivo) {
    std::ifstream f(archivo);
    if (!f) return 0;   // TransMilenio es opcional
    std::string linea;
    while (std::getline(f, linea)) {
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        double x, y;
        if (c.tamano() < 3 || !aNumero(c[1], x) || !aNumero(c[2], y)) continue;
        if (indiceLugares.buscar(c[0]) != -1) continue;
        Punto p = {(float)x, (float)y};
        int v = grafo.agregarVertice(c[0], p, ESTACION_TM);
        indiceLugares.insertar(c[0], v);
        estaciones.agregar(v);
    }
    return estaciones.tamano();
}

// tm_tramos.csv: estacion_a;estacion_b;metros;servicios;trazado
int Enrutador::cargarTramosTM(const std::string& archivo) {
    std::ifstream f(archivo);
    if (!f) return 0;
    std::string linea;
    int n = 0;
    while (std::getline(f, linea)) {
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        if (c.tamano() < 5) continue;
        int a = indiceLugares.buscar(c[0]);
        int b = indiceLugares.buscar(c[1]);
        double metros;
        if (a == -1 || b == -1 || !aNumero(c[2], metros)) continue;

        Arreglo<Punto> trazado;
        trazado.agregar(grafo.vertice(a).pos);
        leerTrazado(c[4], trazado);
        trazado.agregar(grafo.vertice(b).pos);

        int inicio = grafo.agregarTrazado(trazado);   // los servicios del tramo comparten la forma

        // Un tramo por cada servicio que lo recorre: "Ricaurte|F19 (Portal Americas)" -> "Banderas|F19 (...)"
        Arreglo<std::string> servicios = separar(c[3], ',');
        for (int s = 0; s < servicios.tamano(); s++) {
            std::string servicio = recortar(servicios[s]);
            if (servicio.empty()) servicio = "TransMilenio";
            int va = verticeServicio(a, servicio);
            int vb = verticeServicio(b, servicio);
            Arista datos = Grafo::crearArista(vb, metros, TM_BUS);
            datos.esTransMilenio = true;
            datos.via = grafo.indiceVia(servicio);
            datos.inicioTrazado = inicio;
            datos.puntosTrazado = trazado.tamano();
            grafo.agregarArista(va, datos);
            n++;
        }
    }
    return n;
}

// Vertice "estacion + servicio" (el servicio parado en esa estacion). Se crea la primera vez que
// se necesita, junto con dos aristas: subirse (cuesta la espera del bus) y bajarse.
// La tabla hash los encuentra con la clave "estacion|servicio".
int Enrutador::verticeServicio(int estacion, const std::string& servicio) {
    std::string clave = grafo.vertice(estacion).nombre + "|" + servicio;
    int v = indiceLugares.buscar(clave);
    if (v != -1) return v;

    v = grafo.agregarVertice(grafo.vertice(estacion).nombre, grafo.vertice(estacion).pos, ESTACION_TM);
    indiceLugares.insertar(clave, v);
    Arista subir = Grafo::crearArista(v, 0, TM_ABORDAR);
    subir.esTransMilenio = true;
    subir.via = grafo.indiceVia(servicio);
    grafo.agregarArista(estacion, subir);
    Arista bajar = Grafo::crearArista(estacion, 0, TM_BAJAR);
    bajar.esTransMilenio = true;
    grafo.agregarArista(v, bajar);
    return v;
}

// A la calle solo se sale bajandose de un bus: las salidas de la estacion pasan a los vertices
// "estacion|servicio". Asi la estacion no sirve de atajo peatonal (entrar y salir sin viajar).
void Enrutador::moverSalidasAServicios(int estacion) {
    Arreglo<Arista> salidas;
    Arreglo<int> servicios;
    for (Arista* a = grafo.vertice(estacion).primera; a != nullptr; a = a->siguiente) {
        if (a->tipo == TM_SALIDA) salidas.agregar(*a);
        if (a->tipo == TM_ABORDAR) servicios.agregar(a->destino);
    }
    for (int i = 0; i < salidas.tamano(); i++) {
        grafo.eliminarArista(estacion, salidas[i].destino);
        for (int s = 0; s < servicios.tamano(); s++) grafo.agregarArista(servicios[s], salidas[i]);
    }
}

// Une el vertice v (sede o estacion) con los 'cantidad' cruces mas cercanos que sirven para 'modo'.
// La union es una linea recta; su longitud se alarga 30% porque en la realidad no es recta.
void Enrutador::conectarALaRed(int v, int cantidad, Modo modo, TipoVia ida, TipoVia vuelta, int numCruces) {
    const int MAX = 4;
    int mejores[MAX];
    double distancias[MAX];
    if (cantidad > MAX) cantidad = MAX;
    for (int j = 0; j < cantidad; j++) {
        mejores[j] = -1;
        distancias[j] = INFINITO;
    }

    for (int i = 0; i < numCruces; i++) {
        const Vertice& c = grafo.vertice(i);
        bool sirve = (modo == MODO_CARRO && c.nucleoCarro) || (modo == MODO_BICI && c.nucleoBici) ||
                     (modo == MODO_PIE && c.nucleoPie);
        if (!sirve) continue;
        double d = distancia(grafo.vertice(v).pos, c.pos);
        // Insercion ordenada en la lista de los mas cercanos
        for (int j = 0; j < cantidad; j++) {
            if (d < distancias[j]) {
                for (int k = cantidad - 1; k > j; k--) {
                    distancias[k] = distancias[k - 1];
                    mejores[k] = mejores[k - 1];
                }
                distancias[j] = d;
                mejores[j] = i;
                break;
            }
        }
    }

    for (int j = 0; j < cantidad; j++) {
        int c = mejores[j];
        if (c == -1) continue;
        Arreglo<Punto> trazado;
        trazado.agregar(grafo.vertice(v).pos);
        trazado.agregar(grafo.vertice(c).pos);
        int inicio = grafo.agregarTrazado(trazado);
        double metros = distancias[j] * METROS_POR_PIXEL * 1.3;

        Arista salida = Grafo::crearArista(c, metros, ida);
        Arista llegada = Grafo::crearArista(v, metros, vuelta);
        Arista* ambas[2] = {&salida, &llegada};
        for (int k = 0; k < 2; k++) {
            Arista* a = ambas[k];
            a->inicioTrazado = inicio;
            a->puntosTrazado = 2;
            if (ida == TM_SALIDA) a->esTransMilenio = true;   // solo lo usa el modo TransMilenio
            else if (modo == MODO_CARRO) a->permiteCarro = true;
            else if (modo == MODO_BICI) a->permiteBici = true;
            else a->permitePie = true;
        }
        grafo.agregarArista(v, salida);
        grafo.agregarArista(c, llegada);
    }
}

bool Enrutador::cargar(const std::string& carpeta) {
    int nodos = cargarNodos(carpeta + "/nodos.csv");
    int tramos = cargarTramos(carpeta + "/tramos.csv");
    if (nodos <= 0 || tramos <= 0) {
        std::cout << "[Error] No se encontro la red vial en '" << carpeta << "'.\n";
        return false;
    }
    int numCruces = grafo.numVertices();

    int numEst = cargarEstaciones(carpeta + "/tm_estaciones.csv");
    int tramosTM = cargarTramosTM(carpeta + "/tm_tramos.csv");
    for (int i = 0; i < estaciones.tamano(); i++) {
        conectarALaRed(estaciones[i], 2, MODO_PIE, TM_SALIDA, TM_ENTRADA, numCruces);
        moverSalidasAServicios(estaciones[i]);
    }

    int numSed = cargarSedes(carpeta + "/sedes.csv");
    if (numSed <= 0) {
        std::cout << "[Error] No se encontraron sedes en '" << carpeta << "/sedes.csv'.\n";
        return false;
    }
    for (int i = 0; i < sedes.tamano(); i++) {
        conectarALaRed(sedes[i], 3, MODO_CARRO, ACCESO_SEDE, ACCESO_SEDE, numCruces);
        conectarALaRed(sedes[i], 2, MODO_BICI, ACCESO_SEDE, ACCESO_SEDE, numCruces);
        conectarALaRed(sedes[i], 2, MODO_PIE, ACCESO_SEDE, ACCESO_SEDE, numCruces);
    }
    prepararMemoria();

    std::cout << "[Datos] " << nodos << " cruces, " << tramos << " tramos de calle, " << numSed << " sedes, "
              << numEst << " estaciones y " << tramosTM << " tramos de TransMilenio\n";
    std::cout << "[Datos] Grafo: " << grafo.numVertices() << " vertices y " << grafo.numAristas()
              << " aristas. Tabla hash: " << indiceLugares.tamano() << " claves en "
              << indiceLugares.numeroCubetas() << " cubetas (carga " << indiceLugares.factorCarga() << ")\n";
    return true;
}
