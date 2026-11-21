#include "Escenario.h"
#include <filesystem>
#include <fstream>
#include "Utilidades.h"

// Interpreta un lugar: "x,y" (coordenadas del mapa) o el nombre de una sede/estacion
static bool leerLugar(const std::string& texto, const Enrutador& e, Punto& resultado) {
    size_t coma = texto.find(',');
    double x, y;
    if (coma != std::string::npos && aNumero(texto.substr(0, coma), x) && aNumero(texto.substr(coma + 1), y)) {
        resultado.x = (float)x;
        resultado.y = (float)y;
        return true;
    }
    int v = e.buscar(texto);
    if (v == -1) return false;
    resultado = e.red().vertice(v).pos;
    return true;
}

static Evento eventoVacio(TipoEvento tipo) {
    Evento ev;
    ev.tipo = tipo;
    ev.centro.x = 0;
    ev.centro.y = 0;
    ev.radioMetros = 0;
    ev.via = -1;
    ev.factor = 1;
    ev.vertice = -1;
    ev.desde = -1;
    ev.hasta = -1;
    ev.desdeArchivo = true;
    return ev;
}

// Si la orden trae un campo extra en la posicion 'i', debe ser la franja horaria "HH:MM-HH:MM"
static std::string leerFranjaOpcional(const Arreglo<std::string>& c, int i, Evento& ev) {
    if (c.tamano() <= i) return "";
    if (!leerFranja(c[i], ev.desde, ev.hasta)) return "franja horaria invalida '" + c[i] + "' (use 10:00-13:00)";
    return "";
}

// Convierte una linea en un evento. Devuelve "" si todo salio bien o el mensaje de error.
static std::string interpretarLinea(const Arreglo<std::string>& c, Enrutador& e, Evento& ev, bool& esEvento) {
    std::string orden = c[0];
    esEvento = true;

    if (orden == "hora") {
        esEvento = false;
        int minuto;
        if (c.tamano() < 2 || !leerHora(c[1], minuto)) return "hora invalida (use por ejemplo 07:30)";
        e.cambiarHora(minuto);
        return "";
    }
    if (orden == "marcha" || orden == "bloqueo") {
        ev = eventoVacio(orden == "marcha" ? EV_MARCHA : EV_BLOQUEO);
        if (c.tamano() < 3) return "formato: " + orden + "; lugar; radio";
        if (!leerLugar(c[1], e, ev.centro)) return "no existe el lugar '" + c[1] + "'";
        if (!aNumero(c[2], ev.radioMetros) || ev.radioMetros <= 0) return "radio invalido '" + c[2] + "'";
        return leerFranjaOpcional(c, 3, ev);
    }
    if (orden == "cerrar_via" || orden == "congestion") {
        ev = eventoVacio(orden == "cerrar_via" ? EV_CERRAR_VIA : EV_CONGESTION);
        if (c.tamano() < 2) return "falta el nombre de la via";
        ev.via = e.red().buscarVia(c[1]);                     // tabla hash de nombres de via
        if (ev.via == -1) return "no existe la via '" + c[1] + "'";
        if (orden == "cerrar_via") return leerFranjaOpcional(c, 2, ev);
        if (c.tamano() < 3 || !aNumero(c[2], ev.factor) || ev.factor < 1) return "factor invalido (use 2, 3, ...)";
        return leerFranjaOpcional(c, 3, ev);
    }
    if (orden == "cerrar_estacion") {
        ev = eventoVacio(EV_CERRAR_LUGAR);
        if (c.tamano() < 2) return "falta el nombre";
        ev.vertice = e.buscar(c[1]);
        if (ev.vertice == -1) return "no existe '" + c[1] + "'";
        return leerFranjaOpcional(c, 2, ev);
    }
    return "orden desconocida '" + orden + "'";
}

int cargarEscenario(const std::string& archivo, Enrutador& e, std::string& informe) {
    std::ifstream f(archivo);
    if (!f) {
        informe = "No existe " + archivo;
        return -1;
    }

    // Se descartan los eventos anteriores del archivo (los del mapa se conservan)
    for (int i = e.numEventos() - 1; i >= 0; i--) {
        if (e.evento(i).desdeArchivo) e.quitarEvento(i);
    }

    std::string linea, errores;
    int numeroLinea = 0, aplicadas = 0;
    while (std::getline(f, linea)) {
        numeroLinea++;
        if (esComentario(linea)) continue;
        Arreglo<std::string> campos = separar(linea, ';');
        for (int i = 0; i < campos.tamano(); i++) campos[i] = recortar(campos[i]);
        while (campos.tamano() > 1 && campos[campos.tamano() - 1].empty()) campos.quitar(campos.tamano() - 1);

        Evento ev;
        bool esEvento = false;
        std::string error = interpretarLinea(campos, e, ev, esEvento);
        if (!error.empty()) {
            errores += "  linea " + std::to_string(numeroLinea) + ": " + error + "\n";
            continue;
        }
        if (esEvento) e.agregarEvento(ev);
        aplicadas++;
    }
    e.aplicarEventos();
    informe = "escenario: " + std::to_string(aplicadas) + " ordenes aplicadas";
    if (!errores.empty()) informe += "\n" + errores;
    return aplicadas;
}

long long fechaModificacion(const std::string& archivo) {
    std::error_code error;
    std::filesystem::file_time_type t = std::filesystem::last_write_time(archivo, error);
    if (error) return -1;
    return (long long)t.time_since_epoch().count();
}
