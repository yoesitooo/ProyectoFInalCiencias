// Contingencias: marchas, bloqueos, vias cerradas o congestionadas y estaciones cerradas.
//
// Idea: el estado de la red NUNCA se modifica "a mano". Se guarda una lista de eventos
// (los de escenario.txt y los que se ponen con clic en el mapa) y aplicarEventos()
// reconstruye el estado desde cero: restaura la red y aplica cada evento activo a la hora
// simulada (un evento puede tener franja horaria, p. ej. una marcha de 10:00 a 13:00).
// Asi recargar el archivo o quitar un evento siempre deja la red consistente.
#include "Enrutador.h"
#include "Utilidades.h"

void Enrutador::agregarEvento(const Evento& e) {
    eventos.agregar(e);
    aplicarEventos();
}

void Enrutador::quitarEvento(int i) {
    if (i < 0 || i >= eventos.tamano()) return;
    eventos.quitar(i);
    aplicarEventos();
}

void Enrutador::quitarEventos(bool delArchivo) {
    for (int i = eventos.tamano() - 1; i >= 0; i--) {
        if (eventos[i].desdeArchivo == delArchivo) eventos.quitar(i);
    }
    aplicarEventos();
}

// Marca con la contingencia cada tramo cuya forma pasa por el circulo de la zona
void Enrutador::aplicarZona(const Evento& e) {
    Contingencia tipo = (e.tipo == EV_BLOQUEO) ? BLOQUEO : MARCHA;
    double radio = e.radioMetros / METROS_POR_PIXEL;   // radio en pixeles del mapa

    for (int u = 0; u < grafo.numVertices(); u++) {
        for (Arista* a = grafo.vertice(u).primera; a != nullptr; a = a->siguiente) {
            // Las uniones con las sedes y estaciones no se bloquean (si no, la sede quedaria aislada)
            if (a->tipo == ACCESO_SEDE || a->tipo == TM_ENTRADA || a->tipo == TM_SALIDA || a->tipo == TM_ABORDAR || a->tipo == TM_BAJAR) continue;
            if (a->contingencia >= tipo) continue;
            // Descarte rapido: todo el tramo esta a menos de su longitud de su vertice inicial
            double largo = a->metros / METROS_POR_PIXEL;
            if (distancia(e.centro, grafo.vertice(u).pos) > radio + largo + 1) continue;

            for (int k = 0; k + 1 < a->puntosTrazado; k++) {
                Punto p1 = grafo.punto(a->inicioTrazado + k);
                Punto p2 = grafo.punto(a->inicioTrazado + k + 1);
                if (distanciaASegmento(e.centro, p1, p2) <= radio) {
                    a->contingencia = tipo;
                    break;
                }
            }
        }
    }
}

// Un evento sin franja (desde = -1) dura todo el dia; si no, solo existe entre 'desde' y 'hasta'
bool Enrutador::eventoActivo(const Evento& e) const {
    if (e.desde == -1) return true;
    return minutoDelDia >= e.desde && minutoDelDia < e.hasta;
}

int Enrutador::aplicarEventos() {
    grafo.restaurar();
    for (int i = 0; i < eventos.tamano(); i++) {
        const Evento& e = eventos[i];
        if (!eventoActivo(e)) continue;
        if (e.tipo == EV_MARCHA || e.tipo == EV_BLOQUEO) {
            aplicarZona(e);
        } else if (e.tipo == EV_CERRAR_LUGAR) {
            grafo.vertice(e.vertice).activo = false;
        } else {
            // cerrar_via o congestion: todas las aristas que tienen ese nombre de via
            for (int u = 0; u < grafo.numVertices(); u++) {
                for (Arista* a = grafo.vertice(u).primera; a != nullptr; a = a->siguiente) {
                    if (a->via != e.via) continue;
                    if (e.tipo == EV_CERRAR_VIA) a->cerrada = true;
                    else a->factorTrafico = e.factor;
                }
            }
        }
    }

    int afectadas = 0;
    for (int u = 0; u < grafo.numVertices(); u++) {
        for (Arista* a = grafo.vertice(u).primera; a != nullptr; a = a->siguiente) {
            if (a->cerrada || a->contingencia != NINGUNA || a->factorTrafico != 1.0) afectadas++;
        }
    }
    return afectadas;
}

int Enrutador::zonaEn(Punto p) const {
    for (int i = eventos.tamano() - 1; i >= 0; i--) {
        const Evento& e = eventos[i];
        if ((e.tipo == EV_MARCHA || e.tipo == EV_BLOQUEO) && eventoActivo(e) &&
            distancia(p, e.centro) * METROS_POR_PIXEL <= e.radioMetros) {
            return i;
        }
    }
    return -1;
}
