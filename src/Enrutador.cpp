// Calculo de rutas: costo de cada tramo, Dijkstra, rutas alternativas e indicaciones.
#include <cstdio>
#include "Enrutador.h"
#include "Pila.h"
#include "Utilidades.h"

// ---------------------------------------------------------------------------
// Parametros del modelo de tiempos
// ---------------------------------------------------------------------------

// Velocidad con las vias libres (km/h) segun el tipo de via. Columnas: carro, moto, bici, a pie.
// 0 = ese modo no puede usar ese tipo de via. El trafico de cada hora reduce la del carro y la moto.
static const double VELOCIDAD[NUM_TIPOS_VIA][4] = {
    /* AUTOPISTA   */ {60, 65, 0, 0},
    /* TRONCAL     */ {45, 50, 10, 4.8},
    /* PRINCIPAL   */ {38, 42, 12, 4.8},
    /* SECUNDARIA  */ {32, 36, 13, 4.8},
    /* TERCIARIA   */ {26, 30, 13, 4.8},
    /* PEATONAL    */ {0, 0, 8, 4.8},
    /* CICLORRUTA  */ {0, 0, 16, 4.8},
    /* CONECTOR    */ {0, 0, 10, 4.8},
    /* ACCESO_SEDE */ {15, 18, 12, 4.8},
    /* TM_BUS      */ {0, 0, 0, 0},
    /* TM_ENTRADA  */ {0, 0, 0, 4.8},
    /* TM_SALIDA   */ {0, 0, 0, 4.8},
    /* TM_ABORDAR  */ {0, 0, 0, 0},
    /* TM_BAJAR    */ {0, 0, 0, 0},
};

// Trafico de un dia habil en Bogota: fraccion de la velocidad libre que logra un carro en cada hora
// (0:00 ... 23:00). Estimacion a partir de las horas pico de la ciudad (6-9 a.m. y 4-8 p.m.).
static const double TRAFICO_POR_HORA[24] = {
    1.00, 1.00, 1.00, 1.00, 0.95, 0.80, 0.60, 0.50, 0.55, 0.70, 0.80, 0.80,
    0.70, 0.70, 0.75, 0.75, 0.65, 0.50, 0.45, 0.55, 0.70, 0.85, 0.95, 1.00,
};

static const double VEL_BICI_EN_CICLORRUTA = 15;  // via normal que tiene carril bici
static const double VEL_TRANSMILENIO = 26;        // con el carril exclusivo casi libre
static const double MIN_PARADA_TM = 0.5;          // minutos detenido en cada estacion
static const double MIN_ESPERA_TM = 4;            // espera del bus en hora valle
static const double MIN_ENTRADA_TM = 1;           // pasar el torniquete
static const double MIN_SALIDA_TM = 1;
static const double MIN_BAJAR_TM = 0.5;
static const int TM_ABRE = 4 * 60 + 30;           // 4:30 a.m.
static const int TM_CIERRA = 23 * 60;             // 11:00 p.m.
static const double PENALIZACION_ALTERNATIVA = 1.5;

// Multiplicador del tiempo por contingencia. Filas: NINGUNA, MARCHA, BLOQUEO.
// Columnas: carro, moto, bici, a pie, bus de TransMilenio. 0 = no se puede pasar.
static const double FACTOR_CONTINGENCIA[3][NUM_MODOS] = {
    {1, 1, 1, 1, 1},
    {6, 4, 2.5, 1.3, 5},   // marcha: el trafico casi se detiene
    {0, 0, 0, 2, 0},       // bloqueo: solo se pasa a pie, y despacio
};

const char* nombreModo(Modo m) {
    static const char* nombres[NUM_MODOS] = {"Carro", "Moto", "Bicicleta", "A pie", "TransMilenio"};
    return nombres[m];
}

std::string textoHora(int minutoDelDia) {
    char texto[8];
    std::snprintf(texto, sizeof(texto), "%02d:%02d", (minutoDelDia / 60) % 24, minutoDelDia % 60);
    return texto;
}

// ---------------------------------------------------------------------------

Enrutador::Enrutador()
    : indiceLugares(1021), minutoDelDia(7 * 60), dist(nullptr), pred(nullptr), predArista(nullptr), heap(nullptr),
      capacidadMemoria(0) {}

// ---- Reloj de la simulacion ----
void Enrutador::cambiarHora(int minuto) {
    minutoDelDia = ((minuto % 1440) + 1440) % 1440;   // siempre entre 00:00 y 23:59
    aplicarEventos();                                  // hay eventos que solo existen en ciertas horas
}

double Enrutador::factorTrafico() const {
    return TRAFICO_POR_HORA[minutoDelDia / 60];
}

bool Enrutador::transmilenioOpera() const {
    return minutoDelDia >= TM_ABRE && minutoDelDia < TM_CIERRA;
}

double Enrutador::minutosRutaALas(int origen, int destino, Modo m, int minuto) {
    int guardada = minutoDelDia;
    cambiarHora(minuto);
    Ruta r = calcularRuta(origen, destino, m);
    cambiarHora(guardada);
    return r.encontrada ? r.minutos : INFINITO;
}

Enrutador::~Enrutador() {
    delete[] dist;
    delete[] pred;
    delete[] predArista;
    delete heap;
}

// Reserva los arreglos de Dijkstra segun la cantidad de vertices
void Enrutador::prepararMemoria() {
    delete[] dist;
    delete[] pred;
    delete[] predArista;
    delete heap;
    capacidadMemoria = grafo.numVertices();
    dist = new double[capacidadMemoria];
    pred = new int[capacidadMemoria];
    predArista = new Arista*[capacidadMemoria];
    heap = new MinHeap(capacidadMemoria);
}

int Enrutador::buscar(const std::string& nombre) const {
    return indiceLugares.buscar(nombre);
}

bool Enrutador::permitida(const Arista& a, Modo m) const {
    if (a.cerrada) return false;
    switch (m) {
        case MODO_CARRO:
        case MODO_MOTO: return a.permiteCarro;
        case MODO_BICI: return a.permiteBici;
        case MODO_PIE: return a.permitePie;
        case MODO_TM: return a.permitePie || a.esTransMilenio;   // se camina hasta la estacion
        default: return false;
    }
}

// Minutos para recorrer la arista en el modo m. INFINITO si no se puede.
double Enrutador::minutosArista(const Arista& a, Modo m, bool conPenalizacion) const {
    if (!permitida(a, m)) return INFINITO;

    double minutos;
    double trafico = factorTrafico();    // 1 = vias libres; 0.5 = mitad de velocidad
    Modo modoEfectivo = m;               // en modo TransMilenio, fuera del bus se camina
    if (a.tipo == TM_BUS) {
        double v = VEL_TRANSMILENIO * (1 - 0.5 * (1 - trafico));   // el carril exclusivo sufre menos
        minutos = a.metros * 0.06 / v + MIN_PARADA_TM;              // metros * 60 / (v * 1000)
    } else if (a.tipo == TM_ABORDAR) {
        // Subirse a un servicio cuesta la espera del bus; por eso cada transbordo "cuesta" otra espera
        if (!transmilenioOpera()) return INFINITO;
        minutos = MIN_ESPERA_TM + 8 * (1 - trafico);               // en hora pico los buses llegan llenos
    } else if (a.tipo == TM_BAJAR) {
        minutos = MIN_BAJAR_TM;
    } else {
        if (m == MODO_TM) modoEfectivo = MODO_PIE;
        double v = VELOCIDAD[a.tipo][modoEfectivo];
        if (m == MODO_BICI && a.ciclorruta && v < VEL_BICI_EN_CICLORRUTA) v = VEL_BICI_EN_CICLORRUTA;
        if (m == MODO_CARRO) v *= trafico;
        if (m == MODO_MOTO) v *= 1 - 0.4 * (1 - trafico);           // la moto se mueve entre los carros
        if (v <= 0) return INFINITO;
        minutos = a.metros * 0.06 / v;
        if (a.tipo == TM_ENTRADA) minutos += MIN_ENTRADA_TM;
        if (a.tipo == TM_SALIDA) minutos += MIN_SALIDA_TM;
    }

    double factor = FACTOR_CONTINGENCIA[a.contingencia][modoEfectivo];
    if (factor == 0) return INFINITO;
    minutos *= factor;
    if (m == MODO_CARRO || m == MODO_MOTO) minutos *= a.factorTrafico;   // congestion reportada
    if (conPenalizacion) minutos *= a.penalizacion;
    return minutos;
}

// ---------------------------------------------------------------------------
// Dijkstra con Min-Heap indexado: O((V + E) log V)
// Es correcto porque ningun tramo tiene tiempo negativo.
// ---------------------------------------------------------------------------
bool Enrutador::dijkstra(int origen, int destino, Modo m, bool conPenalizacion) {
    int n = grafo.numVertices();
    for (int i = 0; i < n; i++) {
        dist[i] = INFINITO;
        pred[i] = -1;
        predArista[i] = nullptr;
    }
    heap->vaciar();
    dist[origen] = 0;
    heap->insertar(origen, 0);

    while (!heap->vacio()) {
        int u = heap->extraerMin().vertice;
        if (u == destino) break;   // la distancia del destino ya es definitiva

        for (Arista* a = grafo.vertice(u).primera; a != nullptr; a = a->siguiente) {
            int v = a->destino;
            const Vertice& vv = grafo.vertice(v);
            if (!vv.activo && v != destino) continue;           // estacion o sede cerrada
            if (vv.tipo == SEDE && v != destino) continue;      // una sede no sirve de atajo

            double w = minutosArista(*a, m, conPenalizacion);
            if (w >= INFINITO) continue;
            if (dist[u] + w < dist[v]) {                        // relajacion
                dist[v] = dist[u] + w;
                pred[v] = u;
                predArista[v] = a;
                if (heap->contiene(v)) heap->disminuirClave(v, dist[v]);
                else heap->insertar(v, dist[v]);
            }
        }
    }
    heap->vaciar();
    return dist[destino] < INFINITO;
}

// Recorre los predecesores desde el destino; la pila los entrega en orden origen -> destino
Ruta Enrutador::reconstruir(int origen, int destino, Modo m) {
    Ruta r;
    r.encontrada = true;
    r.modo = m;
    r.origen = origen;
    r.destino = destino;

    Pila<int> pila;
    for (int v = destino; v != origen; v = pred[v]) {
        pila.push(v);
    }

    int anterior = origen;
    while (!pila.vacia()) {
        int actual = pila.pop();
        Paso p;
        p.desde = anterior;
        p.hasta = actual;
        p.arista = predArista[actual];
        p.minutos = minutosArista(*p.arista, m, false);   // tiempo real, sin penalizacion
        r.pasos.agregar(p);
        r.minutos += p.minutos;
        r.metros += p.arista->metros;
        anterior = actual;
    }
    return r;
}

Ruta Enrutador::calcularRuta(int origen, int destino, Modo m) {
    Ruta vacia;
    vacia.modo = m;
    vacia.origen = origen;
    vacia.destino = destino;
    int n = grafo.numVertices();
    if (origen < 0 || destino < 0 || origen >= n || destino >= n || origen == destino) return vacia;
    if (!dijkstra(origen, destino, m, false)) return vacia;
    return reconstruir(origen, destino, m);
}

// Metros que la ruta a comparte con la ruta b
static double metrosCompartidos(const Ruta& a, const Ruta& b) {
    double total = 0;
    for (int i = 0; i < a.pasos.tamano(); i++) {
        for (int j = 0; j < b.pasos.tamano(); j++) {
            if (a.pasos[i].arista == b.pasos[j].arista) {
                total += a.pasos[i].arista->metros;
                break;
            }
        }
    }
    return total;
}

void Enrutador::quitarPenalizaciones() {
    for (int i = 0; i < grafo.numVertices(); i++) {
        for (Arista* a = grafo.vertice(i).primera; a != nullptr; a = a->siguiente) {
            a->penalizacion = 1.0;
        }
    }
}

// Rutas alternativas por penalizacion: despues de cada ruta hallada se encarecen
// temporalmente sus tramos y se vuelve a ejecutar Dijkstra. Una candidata se acepta
// si comparte menos del 75% del recorrido con las anteriores y no es mucho mas lenta.
int Enrutador::calcularAlternativas(int origen, int destino, Modo m, Ruta rutas[], int maximo) {
    rutas[0] = calcularRuta(origen, destino, m);
    if (!rutas[0].encontrada) return 0;
    int cantidad = 1;

    for (int intento = 0; intento < 6 && cantidad < maximo; intento++) {
        const Ruta& ultima = rutas[cantidad - 1];
        for (int i = 0; i < ultima.pasos.tamano(); i++) {
            ultima.pasos[i].arista->penalizacion *= PENALIZACION_ALTERNATIVA;
        }
        if (!dijkstra(origen, destino, m, true)) break;
        Ruta candidata = reconstruir(origen, destino, m);

        bool distinta = true;
        for (int j = 0; j < cantidad; j++) {
            if (metrosCompartidos(candidata, rutas[j]) > 0.75 * candidata.metros) distinta = false;
        }
        bool razonable = candidata.minutos <= rutas[0].minutos * 1.6 + 5;
        if (distinta && razonable) {
            rutas[cantidad] = candidata;
            cantidad++;
        } else {
            // Se encarece tambien la candidata para empujar la busqueda hacia otras vias
            for (int i = 0; i < candidata.pasos.tamano(); i++) {
                candidata.pasos[i].arista->penalizacion *= 1.2;
            }
        }
    }
    quitarPenalizaciones();
    return cantidad;
}

// ---------------------------------------------------------------------------
// Indicaciones: se agrupan los pasos consecutivos por la misma via
// ---------------------------------------------------------------------------
static bool esCalle(TipoVia t) {
    return t <= CONECTOR;
}

static const char* verbo(Modo m) {
    if (m == MODO_CARRO) return "Conduce por ";
    if (m == MODO_MOTO) return "Sigue en moto por ";
    if (m == MODO_BICI) return "Pedalea por ";
    return "Camina por ";
}

Arreglo<Indicacion> Enrutador::indicaciones(const Ruta& r) const {
    Arreglo<Indicacion> lista;
    int i = 0;
    int n = r.pasos.tamano();
    while (i < n) {
        const Paso& p = r.pasos[i];
        Indicacion ind;
        ind.tipo = p.arista->tipo;
        ind.metros = 0;
        ind.minutos = 0;

        if (p.arista->tipo == ACCESO_SEDE) {
            const Vertice& desde = grafo.vertice(p.desde);
            if (desde.tipo == SEDE) ind.texto = "Sal de " + desde.nombre + " hacia la vía principal";
            else ind.texto = "Llega a " + grafo.vertice(p.hasta).nombre;
            ind.metros = p.arista->metros;
            ind.minutos = p.minutos;
            i++;
        } else if (p.arista->tipo == TM_ENTRADA) {
            ind.texto = "Entra a la estación " + grafo.vertice(p.hasta).nombre;
            ind.metros = p.arista->metros;
            ind.minutos = p.minutos;
            i++;
        } else if (p.arista->tipo == TM_SALIDA) {
            ind.texto = "Sal de la estación " + grafo.vertice(p.desde).nombre;
            ind.metros = p.arista->metros;
            ind.minutos = p.minutos;
            i++;
        } else if (p.arista->tipo == TM_ABORDAR) {
            // Subirse a un servicio, viajar las paradas seguidas en ese servicio y bajarse
            std::string servicio = grafo.nombreVia(p.arista->via);
            int inicio = p.desde;
            double espera = p.minutos;
            ind.minutos = espera;
            ind.tipo = TM_BUS;
            int paradas = 0;
            i++;
            while (i < n && r.pasos[i].arista->tipo == TM_BUS) {
                ind.metros += r.pasos[i].arista->metros;
                ind.minutos += r.pasos[i].minutos;
                paradas++;
                i++;
            }
            int fin = r.pasos[i - 1].hasta;   // ultima parada del bus (se sale a la calle o se transborda)
            if (i < n && r.pasos[i].arista->tipo == TM_BAJAR) {
                ind.minutos += r.pasos[i].minutos;
                fin = r.pasos[i].hasta;
                i++;
            }
            bool transbordo = !lista.vacio() && lista[lista.tamano() - 1].tipo == TM_BUS;
            char detalle[64];
            std::snprintf(detalle, sizeof(detalle), " (%d %s, espera aprox. %.0f min)", paradas,
                          paradas == 1 ? "parada" : "paradas", espera);
            ind.texto = (transbordo ? "Transbordo en " : "En la estación ") + grafo.vertice(inicio).nombre +
                        " toma el " + servicio + " y bájate en " + grafo.vertice(fin).nombre + detalle;
        } else if (p.arista->tipo == TM_BUS || p.arista->tipo == TM_BAJAR) {
            i++;   // no deberia pasar: todo viaje en bus empieza con TM_ABORDAR
            continue;
        } else {
            // Calle: se suman los pasos seguidos con el mismo nombre de via.
            // Los tramos muy cortos de otra via (un cruce, un conector) no cortan la indicacion.
            int via = p.arista->via;
            bool ciclorruta = p.arista->ciclorruta || p.arista->tipo == CICLORRUTA;
            while (i < n && esCalle(r.pasos[i].arista->tipo)) {
                const Arista* b = r.pasos[i].arista;
                bool muyCorto = b->metros < 80 || (b->via == -1 && b->metros < 120);
                if (b->via != via && !muyCorto) {
                    if (via == -1 && ind.metros < 120) via = b->via;   // el inicio sin nombre se une
                    else break;
                }
                ind.metros += b->metros;
                ind.minutos += r.pasos[i].minutos;
                i++;
            }
            std::string nombre = grafo.nombreVia(via);
            if (r.modo == MODO_BICI && ciclorruta && nombre != "Ciclorruta") nombre = "la ciclorruta de " + nombre;
            ind.texto = verbo(r.modo) + nombre;
        }
        lista.agregar(ind);
    }
    return lista;
}
