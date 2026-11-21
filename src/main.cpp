// Motor de Enrutamiento y Movilidad Inter-Sedes - Universidad Distrital
// Ciencias de la Computacion I
//
//   enrutador              ventana con el mapa (raylib)
//   enrutador --consola    menu de consola (interfaz de respaldo)
//   enrutador --pruebas    pruebas automaticas de las estructuras
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include "Enrutador.h"
#include "Escenario.h"
#include "Horario.h"
#include "Mapa.h"
#include "Pruebas.h"
#include "Utilidades.h"

static std::string buscarCarpetaDatos() {
    const char* candidatas[] = {"datos", "../datos", "../../datos"};
    for (const char* c : candidatas) {
        std::ifstream prueba(std::string(c) + "/sedes.csv");
        if (prueba) return c;
    }
    return "datos";
}

static std::string leerLinea(const std::string& mensaje) {
    std::cout << mensaje;
    std::string linea;
    std::getline(std::cin, linea);
    return recortar(linea);
}

static void imprimirRuta(const Enrutador& e, const Ruta& r) {
    if (!r.encontrada) {
        std::cout << "  No hay ruta en " << nombreModo(r.modo) << " (revise las contingencias).\n";
        return;
    }
    char resumen[160];
    std::snprintf(resumen, sizeof(resumen), "  %s -> %s en %s: %.0f min, %.1f km\n",
                  e.red().vertice(r.origen).nombre.c_str(), e.red().vertice(r.destino).nombre.c_str(),
                  nombreModo(r.modo), r.minutos, r.metros / 1000);
    std::cout << resumen;
    Arreglo<Indicacion> lista = e.indicaciones(r);
    for (int i = 0; i < lista.tamano(); i++) {
        char detalle[64];
        std::snprintf(detalle, sizeof(detalle), "  (%.2f km, %.0f min)", lista[i].metros / 1000, lista[i].minutos);
        std::cout << "   " << (i + 1) << ". " << lista[i].texto << detalle << "\n";
    }
}

static void listarSedes(const Enrutador& e) {
    std::cout << "\n  Sedes:\n";
    for (int i = 0; i < e.numSedes(); i++) {
        std::cout << "   " << (i + 1) << ". " << e.red().vertice(e.sede(i)).nombre << "  (" << e.direccion(i) << ")\n";
    }
}

// Acepta el numero de la lista o el nombre exacto (se busca en la tabla hash)
static int pedirLugar(const Enrutador& e, const std::string& mensaje) {
    std::string texto = leerLinea(mensaje);
    double numero;
    if (aNumero(texto, numero) && numero >= 1 && numero <= e.numSedes()) return e.sede((int)numero - 1);
    int v = e.buscar(texto);
    if (v == -1) std::cout << "  '" << texto << "' no existe.\n";
    return v;
}

static void mostrarHorario(Enrutador& e, const std::string& archivo) {
    Arreglo<Clase> clases;
    std::string informe;
    int n = cargarHorario(archivo, e, clases, informe);
    std::cout << "  " << informe << "\n";
    if (n < 0) return;
    Arreglo<Traslado> traslados = calcularTraslados(e, clases);
    for (int i = 0; i < traslados.tamano(); i++) {
        const Traslado& t = traslados[i];
        const Clase& a = clases[t.claseOrigen];
        const Clase& b = clases[t.claseDestino];
        std::cout << "\n  " << nombreDia(a.dia) << " " << textoHora(a.fin) << " " << e.red().vertice(a.sede).nombre
                  << " -> " << textoHora(b.inicio) << " " << e.red().vertice(b.sede).nombre << "  (" << t.disponible
                  << " min disponibles)\n";
        for (int m = 0; m < NUM_MODOS; m++) {
            char linea[96];
            if (t.minutos[m] >= INFINITO) {
                std::snprintf(linea, sizeof(linea), "     %-13s sin ruta\n", nombreModo((Modo)m));
            } else {
                std::snprintf(linea, sizeof(linea), "     %-13s %4.0f min  %s%s\n", nombreModo((Modo)m), t.minutos[m],
                              t.minutos[m] <= t.disponible ? "a tiempo" : "TARDE",
                              m == t.mejorModo ? "  <- el mas rapido" : "");
            }
            std::cout << linea;
        }
    }
}

static void menuConsola(Enrutador& e, const std::string& carpeta) {
    const std::string archivoEscenario = carpeta + "/escenario.txt";
    while (true) {
        std::cout << "\n=========== RUTAS INTER-SEDES UD (consola) ===========\n"
                  << " 1. Calcular ruta entre sedes (con alternativas)\n"
                  << " 2. Recargar " << archivoEscenario << "\n"
                  << " 3. Cambiar la hora simulada (ahora " << textoHora(e.hora()) << ")\n"
                  << " 4. Listar sedes\n"
                  << " 5. Pruebas de las estructuras\n"
                  << " 6. Horario de clases: traslados entre sedes\n"
                  << " 0. Salir\n";
        std::string opcion = leerLinea("Opcion: ");
        if (opcion == "0") return;
        if (opcion == "1") {
            listarSedes(e);
            int origen = pedirLugar(e, "  Origen (numero o nombre): ");
            if (origen == -1) continue;
            int destino = pedirLugar(e, "  Destino (numero o nombre): ");
            if (destino == -1) continue;
            std::cout << "  Modo: 1 Carro  2 Moto  3 Bicicleta  4 A pie  5 TransMilenio\n";
            double m;
            if (!aNumero(leerLinea("  > "), m) || m < 1 || m > NUM_MODOS) m = 1;
            Ruta rutas[MAX_RUTAS];
            int n = e.calcularAlternativas(origen, destino, (Modo)((int)m - 1), rutas, MAX_RUTAS);
            if (n == 0) imprimirRuta(e, rutas[0]);
            for (int i = 0; i < n; i++) {
                std::cout << "\n  --- " << (i == 0 ? "Ruta mas rapida" : "Alternativa") << " ---\n";
                imprimirRuta(e, rutas[i]);
            }
        } else if (opcion == "2") {
            std::string informe;
            cargarEscenario(archivoEscenario, e, informe);
            std::cout << "  " << informe << "\n";
        } else if (opcion == "3") {
            int minuto;
            if (leerHora(leerLinea("  Nueva hora (HH:MM): "), minuto)) e.cambiarHora(minuto);
            else std::cout << "  Hora invalida.\n";
        } else if (opcion == "4") {
            listarSedes(e);
        } else if (opcion == "5") {
            ejecutarPruebas();
        } else if (opcion == "6") {
            mostrarHorario(e, carpeta + "/horario.txt");
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    std::system("chcp 65001 > nul");   // para ver tildes en la consola de Windows
#endif
    bool consola = false;
    std::string captura;   // --captura archivo.png: guarda una imagen de la ventana y sale (para pruebas)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--pruebas") return ejecutarPruebas() == 0 ? 0 : 1;
        if (arg == "--consola") consola = true;
        if (arg == "--captura" && i + 1 < argc) captura = argv[++i];
    }

    std::string carpeta = buscarCarpetaDatos();
    Enrutador e;
    if (!e.cargar(carpeta)) return 1;

    std::string informe;
    if (cargarEscenario(carpeta + "/escenario.txt", e, informe) >= 0) std::cout << informe << "\n";

    if (consola) menuConsola(e, carpeta);
    else ejecutarMapa(e, carpeta, captura);
    return 0;
}
