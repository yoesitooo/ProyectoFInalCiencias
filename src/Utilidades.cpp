#include "Utilidades.h"
#include <cmath>
#include <sstream>

Arreglo<std::string> separar(const std::string& linea, char separador) {
    Arreglo<std::string> campos;
    std::string actual;
    for (char c : linea) {
        if (c == separador) {
            campos.agregar(actual);
            actual.clear();
        } else if (c != '\r') {
            actual += c;
        }
    }
    campos.agregar(actual);
    return campos;
}

std::string recortar(const std::string& texto) {
    size_t inicio = texto.find_first_not_of(" \t\r");
    if (inicio == std::string::npos) return "";
    size_t fin = texto.find_last_not_of(" \t\r");
    return texto.substr(inicio, fin - inicio + 1);
}

bool aNumero(const std::string& texto, double& resultado) {
    std::istringstream entrada(recortar(texto));
    entrada >> resultado;
    return !entrada.fail() && entrada.eof();
}

bool leerHora(const std::string& texto, int& minuto) {
    std::string t = recortar(texto);
    size_t dosPuntos = t.find(':');
    double h, m = 0;
    if (dosPuntos == std::string::npos) {
        if (!aNumero(t, h)) return false;
    } else if (!aNumero(t.substr(0, dosPuntos), h) || !aNumero(t.substr(dosPuntos + 1), m)) {
        return false;
    }
    if (h < 0 || h > 24 || m < 0 || m >= 60) return false;
    minuto = (int)h * 60 + (int)m;
    return minuto <= 24 * 60;
}

bool leerFranja(const std::string& texto, int& desde, int& hasta) {
    size_t guion = texto.find('-');
    if (guion == std::string::npos) return false;
    return leerHora(texto.substr(0, guion), desde) && leerHora(texto.substr(guion + 1), hasta) && desde < hasta;
}

bool esComentario(const std::string& linea) {
    std::string t = recortar(linea);
    return t.empty() || t[0] == '#';
}

double distancia(Punto a, Punto b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Distancia del punto p al segmento a-b: se proyecta p sobre la recta a-b y se
// recorta el parametro t a [0, 1] para quedarse dentro del segmento.
double distanciaASegmento(Punto p, Punto a, Punto b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double largo2 = dx * dx + dy * dy;
    if (largo2 == 0) return distancia(p, a);
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / largo2;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    Punto cercano = {(float)(a.x + t * dx), (float)(a.y + t * dy)};
    return distancia(p, cercano);
}
