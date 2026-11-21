#include "Horario.h"
#include <fstream>
#include "Utilidades.h"

static const char* DIAS[6] = {"lunes", "martes", "miercoles", "jueves", "viernes", "sabado"};

const char* nombreDia(int dia) {
    static const char* bonitos[6] = {"Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
    return (dia >= 0 && dia < 6) ? bonitos[dia] : "?";
}

static int diaDesdeTexto(const std::string& texto) {
    std::string t;
    for (char c : texto) t += (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);   // a minusculas
    if (t == "miércoles") t = "miercoles";
    if (t == "sábado") t = "sabado";
    for (int i = 0; i < 6; i++) {
        if (t == DIAS[i]) return i;
    }
    return -1;
}

// true si la clase a va antes que la b (primero por dia, luego por hora de inicio)
static bool vaAntes(const Clase& a, const Clase& b) {
    if (a.dia != b.dia) return a.dia < b.dia;
    return a.inicio < b.inicio;
}

// Ordenamiento por insercion: O(n^2), suficiente para las pocas clases de un horario
static void ordenarClases(Arreglo<Clase>& clases) {
    for (int i = 1; i < clases.tamano(); i++) {
        Clase actual = clases[i];
        int j = i - 1;
        while (j >= 0 && vaAntes(actual, clases[j])) {
            clases[j + 1] = clases[j];
            j--;
        }
        clases[j + 1] = actual;
    }
}

int cargarHorario(const std::string& archivo, const Enrutador& e, Arreglo<Clase>& clases, std::string& informe) {
    clases.limpiar();
    std::ifstream f(archivo);
    if (!f) {
        informe = "No existe " + archivo;
        return -1;
    }
    std::string linea, errores;
    int numeroLinea = 0;
    while (std::getline(f, linea)) {
        numeroLinea++;
        if (esComentario(linea)) continue;
        Arreglo<std::string> c = separar(linea, ';');
        for (int i = 0; i < c.tamano(); i++) c[i] = recortar(c[i]);

        std::string error;
        Clase clase;
        if (c.tamano() < 4) error = "formato: dia; inicio; fin; sede; materia";
        else if ((clase.dia = diaDesdeTexto(c[0])) == -1) error = "dia invalido '" + c[0] + "'";
        else if (!leerHora(c[1], clase.inicio) || !leerHora(c[2], clase.fin) || clase.inicio >= clase.fin)
            error = "horas invalidas";
        else if ((clase.sede = e.buscar(c[3])) == -1) error = "no existe la sede '" + c[3] + "'";

        if (!error.empty()) {
            errores += "  linea " + std::to_string(numeroLinea) + ": " + error + "\n";
            continue;
        }
        clase.materia = (c.tamano() > 4) ? c[4] : "";
        clases.agregar(clase);
    }
    ordenarClases(clases);
    informe = "horario: " + std::to_string(clases.tamano()) + " clases";
    if (!errores.empty()) informe += "\n" + errores;
    return clases.tamano();
}

Arreglo<Traslado> calcularTraslados(Enrutador& e, const Arreglo<Clase>& clases) {
    Arreglo<Traslado> traslados;
    int horaGuardada = e.hora();

    for (int i = 0; i + 1 < clases.tamano(); i++) {
        const Clase& a = clases[i];
        const Clase& b = clases[i + 1];
        if (a.dia != b.dia || a.sede == b.sede) continue;   // otro dia o misma sede: no hay traslado

        Traslado t;
        t.claseOrigen = i;
        t.claseDestino = i + 1;
        t.disponible = b.inicio - a.fin;
        t.mejorModo = MODO_CARRO;
        e.cambiarHora(a.fin);   // se sale al terminar la clase: trafico y contingencias de esa hora
        for (int m = 0; m < NUM_MODOS; m++) {
            Ruta r = e.calcularRuta(a.sede, b.sede, (Modo)m);
            t.minutos[m] = r.encontrada ? r.minutos : INFINITO;
            if (t.minutos[m] < t.minutos[t.mejorModo]) t.mejorModo = m;
        }
        traslados.agregar(t);
    }
    e.cambiarHora(horaGuardada);
    return traslados;
}
