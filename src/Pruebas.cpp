#include "Pruebas.h"
#include <cmath>
#include <iostream>
#include <string>
#include "Arreglo.h"
#include "Enrutador.h"
#include "MinHeap.h"
#include "Pila.h"
#include "TablaHash.h"

static int fallas = 0;

static void comprobar(bool condicion, const std::string& descripcion) {
    std::cout << "   [" << (condicion ? " OK " : "FALLA") << "] " << descripcion << "\n";
    if (!condicion) fallas++;
}

static bool casiIgual(double a, double b) {
    return std::fabs(a - b) < 1e-6;
}

static void probarArreglo() {
    std::cout << "Arreglo dinamico\n";
    Arreglo<int> a;
    for (int i = 0; i < 100; i++) a.agregar(i * i);
    comprobar(a.tamano() == 100 && a[10] == 100 && a[99] == 9801, "agregar 100 elementos (crece solo)");
    Arreglo<int> copia = a;
    copia[0] = -5;
    comprobar(a[0] == 0 && copia[0] == -5, "la copia es independiente (copia profunda)");
    a.quitar(0);
    comprobar(a.tamano() == 99 && a[0] == 1, "quitar corre los elementos");
}

static void probarPila() {
    std::cout << "Pila\n";
    Pila<int> p;
    p.push(10);
    p.push(20);
    p.push(30);
    comprobar(p.top() == 30 && p.tamano() == 3, "push y top");
    int a = p.pop(), b = p.pop(), c = p.pop();
    comprobar(a == 30 && b == 20 && c == 10, "pop en orden inverso (LIFO)");
    comprobar(p.vacia(), "queda vacia");
}

static void probarHeap() {
    std::cout << "Min-Heap indexado\n";
    MinHeap h(10);
    h.insertar(0, 50);
    h.insertar(1, 20);
    h.insertar(2, 70);
    h.insertar(3, 10);
    comprobar(h.contiene(2) && !h.contiene(7), "contiene() usa el arreglo pos[]");
    h.disminuirClave(2, 5);
    ElementoHeap e1 = h.extraerMin();
    ElementoHeap e2 = h.extraerMin();
    ElementoHeap e3 = h.extraerMin();
    ElementoHeap e4 = h.extraerMin();
    comprobar(e1.vertice == 2 && casiIgual(e1.clave, 5), "disminuirClave sube el vertice 2 a la raiz");
    comprobar(e2.vertice == 3 && e3.vertice == 1 && e4.vertice == 0, "se extrae en orden creciente");
    comprobar(h.vacio(), "queda vacio");
    h.insertar(4, 1);
    h.vaciar();
    comprobar(h.vacio() && !h.contiene(4), "vaciar() lo deja listo para reutilizar");
}

static void probarHash() {
    std::cout << "Tabla hash\n";
    TablaHash t(4);
    for (int i = 0; i < 200; i++) t.insertar("Estacion " + std::to_string(i), i);
    comprobar(t.tamano() == 200, "200 claves insertadas");
    comprobar(t.numeroCubetas() > 4 && t.factorCarga() <= 0.75, "el rehash mantiene la carga <= 0.75");
    bool todas = true;
    for (int i = 0; i < 200; i++) {
        if (t.buscar("Estacion " + std::to_string(i)) != i) todas = false;
    }
    comprobar(todas, "todas las claves se encuentran despues del rehash");
    comprobar(t.buscar("No existe") == -1, "clave inexistente devuelve -1");
    comprobar(t.eliminar("Estacion 7") && t.buscar("Estacion 7") == -1, "eliminar");
}

// Grafo de prueba (via terciaria, carro a 26 km/h sin trafico):
//        A --1000 m--> B --1000 m--> D
//        A --500 m---> C --3000 m--> D
static void probarDijkstra() {
    std::cout << "Grafo + Dijkstra + contingencias\n";
    Enrutador e;
    Grafo& g = e.redEditable();
    Punto pa = {0, 0}, pb = {200, 0}, pc = {0, 300}, pd = {400, 0};
    int A = g.agregarVertice("A", pa, CRUCE);
    int B = g.agregarVertice("B", pb, CRUCE);
    int C = g.agregarVertice("C", pc, CRUCE);
    int D = g.agregarVertice("D", pd, CRUCE);
    int extremos[4][2] = {{A, B}, {B, D}, {A, C}, {C, D}};
    double metros[4] = {1000, 1000, 500, 3000};
    for (int i = 0; i < 4; i++) {
        int u = extremos[i][0], v = extremos[i][1];
        Arreglo<Punto> trazado;
        trazado.agregar(g.vertice(u).pos);
        trazado.agregar(g.vertice(v).pos);
        Arista a = Grafo::crearArista(v, metros[i], TERCIARIA);
        a.permiteCarro = a.permiteBici = a.permitePie = true;
        a.inicioTrazado = g.agregarTrazado(trazado);
        a.puntosTrazado = 2;
        g.agregarArista(u, a);
    }
    e.registrarLugar("A", A);
    e.prepararMemoria();
    comprobar(g.numAristas() == 4 && e.buscar("A") == A, "grafo con 4 aristas y busqueda por nombre");

    // A las 2:00 a.m. no hay trafico: el carro va a la velocidad libre de una via terciaria (26 km/h)
    e.cambiarHora(2 * 60);
    double libre = 2000 * 0.06 / 26;   // minutos = metros * 60 / (km/h * 1000)
    Ruta r = e.calcularRuta(A, D, MODO_CARRO);
    comprobar(r.encontrada && r.pasos.tamano() == 2 && r.pasos[0].hasta == B && casiIgual(r.minutos, libre),
              "ruta optima A-B-D a las 2:00 (2000 m a 26 km/h = 4.6 min)");
    e.cambiarHora(7 * 60);
    r = e.calcularRuta(A, D, MODO_CARRO);
    comprobar(casiIgual(r.minutos, libre * 2), "a las 7:00 (hora pico) el mismo viaje tarda el doble");

    Evento marcha;
    marcha.tipo = EV_MARCHA;
    marcha.centro = pb;
    marcha.radioMetros = 100;
    marcha.via = -1;
    marcha.factor = 1;
    marcha.vertice = -1;
    marcha.desde = 10 * 60;            // la marcha solo existe de 10:00 a 12:00
    marcha.hasta = 12 * 60;
    marcha.desdeArchivo = false;
    e.agregarEvento(marcha);
    e.cambiarHora(11 * 60);
    r = e.calcularRuta(A, D, MODO_CARRO);
    comprobar(r.encontrada && r.pasos[0].hasta == C, "a las 11:00 la marcha en B desvia el carro por C");
    r = e.calcularRuta(A, D, MODO_PIE);
    comprobar(r.encontrada && r.pasos[0].hasta == B, "a pie la marcha casi no afecta: sigue por B");
    e.cambiarHora(13 * 60);
    r = e.calcularRuta(A, D, MODO_CARRO);
    comprobar(r.encontrada && r.pasos[0].hasta == B, "a las 13:00 la marcha ya termino: vuelve a ir por B");

    e.quitarEventos(false);
    e.cambiarHora(2 * 60);
    Ruta rutas[MAX_RUTAS];
    int n = e.calcularAlternativas(A, D, MODO_CARRO, rutas, MAX_RUTAS);
    comprobar(n == 2 && rutas[1].pasos[0].hasta == C && casiIgual(rutas[1].minutos, 3500 * 0.06 / 26),
              "la alternativa es A-C-D (3500 m)");

    g.buscarArista(A, B)->cerrada = true;
    g.buscarArista(C, D)->cerrada = true;
    r = e.calcularRuta(A, D, MODO_CARRO);
    comprobar(!r.encontrada, "sin ruta cuando ambos caminos estan cerrados");

    comprobar(g.eliminarArista(A, C) && g.numAristas() == 3, "eliminarArista");
}

int ejecutarPruebas() {
    fallas = 0;
    std::cout << "\n===== Pruebas de las estructuras de datos =====\n";
    probarArreglo();
    probarPila();
    probarHeap();
    probarHash();
    probarDijkstra();
    std::cout << "Resultado: " << (fallas == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY FALLAS") << " (" << fallas
              << " fallas)\n\n";
    return fallas;
}
