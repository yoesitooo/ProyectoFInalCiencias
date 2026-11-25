// Interfaz grafica con raylib: mapa, sedes, rutas, contingencias, reloj del dia y horario.
#include "Mapa.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "Escenario.h"
#include "Horario.h"
#include "raylib.h"

// ---------------------------------------------------------------------------
// Apariencia
// ---------------------------------------------------------------------------
static const int ANCHO_IZQ = 330;
static const int ANCHO_DER = 370;
static const int HORA_GRAFICA_INICIO = 5;    // la grafica muestra de 5:00 a 22:00
static const int HORA_GRAFICA_FIN = 22;

static const Color COLOR_MODO[NUM_MODOS] = {
    {37, 99, 235, 255},   // carro: azul
    {147, 51, 234, 255},  // moto: morado
    {22, 163, 74, 255},   // bici: verde
    {234, 88, 12, 255},   // a pie: naranja
    {200, 16, 46, 255},   // TransMilenio: rojo
};
static const char* ETIQUETA_MODO[NUM_MODOS] = {"Carro", "Moto", "Bici", "A pie", "TransMi"};

static const Color FONDO_PANEL = {241, 245, 249, 255};
static const Color BORDE = {203, 213, 225, 255};
static const Color TEXTO = {30, 41, 59, 255};
static const Color TEXTO_SUAVE = {90, 105, 125, 255};
static const Color BOTON = {222, 229, 239, 255};
static const Color BOTON_BORDE = {170, 184, 204, 255};
static const Color BOTON_ENCIMA = {199, 210, 225, 255};
static const Color OSCURO = {51, 65, 85, 255};
static const Color COLOR_UD = {165, 28, 48, 255};
static const Color COLOR_ECCI = {0, 92, 169, 255};
static const Color COLOR_MARCHA = {245, 158, 11, 255};
static const Color COLOR_BLOQUEO = {220, 38, 38, 255};
static const Color COLOR_CICLO = {22, 163, 74, 255};
static const Color COLOR_TM = {200, 16, 46, 255};
static const Color VERDE = {22, 163, 74, 255};
static const Color ROJO = {220, 38, 38, 255};

enum Herramienta { NAVEGAR, PONER_MARCHA, PONER_BLOQUEO, CERRAR_ESTACION };
enum Pestana { PESTANA_RUTA, PESTANA_HORARIO };

// Un tramo para dibujar: posicion de su forma en Grafo::puntos
struct TramoDibujo {
    int inicio;
    int cantidad;
    Contingencia estado;
};

static Vector2 aVector(Punto p) {
    Vector2 v = {p.x, p.y};
    return v;
}

static Punto aPunto(Vector2 v) {
    Punto p = {v.x, v.y};
    return p;
}

// ---------------------------------------------------------------------------
class Mapa {
public:
    Mapa(Enrutador& e, const std::string& carpeta);
    void ejecutar(const std::string& captura);

private:
    Enrutador& e;
    std::string carpeta;
    std::string archivoEscenario, archivoHorario;
    Texture2D textura;
    bool hayMapa;
    Font fuente;
    bool fuentePropia;
    Camera2D camara;

    Modo modo;
    int origen, destino;
    int campoActivo;            // 0 = se esta eligiendo el origen, 1 = el destino
    bool alternativas, verCiclo, verTM;
    Herramienta herramienta;
    float radio;                // radio de la proxima protesta, en metros
    Pestana pestana;

    // Resultado de la ruta
    Ruta rutas[MAX_RUTAS];
    int numRutas, rutaElegida;
    Arreglo<Indicacion> pasos;
    bool hayQueRecalcular;
    double grafica[24];         // minutos de la ruta segun la hora de salida
    bool graficaSucia;

    // Reloj del dia
    bool simulando;
    double ultimoPaso;

    // Horario de clases
    Arreglo<Clase> clases;
    Arreglo<Traslado> traslados;
    bool horarioSucio;

    Arreglo<TramoDibujo> capaCiclo, capaTM, afectados;
    float scrollSedes, scrollPasos, scrollHorario;
    int sedeBajoMouse, estacionBajoMouse;
    bool arrastrando;
    Vector2 inicioClic;
    std::string aviso;
    double avisoDesde;
    std::string informeEscenario, informeHorario;
    long long fechaEscenario, fechaHorario;
    double ultimaRevision;

    // Preparacion y calculo
    void prepararCapas();
    void actualizarAfectados();
    void encuadrarSedes();
    void cargarFuente();
    void recalcular();
    void calcularGrafica();
    void elegirRuta(int i);
    void asignarSede(int vertice, int campo);
    void cambiarHora(int minuto);
    void contingenciasCambiaron();
    void alternarEstacion(int vertice);
    void avisar(const std::string& texto);
    void revisarArchivos(bool forzar);

    // Dibujo
    void texto(const char* s, float x, float y, float tam, Color c);
    float anchoTexto(const char* s, float tam);
    float textoAjustado(const std::string& s, float x, float y, float ancho, float tam, Color c);
    bool boton(Rectangle r, const char* s, bool activo, Color acento, Color textoInactivo = TEXTO);
    void dibujarTramo(int inicio, int cantidad, float grosor, Color c, bool redondear);
    void dibujarRuta(const Ruta& r, bool elegida);
    void dibujarMundo();
    void dibujarMarcadores();
    void panelIzquierdo();
    void panelDerecho();
    void pestanaRuta(float x, float y, float w, float alto);
    void pestanaHorario(float x, float y, float w, float alto);
    void dibujarGrafica(float x, float y, float w);

    // Interaccion
    Rectangle areaMapa() const;
    void manejarMapa();
    void manejarTeclado();
};

// ---------------------------------------------------------------------------
// Preparacion
// ---------------------------------------------------------------------------
Mapa::Mapa(Enrutador& enrutador, const std::string& carpetaDatos)
    : e(enrutador), carpeta(carpetaDatos), hayMapa(false), fuentePropia(false), modo(MODO_CARRO), origen(-1),
      destino(-1), campoActivo(0), alternativas(true), verCiclo(false), verTM(false), herramienta(NAVEGAR),
      radio(400), pestana(PESTANA_RUTA), numRutas(0), rutaElegida(0), hayQueRecalcular(true), graficaSucia(true),
      simulando(false), ultimoPaso(0), horarioSucio(true), scrollSedes(0), scrollPasos(0), scrollHorario(0),
      sedeBajoMouse(-1), estacionBajoMouse(-1), arrastrando(false), avisoDesde(0), fechaEscenario(-1), fechaHorario(-1),
      ultimaRevision(0) {
    archivoEscenario = carpeta + "/escenario.txt";
    archivoHorario = carpeta + "/horario.txt";
    inicioClic.x = -1;
    inicioClic.y = -1;
    for (int h = 0; h < 24; h++) grafica[h] = INFINITO;
    // Ruta de ejemplo al abrir: Ingenieria (Calle 40) -> Facultad Tecnologica
    if (e.numSedes() > 8) {
        origen = e.sede(2);
        destino = e.sede(8);
    }
}

void Mapa::cargarFuente() {
    // Fuente del sistema con tildes y enes; si no existe se usa la de raylib
    const char* candidatas[] = {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    const char* letras = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                         "abcdefghijklmnopqrstuvwxyz{|}~áéíóúÁÉÍÓÚñÑüÜ¿¡°·©▶■←→";
    int cantidad = 0;
    int* codigos = LoadCodepoints(letras, &cantidad);
    fuente = GetFontDefault();
    for (const char* ruta : candidatas) {
        if (FileExists(ruta)) {
            fuente = LoadFontEx(ruta, 40, codigos, cantidad);
            SetTextureFilter(fuente.texture, TEXTURE_FILTER_BILINEAR);
            fuentePropia = true;
            break;
        }
    }
    UnloadCodepoints(codigos);
}

// Una arista de doble sentido aparece dos veces en el grafo; se dibuja una sola
static bool esPrimeraDeSuPar(const Grafo& g, int u, const Arista* a) {
    return u < a->destino || g.buscarArista(a->destino, u) == nullptr;
}

void Mapa::prepararCapas() {
    const Grafo& g = e.red();
    TablaHash formasTM;   // varios servicios comparten la misma forma: se dibuja una sola vez
    for (int u = 0; u < g.numVertices(); u++) {
        for (const Arista* a = g.vertice(u).primera; a != nullptr; a = a->siguiente) {
            TramoDibujo t = {a->inicioTrazado, a->puntosTrazado, NINGUNA};
            if (a->tipo == TM_BUS) {
                std::string clave = std::to_string(a->inicioTrazado);
                if (formasTM.buscar(clave) == -1) {
                    formasTM.insertar(clave, 1);
                    capaTM.agregar(t);
                }
            } else if (esPrimeraDeSuPar(g, u, a) &&
                       (a->tipo == CICLORRUTA || (a->ciclorruta && a->tipo <= TERCIARIA))) {
                capaCiclo.agregar(t);
            }
        }
    }
}

void Mapa::actualizarAfectados() {
    const Grafo& g = e.red();
    afectados.limpiar();
    for (int u = 0; u < g.numVertices(); u++) {
        for (const Arista* a = g.vertice(u).primera; a != nullptr; a = a->siguiente) {
            bool afectada = a->cerrada || a->contingencia != NINGUNA || a->factorTrafico > 1;
            if (!afectada || a->tipo == ACCESO_SEDE || !esPrimeraDeSuPar(g, u, a)) continue;
            Contingencia estado = a->contingencia;
            if (a->cerrada) estado = BLOQUEO;
            else if (estado == NINGUNA) estado = MARCHA;   // congestion: se pinta como marcha
            TramoDibujo t = {a->inicioTrazado, a->puntosTrazado, estado};
            afectados.agregar(t);
        }
    }
}

Rectangle Mapa::areaMapa() const {
    Rectangle r = {(float)ANCHO_IZQ, 0, (float)(GetScreenWidth() - ANCHO_IZQ - ANCHO_DER), (float)GetScreenHeight()};
    return r;
}

void Mapa::encuadrarSedes() {
    Rectangle area = areaMapa();
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < e.numSedes(); i++) {
        Punto p = e.red().vertice(e.sede(i)).pos;
        minX = fminf(minX, p.x);
        maxX = fmaxf(maxX, p.x);
        minY = fminf(minY, p.y);
        maxY = fmaxf(maxY, p.y);
    }
    float margen = 120;
    float zoomX = area.width / (maxX - minX + 2 * margen);
    float zoomY = area.height / (maxY - minY + 2 * margen);
    camara.zoom = fminf(zoomX, zoomY);
    camara.target.x = (minX + maxX) / 2;
    camara.target.y = (minY + maxY) / 2;
    camara.offset.x = area.x + area.width / 2;
    camara.offset.y = area.y + area.height / 2;
    camara.rotation = 0;
}

// ---------------------------------------------------------------------------
// Calculo
// ---------------------------------------------------------------------------
void Mapa::recalcular() {
    hayQueRecalcular = false;
    numRutas = 0;
    rutaElegida = 0;
    scrollPasos = 0;
    pasos.limpiar();
    if (origen == -1 || destino == -1 || origen == destino) return;
    numRutas = e.calcularAlternativas(origen, destino, modo, rutas, alternativas ? MAX_RUTAS : 1);
    if (numRutas > 0) pasos = e.indicaciones(rutas[0]);
    if (graficaSucia) calcularGrafica();
}

// Tiempo de la ruta actual saliendo a cada hora del dia (incluye trafico y eventos de esa hora)
void Mapa::calcularGrafica() {
    graficaSucia = false;
    for (int h = 0; h < 24; h++) grafica[h] = INFINITO;
    if (origen == -1 || destino == -1 || origen == destino) return;
    for (int h = HORA_GRAFICA_INICIO; h <= HORA_GRAFICA_FIN; h++) {
        grafica[h] = e.minutosRutaALas(origen, destino, modo, h * 60);
    }
}

void Mapa::elegirRuta(int i) {
    rutaElegida = i;
    scrollPasos = 0;
    pasos = e.indicaciones(rutas[i]);
}

void Mapa::asignarSede(int vertice, int campo) {
    if (campo == 0) origen = vertice;
    else destino = vertice;
    campoActivo = (campo == 0) ? 1 : 0;
    hayQueRecalcular = true;
    graficaSucia = true;
}

void Mapa::cambiarHora(int minuto) {
    e.cambiarHora(minuto);
    actualizarAfectados();
    hayQueRecalcular = true;
}

// Algo cambio en los eventos: hay que repintar, recalcular la ruta, la grafica y el horario
void Mapa::contingenciasCambiaron() {
    actualizarAfectados();
    hayQueRecalcular = true;
    graficaSucia = true;
    horarioSucio = true;
}

// Clic en una estacion: si esta abierta se cierra (todo el dia); si se cerro con el mouse se reabre
void Mapa::alternarEstacion(int vertice) {
    const std::string& nombre = e.red().vertice(vertice).nombre;
    for (int i = 0; i < e.numEventos(); i++) {
        const Evento& ev = e.evento(i);
        if (ev.tipo != EV_CERRAR_LUGAR || ev.vertice != vertice || !e.eventoActivo(ev)) continue;
        if (ev.desdeArchivo) {
            avisar("Esa estación está cerrada en escenario.txt: quítela del archivo");
        } else {
            e.quitarEvento(i);
            avisar("Estación abierta: " + nombre);
        }
        return;
    }
    Evento ev;
    ev.tipo = EV_CERRAR_LUGAR;
    ev.centro = e.red().vertice(vertice).pos;
    ev.radioMetros = 0;
    ev.via = -1;
    ev.factor = 1;
    ev.vertice = vertice;
    ev.desde = -1;
    ev.hasta = -1;
    ev.desdeArchivo = false;
    e.agregarEvento(ev);
    avisar("Estación cerrada: " + nombre);
}

void Mapa::avisar(const std::string& textoAviso) {
    aviso = textoAviso;
    avisoDesde = GetTime();
}

// Cada segundo se mira si escenario.txt u horario.txt cambiaron; si cambiaron se vuelven a cargar
void Mapa::revisarArchivos(bool forzar) {
    if (!forzar && GetTime() - ultimaRevision < 1.0) return;
    ultimaRevision = GetTime();

    long long fecha = fechaModificacion(archivoEscenario);
    if (forzar || fecha != fechaEscenario) {
        fechaEscenario = fecha;
        if (fecha == -1) {
            informeEscenario = "No existe " + archivoEscenario;
        } else {
            cargarEscenario(archivoEscenario, e, informeEscenario);
            size_t salto = informeEscenario.find('\n');
            avisar(salto == std::string::npos ? informeEscenario : informeEscenario.substr(0, salto) + " (hay errores)");
        }
        contingenciasCambiaron();
    }

    fecha = fechaModificacion(archivoHorario);
    if (forzar || fecha != fechaHorario) {
        fechaHorario = fecha;
        cargarHorario(archivoHorario, e, clases, informeHorario);
        horarioSucio = true;
    }
    if (horarioSucio) {
        traslados = calcularTraslados(e, clases);
        horarioSucio = false;
    }
}

// ---------------------------------------------------------------------------
// Utilidades de dibujo
// ---------------------------------------------------------------------------
void Mapa::texto(const char* s, float x, float y, float tam, Color c) {
    Vector2 pos = {x, y};
    DrawTextEx(fuente, s, pos, tam, 0.5f, c);
}

float Mapa::anchoTexto(const char* s, float tam) {
    return MeasureTextEx(fuente, s, tam, 0.5f).x;
}

// Escribe el texto partiendolo en varias lineas si no cabe. Devuelve la altura usada.
float Mapa::textoAjustado(const std::string& s, float x, float y, float ancho, float tam, Color c) {
    std::string linea, palabra;
    float alto = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i < s.size() && s[i] != ' ' && s[i] != '\n') {
            palabra += s[i];
            continue;
        }
        std::string prueba = linea.empty() ? palabra : linea + " " + palabra;
        if (!linea.empty() && anchoTexto(prueba.c_str(), tam) > ancho) {
            texto(linea.c_str(), x, y + alto, tam, c);
            alto += tam * 1.2f;
            linea = palabra;
        } else {
            linea = prueba;
        }
        palabra.clear();
        if (i < s.size() && s[i] == '\n' && !linea.empty()) {   // salto de linea explicito
            texto(linea.c_str(), x, y + alto, tam, c);
            alto += tam * 1.2f;
            linea.clear();
        }
    }
    if (!linea.empty()) {
        texto(linea.c_str(), x, y + alto, tam, c);
        alto += tam * 1.2f;
    }
    return alto;
}

bool Mapa::boton(Rectangle r, const char* s, bool activo, Color acento, Color textoInactivo) {
    bool encima = CheckCollisionPointRec(GetMousePosition(), r);
    Color fondo = activo ? acento : (encima ? BOTON_ENCIMA : BOTON);
    DrawRectangleRounded(r, 0.3f, 6, fondo);
    if (!activo) DrawRectangleRoundedLines(r, 0.3f, 6, BOTON_BORDE);
    float tam = 14;
    float w = anchoTexto(s, tam);
    texto(s, r.x + (r.width - w) / 2, r.y + (r.height - tam) / 2, tam, activo ? WHITE : textoInactivo);
    return encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Mapa::dibujarTramo(int inicio, int cantidad, float grosor, Color c, bool redondear) {
    const Grafo& g = e.red();
    for (int k = 0; k + 1 < cantidad; k++) {
        DrawLineEx(aVector(g.punto(inicio + k)), aVector(g.punto(inicio + k + 1)), grosor, c);
        if (redondear && k > 0) DrawCircleV(aVector(g.punto(inicio + k)), grosor / 2, c);
    }
}

// ---------------------------------------------------------------------------
// Dibujo del mapa (coordenadas del mundo = pixeles de mapa.png)
// ---------------------------------------------------------------------------
void Mapa::dibujarRuta(const Ruta& r, bool elegida) {
    float z = camara.zoom;
    float grosor = (elegida ? 6.5f : 4.5f) / z;
    Color base = elegida ? COLOR_MODO[r.modo] : Color{100, 116, 139, 230};
    // Primero un contorno blanco para que la ruta resalte sobre el mapa
    for (int i = 0; i < r.pasos.tamano(); i++) {
        const Arista* a = r.pasos[i].arista;
        if (a->tipo != ACCESO_SEDE) dibujarTramo(a->inicioTrazado, a->puntosTrazado, grosor + 3 / z, WHITE, true);
    }
    for (int i = 0; i < r.pasos.tamano(); i++) {
        const Arista* a = r.pasos[i].arista;
        Color c = base;
        float g = grosor;
        if (a->tipo == ACCESO_SEDE || a->tipo == TM_ENTRADA || a->tipo == TM_SALIDA) g = 2.5f / z;
        else if (elegida && r.modo == MODO_TM && a->tipo != TM_BUS) c = COLOR_MODO[MODO_PIE];
        dibujarTramo(a->inicioTrazado, a->puntosTrazado, g, c, true);
    }
}

void Mapa::dibujarMundo() {
    float z = camara.zoom;
    if (hayMapa) DrawTexture(textura, 0, 0, WHITE);

    if (verCiclo) {
        for (int i = 0; i < capaCiclo.tamano(); i++)
            dibujarTramo(capaCiclo[i].inicio, capaCiclo[i].cantidad, 2.2f / z, Fade(COLOR_CICLO, 0.8f), false);
    }
    if (verTM) {
        for (int i = 0; i < capaTM.tamano(); i++)
            dibujarTramo(capaTM[i].inicio, capaTM[i].cantidad, 3.0f / z, Fade(COLOR_TM, 0.55f), false);
    }

    // Zonas de marcha o bloqueo activas a la hora simulada
    for (int i = 0; i < e.numEventos(); i++) {
        const Evento& ev = e.evento(i);
        if ((ev.tipo != EV_MARCHA && ev.tipo != EV_BLOQUEO) || !e.eventoActivo(ev)) continue;
        Color c = (ev.tipo == EV_BLOQUEO) ? COLOR_BLOQUEO : COLOR_MARCHA;
        float r = (float)(ev.radioMetros / METROS_POR_PIXEL);
        DrawCircleV(aVector(ev.centro), r, Fade(c, 0.18f));
        DrawCircleLinesV(aVector(ev.centro), r, c);
    }
    // Tramos afectados por contingencias
    for (int i = 0; i < afectados.tamano(); i++) {
        Color c = (afectados[i].estado == BLOQUEO) ? COLOR_BLOQUEO : COLOR_MARCHA;
        dibujarTramo(afectados[i].inicio, afectados[i].cantidad, 3.0f / z, c, false);
    }

    // Primero las alternativas, encima la ruta elegida
    for (int i = 0; i < numRutas; i++) {
        if (i != rutaElegida) dibujarRuta(rutas[i], false);
    }
    if (numRutas > 0) dibujarRuta(rutas[rutaElegida], true);
}

// Sedes, estaciones y textos: se dibujan en coordenadas de pantalla (tamano fijo)
void Mapa::dibujarMarcadores() {
    const Grafo& g = e.red();
    Rectangle area = areaMapa();
    Vector2 mouse = GetMousePosition();
    BeginScissorMode((int)area.x, 0, (int)area.width, (int)area.height);

    estacionBajoMouse = -1;
    if ((verTM && camara.zoom > 0.55f) || herramienta == CERRAR_ESTACION) {
        for (int i = 0; i < e.numEstaciones(); i++) {
            const Vertice& v = g.vertice(e.estacion(i));
            Vector2 s = GetWorldToScreen2D(aVector(v.pos), camara);
            DrawCircleV(s, 4.5f, v.activo ? WHITE : Color{120, 120, 120, 255});
            DrawCircleLinesV(s, 4.5f, COLOR_TM);
            if (herramienta == CERRAR_ESTACION && CheckCollisionPointRec(mouse, area) &&
                CheckCollisionPointCircle(mouse, s, 8)) estacionBajoMouse = e.estacion(i);
            if (camara.zoom > 1.6f || estacionBajoMouse == e.estacion(i)) texto(v.nombre.c_str(), s.x + 7, s.y - 7, 13, COLOR_TM);
        }
    }

    sedeBajoMouse = -1;
    for (int i = 0; i < e.numSedes(); i++) {
        int id = e.sede(i);
        const Vertice& v = g.vertice(id);
        Vector2 s = GetWorldToScreen2D(aVector(v.pos), camara);
        Color c = (v.nombre.find("ECCI") != std::string::npos) ? COLOR_ECCI : COLOR_UD;
        if (CheckCollisionPointRec(mouse, area) && CheckCollisionPointCircle(mouse, s, 11)) sedeBajoMouse = i;
        float radioSede = 7;
        if (id == origen || id == destino) {
            DrawCircleV(s, 13, id == origen ? VERDE : ROJO);
            radioSede = 8;
        }
        DrawCircleV(s, radioSede + 2, WHITE);
        DrawCircleV(s, radioSede, c);
        if (camara.zoom > 0.9f || id == origen || id == destino || sedeBajoMouse == i) {
            float w = anchoTexto(v.nombre.c_str(), 14);
            Rectangle caja = {s.x + 12, s.y - 10, w + 10, 20};
            DrawRectangleRounded(caja, 0.4f, 6, Fade(WHITE, 0.9f));
            texto(v.nombre.c_str(), caja.x + 5, caja.y + 3, 14, c);
        }
    }
    if (sedeBajoMouse != -1) {
        Vector2 s = GetWorldToScreen2D(aVector(g.vertice(e.sede(sedeBajoMouse)).pos), camara);
        const std::string& dir = e.direccion(sedeBajoMouse);
        float w = anchoTexto(dir.c_str(), 13) + 12;
        Rectangle caja = {s.x + 12, s.y + 12, w, 20};
        DrawRectangleRounded(caja, 0.4f, 6, Fade(TEXTO, 0.88f));
        texto(dir.c_str(), s.x + 18, s.y + 15, 13, WHITE);
    }

    // Circulo de la protesta que se va a poner
    if ((herramienta == PONER_MARCHA || herramienta == PONER_BLOQUEO) && CheckCollisionPointRec(mouse, area)) {
        Color c = (herramienta == PONER_BLOQUEO) ? COLOR_BLOQUEO : COLOR_MARCHA;
        DrawCircleLinesV(mouse, (float)(radio / METROS_POR_PIXEL) * camara.zoom, c);
    }

    // Reloj grande sobre el mapa
    std::string hora = textoHora(e.hora());
    Rectangle reloj = {area.x + 12, 12, 92, 34};
    DrawRectangleRounded(reloj, 0.4f, 8, Fade(OSCURO, 0.9f));
    texto(hora.c_str(), reloj.x + (reloj.width - anchoTexto(hora.c_str(), 24)) / 2, reloj.y + 5, 24, WHITE);
    if (simulando) texto("simulando...", reloj.x + 100, reloj.y + 10, 14, OSCURO);

    // Coordenadas bajo el mouse (sirven para agregar sedes en sedes.csv) y creditos del mapa
    char linea[96];
    Vector2 mundo = GetScreenToWorld2D(mouse, camara);
    std::snprintf(linea, sizeof(linea), "x=%.0f  y=%.0f", mundo.x, mundo.y);
    DrawRectangle((int)area.x, (int)area.height - 20, 120, 20, Fade(WHITE, 0.8f));
    texto(linea, area.x + 6, area.height - 17, 12, TEXTO_SUAVE);
    const char* creditos = "Mapa: © OpenStreetMap contributors";
    float w = anchoTexto(creditos, 12);
    DrawRectangle((int)(area.x + area.width - w - 12), (int)area.height - 20, (int)w + 12, 20, Fade(WHITE, 0.8f));
    texto(creditos, area.x + area.width - w - 6, area.height - 17, 12, TEXTO_SUAVE);

    if (!aviso.empty() && GetTime() - avisoDesde < 4) {
        float wa = anchoTexto(aviso.c_str(), 15) + 20;
        Rectangle caja = {area.x + (area.width - wa) / 2, 12, wa, 28};
        DrawRectangleRounded(caja, 0.5f, 8, Fade(TEXTO, 0.85f));
        texto(aviso.c_str(), caja.x + 10, caja.y + 6, 15, WHITE);
    }
    EndScissorMode();
}

// ---------------------------------------------------------------------------
// Interaccion con el mapa
// ---------------------------------------------------------------------------
void Mapa::manejarMapa() {
    Rectangle area = areaMapa();
    Vector2 mouse = GetMousePosition();
    bool sobreMapa = CheckCollisionPointRec(mouse, area);

    // Zoom con la rueda, centrado en el mouse
    float rueda = GetMouseWheelMove();
    if (sobreMapa && rueda != 0) {
        camara.target = GetScreenToWorld2D(mouse, camara);
        camara.offset = mouse;
        camara.zoom *= (rueda > 0) ? 1.18f : 1 / 1.18f;
        camara.zoom = fmaxf(0.12f, fminf(camara.zoom, 8.0f));
    }

    // Arrastrar mueve el mapa; un clic sin arrastrar es una accion
    if (sobreMapa && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        inicioClic = mouse;
        arrastrando = false;
    }
    bool clicEmpezoEnMapa = inicioClic.x >= area.x;
    if (clicEmpezoEnMapa && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float dx = mouse.x - inicioClic.x, dy = mouse.y - inicioClic.y;
        if (dx * dx + dy * dy > 25) arrastrando = true;
        if (arrastrando) {
            Vector2 delta = GetMouseDelta();
            camara.target.x -= delta.x / camara.zoom;
            camara.target.y -= delta.y / camara.zoom;
        }
    }
    if (clicEmpezoEnMapa && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        bool fueClic = !arrastrando && sobreMapa;
        inicioClic.x = -1;
        arrastrando = false;
        if (fueClic) {
            if (herramienta == NAVEGAR) {
                if (sedeBajoMouse != -1) asignarSede(e.sede(sedeBajoMouse), campoActivo);
            } else if (herramienta == CERRAR_ESTACION) {
                if (estacionBajoMouse != -1) {
                    alternarEstacion(estacionBajoMouse);
                    contingenciasCambiaron();
                }
            } else {
                Punto p = aPunto(GetScreenToWorld2D(mouse, camara));
                int zona = e.zonaEn(p);
                if (zona != -1) {                        // clic dentro de una zona: se quita
                    if (e.evento(zona).desdeArchivo) {
                        avisar("Esa zona viene de escenario.txt: quítela del archivo");
                    } else {
                        e.quitarEvento(zona);
                        avisar("Zona retirada");
                    }
                } else {
                    Evento ev;
                    ev.tipo = (herramienta == PONER_BLOQUEO) ? EV_BLOQUEO : EV_MARCHA;
                    ev.centro = p;
                    ev.radioMetros = radio;
                    ev.via = -1;
                    ev.factor = 1;
                    ev.vertice = -1;
                    ev.desde = -1;               // las zonas puestas con el mouse duran todo el dia
                    ev.hasta = -1;
                    ev.desdeArchivo = false;
                    e.agregarEvento(ev);
                    char msg[96];
                    std::snprintf(msg, sizeof(msg), "%s de %.0f m en x=%.0f y=%.0f",
                                  ev.tipo == EV_BLOQUEO ? "Bloqueo" : "Marcha", radio, p.x, p.y);
                    avisar(msg);
                }
                contingenciasCambiaron();
            }
        }
    }
    if (sobreMapa && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && sedeBajoMouse != -1) {
        asignarSede(e.sede(sedeBajoMouse), 1);
    }
}

void Mapa::manejarTeclado() {
    for (int i = 0; i < NUM_MODOS; i++) {
        if (IsKeyPressed(KEY_ONE + i)) {
            modo = (Modo)i;
            hayQueRecalcular = true;
            graficaSucia = true;
        }
    }
    if (IsKeyPressed(KEY_A)) {
        alternativas = !alternativas;
        hayQueRecalcular = true;
    }
    if (IsKeyPressed(KEY_C)) verCiclo = !verCiclo;
    if (IsKeyPressed(KEY_T)) verTM = !verTM;
    if (IsKeyPressed(KEY_N)) herramienta = NAVEGAR;
    if (IsKeyPressed(KEY_M)) herramienta = PONER_MARCHA;
    if (IsKeyPressed(KEY_B)) herramienta = PONER_BLOQUEO;
    if (IsKeyPressed(KEY_E)) herramienta = CERRAR_ESTACION;
    if (IsKeyPressed(KEY_F)) encuadrarSedes();
    if (IsKeyPressed(KEY_R)) revisarArchivos(true);
    if (IsKeyPressed(KEY_LEFT)) cambiarHora(e.hora() - 30);
    if (IsKeyPressed(KEY_RIGHT)) cambiarHora(e.hora() + 30);
    if (IsKeyPressed(KEY_SPACE)) simulando = !simulando;
}

// ---------------------------------------------------------------------------
// Panel izquierdo: modo, sedes, hora, opciones y contingencias
// ---------------------------------------------------------------------------
void Mapa::panelIzquierdo() {
    float alto = (float)GetScreenHeight();
    DrawRectangle(0, 0, ANCHO_IZQ, (int)alto, FONDO_PANEL);
    DrawLine(ANCHO_IZQ - 1, 0, ANCHO_IZQ - 1, (int)alto, BORDE);
    float x = 16, y = 14, w = ANCHO_IZQ - 32;
    Vector2 mouse = GetMousePosition();

    texto("Rutas Inter-Sedes", x, y, 24, COLOR_UD);
    texto("Universidad Distrital · Bogotá", x, y + 28, 14, TEXTO_SUAVE);
    y += 56;

    texto("Modo de transporte", x, y, 15, TEXTO_SUAVE);
    y += 22;
    float bw = (w - 4 * 5) / 5;
    for (int i = 0; i < NUM_MODOS; i++) {
        Rectangle r = {x + i * (bw + 5), y, bw, 30};
        if (boton(r, ETIQUETA_MODO[i], modo == i, COLOR_MODO[i], COLOR_MODO[i])) {
            modo = (Modo)i;
            hayQueRecalcular = true;
            graficaSucia = true;
        }
    }
    y += 42;

    // Campos de origen y destino
    const char* etiquetas[2] = {"Origen", "Destino"};
    int valores[2] = {origen, destino};
    Color colores[2] = {VERDE, ROJO};
    for (int k = 0; k < 2; k++) {
        Rectangle r = {x, y, w - 40, 34};
        bool encima = CheckCollisionPointRec(mouse, r);
        Color fondo = (campoActivo == k) ? Fade(colores[k], 0.15f) : (encima ? BOTON_ENCIMA : WHITE);
        DrawRectangleRounded(r, 0.25f, 6, fondo);
        DrawRectangleRoundedLines(r, 0.25f, 6, campoActivo == k ? colores[k] : BORDE);
        DrawRectangle((int)r.x, (int)r.y + 4, 4, (int)r.height - 8, colores[k]);
        texto(etiquetas[k], r.x + 12, r.y + 2, 12, colores[k]);
        const char* nombre = (valores[k] != -1) ? e.red().vertice(valores[k]).nombre.c_str() : "(elige una sede)";
        texto(nombre, r.x + 12, r.y + 16, 15, valores[k] != -1 ? TEXTO : TEXTO_SUAVE);
        if (encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) campoActivo = k;
        y += 40;
    }
    Rectangle intercambiar = {x + w - 34, y - 60, 34, 34};
    if (boton(intercambiar, "<>", false, TEXTO)) {
        int t = origen;
        origen = destino;
        destino = t;
        hayQueRecalcular = true;
        graficaSucia = true;
    }
    y += 4;

    // Lista de sedes (con desplazamiento)
    texto(campoActivo == 0 ? "Sedes  (clic = origen)" : "Sedes  (clic = destino)", x, y, 15, TEXTO_SUAVE);
    y += 22;
    float altoLista = fmaxf(100, alto - y - 360);
    Rectangle lista = {x, y, w, altoLista};
    DrawRectangleRec(lista, WHITE);
    DrawRectangleLinesEx(lista, 1, BORDE);
    float altoItem = 26;
    if (CheckCollisionPointRec(mouse, lista)) scrollSedes -= GetMouseWheelMove() * 40;
    scrollSedes = fmaxf(0, fminf(scrollSedes, e.numSedes() * altoItem - altoLista));
    BeginScissorMode((int)lista.x, (int)lista.y, (int)lista.width, (int)lista.height);
    for (int i = 0; i < e.numSedes(); i++) {
        int id = e.sede(i);
        Rectangle item = {lista.x + 1, lista.y + i * altoItem - scrollSedes, lista.width - 2, altoItem};
        bool encima = CheckCollisionPointRec(mouse, item) && CheckCollisionPointRec(mouse, lista);
        if (id == origen) DrawRectangleRec(item, Fade(VERDE, 0.18f));
        else if (id == destino) DrawRectangleRec(item, Fade(ROJO, 0.18f));
        else if (encima) DrawRectangleRec(item, BOTON);
        const std::string& nombre = e.red().vertice(id).nombre;
        Color c = (nombre.find("ECCI") != std::string::npos) ? COLOR_ECCI : COLOR_UD;
        DrawCircle((int)item.x + 12, (int)(item.y + altoItem / 2), 4, c);
        texto(nombre.c_str(), item.x + 22, item.y + 5, 15, TEXTO);
        if (encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) asignarSede(id, campoActivo);
        if (encima && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) asignarSede(id, 1);
    }
    EndScissorMode();
    y += altoLista + 12;

    // Hora del dia
    texto("Hora de salida (simulación del día)", x, y, 15, TEXTO_SUAVE);
    y += 22;
    Rectangle menos = {x, y, 34, 30}, mas = {x + 134, y, 34, 30}, simular = {x + 176, y, w - 176, 30};
    if (boton(menos, "-", false, TEXTO)) cambiarHora(e.hora() - 30);
    std::string hora = textoHora(e.hora());
    texto(hora.c_str(), x + 84 - anchoTexto(hora.c_str(), 24) / 2, y + 2, 24, TEXTO);
    if (boton(mas, "+", false, TEXTO)) cambiarHora(e.hora() + 30);
    if (boton(simular, simulando ? "Detener" : "Simular día", simulando, OSCURO)) simulando = !simulando;
    y += 36;
    double f = e.factorTrafico();
    const char* estado = f >= 0.9 ? "fluido" : (f >= 0.7 ? "moderado" : "pesado");
    char linea[128];
    std::snprintf(linea, sizeof(linea), "Tráfico %s (carro al %.0f%%)%s", estado, f * 100,
                  e.transmilenioOpera() ? "" : " · TransMilenio cerrado");
    texto(linea, x, y, 13, f >= 0.7 ? TEXTO_SUAVE : ROJO);
    y += 24;

    // Opciones
    float tercio = (w - 12) / 3;
    Rectangle o1 = {x, y, tercio, 28}, o2 = {x + tercio + 6, y, tercio, 28}, o3 = {x + 2 * (tercio + 6), y, tercio, 28};
    if (boton(o1, "Alternativas", alternativas, OSCURO)) {
        alternativas = !alternativas;
        hayQueRecalcular = true;
    }
    if (boton(o2, "Ciclorrutas", verCiclo, COLOR_CICLO)) verCiclo = !verCiclo;
    if (boton(o3, "TransMilenio", verTM, COLOR_TM)) verTM = !verTM;
    y += 40;

    // Contingencias
    texto("Contingencias (clic en el mapa)", x, y, 15, TEXTO_SUAVE);
    y += 22;
    float cuarto = (w - 18) / 4;
    Rectangle h1 = {x, y, cuarto, 28}, h2 = {x + cuarto + 6, y, cuarto, 28};
    Rectangle h3 = {x + 2 * (cuarto + 6), y, cuarto, 28}, h4 = {x + 3 * (cuarto + 6), y, cuarto, 28};
    if (boton(h1, "Navegar", herramienta == NAVEGAR, OSCURO)) herramienta = NAVEGAR;
    if (boton(h2, "Marcha", herramienta == PONER_MARCHA, COLOR_MARCHA)) herramienta = PONER_MARCHA;
    if (boton(h3, "Bloqueo", herramienta == PONER_BLOQUEO, COLOR_BLOQUEO)) herramienta = PONER_BLOQUEO;
    if (boton(h4, "Estación", herramienta == CERRAR_ESTACION, COLOR_TM)) herramienta = CERRAR_ESTACION;
    y += 34;
    Rectangle rMenos = {x, y, 30, 28}, rMas = {x + 130, y, 30, 28}, quitar = {x + 170, y, w - 170, 28};
    if (boton(rMenos, "-", false, TEXTO) && radio > 100) radio -= 100;
    std::snprintf(linea, sizeof(linea), "Radio %.0f m", radio);
    texto(linea, x + 38, y + 6, 14, TEXTO);
    if (boton(rMas, "+", false, TEXTO) && radio < 3000) radio += 100;
    if (boton(quitar, "Quitar del mapa", false, TEXTO)) {
        e.quitarEventos(false);
        contingenciasCambiaron();
        avisar("Se quitaron los eventos puestos con el mouse");
    }
    y += 34;

    // Archivos de texto
    float mitad = (w - 6) / 2;
    Rectangle editar = {x, y, mitad, 28}, editarHorario = {x + mitad + 6, y, mitad, 28};
    std::string abrir;
    if (boton(editar, "Editar escenario", false, TEXTO)) abrir = archivoEscenario;
    if (boton(editarHorario, "Editar horario", false, TEXTO)) abrir = archivoHorario;
#ifdef _WIN32
    if (!abrir.empty()) {
        for (char& c : abrir) if (c == '/') c = '\\';
        std::system(("start \"\" notepad \"" + abrir + "\"").c_str());
    }
#endif
    y += 34;
    int activos = 0;
    for (int i = 0; i < e.numEventos(); i++) if (e.eventoActivo(e.evento(i))) activos++;
    std::snprintf(linea, sizeof(linea), "%d de %d eventos activos a esta hora", activos, e.numEventos());
    texto(linea, x, y, 13, TEXTO_SUAVE);
    y += 18;
    size_t salto = informeEscenario.find('\n');
    if (salto != std::string::npos) textoAjustado(informeEscenario.substr(salto + 1), x, y, w, 12, ROJO);

    texto("Rueda: zoom · Arrastrar: mover · Espacio: simular", x, alto - 22, 12, TEXTO_SUAVE);
}

// ---------------------------------------------------------------------------
// Panel derecho: pestanas "Ruta" y "Horario"
// ---------------------------------------------------------------------------
void Mapa::panelDerecho() {
    float ancho = (float)GetScreenWidth(), alto = (float)GetScreenHeight();
    float x0 = ancho - ANCHO_DER;
    DrawRectangle((int)x0, 0, ANCHO_DER, (int)alto, FONDO_PANEL);
    DrawLine((int)x0, 0, (int)x0, (int)alto, BORDE);
    float x = x0 + 16, y = 14, w = ANCHO_DER - 32;

    Rectangle t1 = {x, y, (w - 6) / 2, 32}, t2 = {x + (w - 6) / 2 + 6, y, (w - 6) / 2, 32};
    if (boton(t1, "Ruta", pestana == PESTANA_RUTA, OSCURO)) pestana = PESTANA_RUTA;
    if (boton(t2, "Mi horario", pestana == PESTANA_HORARIO, OSCURO)) pestana = PESTANA_HORARIO;
    y += 46;

    if (pestana == PESTANA_RUTA) pestanaRuta(x, y, w, alto);
    else pestanaHorario(x, y, w, alto);
}

// Barras: cuanto tarda esta ruta segun la hora de salida. Clic en una barra = salir a esa hora.
void Mapa::dibujarGrafica(float x, float y, float w) {
    texto("Tiempo según la hora de salida", x, y, 14, TEXTO);
    y += 20;
    int barras = HORA_GRAFICA_FIN - HORA_GRAFICA_INICIO + 1;
    float anchoBarra = w / barras;
    float altoMax = 64;
    double maximo = 1;
    for (int h = HORA_GRAFICA_INICIO; h <= HORA_GRAFICA_FIN; h++)
        if (grafica[h] < INFINITO && grafica[h] > maximo) maximo = grafica[h];

    Vector2 mouse = GetMousePosition();
    int horaActual = e.hora() / 60;
    for (int h = HORA_GRAFICA_INICIO; h <= HORA_GRAFICA_FIN; h++) {
        float bx = x + (h - HORA_GRAFICA_INICIO) * anchoBarra;
        Rectangle zona = {bx, y, anchoBarra, altoMax};
        bool encima = CheckCollisionPointRec(mouse, zona);
        if (grafica[h] >= INFINITO) {
            texto("x", bx + anchoBarra / 2 - 3, y + altoMax - 14, 12, TEXTO_SUAVE);   // sin ruta a esa hora
        } else {
            float hb = (float)(grafica[h] / maximo) * altoMax;
            Color c = (h == horaActual) ? COLOR_MODO[modo] : (encima ? OSCURO : Color{148, 163, 184, 255});
            DrawRectangleRec(Rectangle{bx + 1, y + altoMax - hb, anchoBarra - 2, hb}, c);
        }
        if (encima) {
            char tip[48];
            if (grafica[h] >= INFINITO) std::snprintf(tip, sizeof(tip), "%02d:00 · sin ruta", h);
            else std::snprintf(tip, sizeof(tip), "%02d:00 · %.0f min", h, grafica[h]);
            texto(tip, x + w - anchoTexto(tip, 13), y - 20, 13, OSCURO);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) cambiarHora(h * 60);
        }
        if (h % 3 == 0) {
            char etiqueta[8];
            std::snprintf(etiqueta, sizeof(etiqueta), "%d", h);
            texto(etiqueta, bx + anchoBarra / 2 - anchoTexto(etiqueta, 11) / 2, y + altoMax + 2, 11, TEXTO_SUAVE);
        }
    }
}

void Mapa::pestanaRuta(float x, float y, float w, float alto) {
    Vector2 mouse = GetMousePosition();
    char linea[160];
    if (origen == -1 || destino == -1) {
        textoAjustado("Elige el origen y el destino en la lista o con clic sobre el mapa.", x, y, w, 15, TEXTO_SUAVE);
        return;
    }
    if (origen == destino) {
        textoAjustado("El origen y el destino son la misma sede.", x, y, w, 15, TEXTO_SUAVE);
        return;
    }
    if (numRutas == 0) {
        std::string msg;
        if (modo == MODO_TM && !e.transmilenioOpera())
            msg = "TransMilenio no opera a esta hora (funciona de 4:30 a. m. a 11:00 p. m.).";
        else
            msg = std::string("No hay ruta en ") + nombreModo(modo) +
                  ": las contingencias cerraron todos los caminos posibles.";
        y += textoAjustado(msg, x, y, w, 16, COLOR_BLOQUEO) + 8;
        y += textoAjustado("Prueba otro modo, otra hora o quita algún bloqueo.", x, y, w, 14, TEXTO_SUAVE) + 16;
        dibujarGrafica(x, y, w);
        return;
    }

    // Tarjetas de las rutas
    for (int i = 0; i < numRutas; i++) {
        const Ruta& r = rutas[i];
        Rectangle tarjeta = {x, y, w, 52};
        bool encima = CheckCollisionPointRec(mouse, tarjeta);
        bool elegida = (i == rutaElegida);
        Color fondo = elegida ? Fade(COLOR_MODO[r.modo], 0.15f) : (encima ? BOTON_ENCIMA : WHITE);
        DrawRectangleRounded(tarjeta, 0.2f, 6, fondo);
        DrawRectangleRoundedLines(tarjeta, 0.2f, 6, elegida ? COLOR_MODO[r.modo] : BORDE);
        std::snprintf(linea, sizeof(linea), "%.0f min", r.minutos);
        texto(linea, x + 14, y + 6, 22, elegida ? COLOR_MODO[r.modo] : TEXTO);
        int llegada = e.hora() + (int)(r.minutos + 0.5);
        std::snprintf(linea, sizeof(linea), "%.1f km · llegas %s · %s", r.metros / 1000, textoHora(llegada).c_str(),
                      i == 0 ? "más rápida" : "alternativa");
        texto(linea, x + 14, y + 31, 13, TEXTO_SUAVE);
        if (i > 0) {
            std::snprintf(linea, sizeof(linea), "+%.0f min", r.minutos - rutas[0].minutos);
            texto(linea, x + w - anchoTexto(linea, 14) - 12, y + 8, 14, TEXTO_SUAVE);
        }
        if (encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) elegirRuta(i);
        y += 58;
    }

    y += 6;
    dibujarGrafica(x, y, w);
    y += 106;

    // Indicaciones paso a paso
    texto("Indicaciones", x, y, 16, TEXTO);
    y += 26;
    Rectangle lista = {x - 15, y, (float)ANCHO_DER - 2, alto - y - 30};
    if (CheckCollisionPointRec(mouse, lista)) scrollPasos -= GetMouseWheelMove() * 40;
    if (scrollPasos < 0) scrollPasos = 0;
    BeginScissorMode((int)lista.x, (int)lista.y, (int)lista.width, (int)lista.height);
    float yy = y - scrollPasos;
    for (int i = 0; i < pasos.tamano(); i++) {
        Color c = COLOR_MODO[modo];
        if (pasos[i].tipo == TM_BUS) c = COLOR_TM;
        else if (modo == MODO_TM) c = COLOR_MODO[MODO_PIE];
        DrawCircle((int)x + 10, (int)yy + 10, 10, c);
        std::snprintf(linea, sizeof(linea), "%d", i + 1);
        texto(linea, x + 10 - anchoTexto(linea, 12) / 2, yy + 4, 12, WHITE);
        float h = textoAjustado(pasos[i].texto, x + 28, yy, w - 30, 14, TEXTO);
        if (pasos[i].metros >= 1000)
            std::snprintf(linea, sizeof(linea), "%.1f km · %.0f min", pasos[i].metros / 1000, pasos[i].minutos);
        else
            std::snprintf(linea, sizeof(linea), "%.0f m · %.0f min", pasos[i].metros, pasos[i].minutos);
        texto(linea, x + 28, yy + h, 12, TEXTO_SUAVE);
        yy += h + 24;
    }
    float contenido = yy + scrollPasos - y;
    scrollPasos = fminf(scrollPasos, fmaxf(0, contenido - lista.height));
    EndScissorMode();

    std::snprintf(linea, sizeof(linea), "Grafo: %d vértices · %d aristas", e.red().numVertices(), e.red().numAristas());
    texto(linea, x, alto - 22, 12, TEXTO_SUAVE);
}

// Traslados entre clases seguidas: en que modos se llega a tiempo
void Mapa::pestanaHorario(float x, float y, float w, float alto) {
    Vector2 mouse = GetMousePosition();
    char linea[160];
    y += textoAjustado(informeHorario, x, y, w, 13, TEXTO_SUAVE) + 4;
    if (traslados.vacio()) {
        textoAjustado("No hay traslados entre sedes distintas. Escribe tu horario en datos/horario.txt "
                      "(botón \"Editar horario\").", x, y, w, 14, TEXTO_SUAVE);
        return;
    }
    texto("Clic en un traslado para ver la ruta", x, y, 13, TEXTO_SUAVE);
    y += 22;

    Rectangle lista = {x - 15, y, (float)ANCHO_DER - 2, alto - y - 10};
    if (CheckCollisionPointRec(mouse, lista)) scrollHorario -= GetMouseWheelMove() * 40;
    float altoTarjeta = 112;
    scrollHorario = fmaxf(0, fminf(scrollHorario, traslados.tamano() * (altoTarjeta + 8) - lista.height));
    BeginScissorMode((int)lista.x, (int)lista.y, (int)lista.width, (int)lista.height);
    float yy = y - scrollHorario;
    for (int i = 0; i < traslados.tamano(); i++) {
        const Traslado& t = traslados[i];
        const Clase& a = clases[t.claseOrigen];
        const Clase& b = clases[t.claseDestino];
        Rectangle tarjeta = {x, yy, w, altoTarjeta};
        bool encima = CheckCollisionPointRec(mouse, tarjeta) && CheckCollisionPointRec(mouse, lista);
        bool alcanza = t.minutos[t.mejorModo] <= t.disponible;
        DrawRectangleRounded(tarjeta, 0.12f, 6, encima ? BOTON_ENCIMA : WHITE);
        DrawRectangleRoundedLines(tarjeta, 0.12f, 6, alcanza ? BORDE : ROJO);

        std::snprintf(linea, sizeof(linea), "%s · sales %s · entras %s (%d min)", nombreDia(a.dia),
                      textoHora(a.fin).c_str(), textoHora(b.inicio).c_str(), t.disponible);
        texto(linea, x + 10, yy + 8, 14, TEXTO);
        std::string ruta = e.red().vertice(a.sede).nombre + "  →  " + e.red().vertice(b.sede).nombre;
        texto(ruta.c_str(), x + 10, yy + 28, 13, TEXTO_SUAVE);
        texto(b.materia.c_str(), x + 10, yy + 45, 13, OSCURO);

        // Una ficha por modo: verde si llega a tiempo, roja si no
        float fw = (w - 20 - 4 * 4) / 5;
        for (int m = 0; m < NUM_MODOS; m++) {
            Rectangle ficha = {x + 10 + m * (fw + 4), yy + 66, fw, 36};
            bool aTiempo = t.minutos[m] <= t.disponible;
            Color c = t.minutos[m] >= INFINITO ? Color{148, 163, 184, 255} : (aTiempo ? VERDE : ROJO);
            DrawRectangleRounded(ficha, 0.3f, 6, Fade(c, m == t.mejorModo ? 0.9f : 0.18f));
            Color tc = (m == t.mejorModo) ? WHITE : c;
            texto(ETIQUETA_MODO[m], ficha.x + (fw - anchoTexto(ETIQUETA_MODO[m], 11)) / 2, ficha.y + 3, 11, tc);
            if (t.minutos[m] >= INFINITO) std::snprintf(linea, sizeof(linea), "--");
            else std::snprintf(linea, sizeof(linea), "%.0f'", t.minutos[m]);
            texto(linea, ficha.x + (fw - anchoTexto(linea, 15)) / 2, ficha.y + 17, 15, tc);
        }

        if (encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {   // ver ese traslado en el mapa
            origen = a.sede;
            destino = b.sede;
            modo = (Modo)t.mejorModo;
            pestana = PESTANA_RUTA;
            graficaSucia = true;
            cambiarHora(a.fin);
        }
        yy += altoTarjeta + 8;
    }
    EndScissorMode();
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------
void Mapa::ejecutar(const std::string& captura) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1600, 900, "Rutas Inter-Sedes - Universidad Distrital");
    SetWindowMinSize(1200, 640);
    int monitor = GetCurrentMonitor();   // en una pantalla pequena la ventana se abre maximizada
    if (GetMonitorWidth(monitor) < 1600 || GetMonitorHeight(monitor) < 1000) MaximizeWindow();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    cargarFuente();
    std::string rutaMapa = carpeta + "/mapa.png";
    if (FileExists(rutaMapa.c_str())) {
        textura = LoadTexture(rutaMapa.c_str());
        hayMapa = textura.id != 0;
        GenTextureMipmaps(&textura);
        SetTextureFilter(textura, TEXTURE_FILTER_TRILINEAR);
    }
    prepararCapas();
    encuadrarSedes();
    revisarArchivos(true);

    int cuadros = 0;
    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            Rectangle area = areaMapa();
            camara.target = GetScreenToWorld2D(camara.offset, camara);
            camara.offset.x = area.x + area.width / 2;
            camara.offset.y = area.y + area.height / 2;
        }
        manejarTeclado();
        revisarArchivos(false);
        // Simulacion del dia: el reloj avanza 15 minutos cada 0.4 segundos
        if (simulando && GetTime() - ultimoPaso >= 0.4) {
            ultimoPaso = GetTime();
            cambiarHora(e.hora() + 15);
        }
        if (hayQueRecalcular) recalcular();

        BeginDrawing();
        ClearBackground(Color{232, 236, 240, 255});
        BeginScissorMode(ANCHO_IZQ, 0, GetScreenWidth() - ANCHO_IZQ - ANCHO_DER, GetScreenHeight());
        BeginMode2D(camara);
        dibujarMundo();
        EndMode2D();
        EndScissorMode();
        dibujarMarcadores();
        manejarMapa();
        panelIzquierdo();
        panelDerecho();
        EndDrawing();

        cuadros++;
        if (!captura.empty() && cuadros == 10) {
            TakeScreenshot(captura.c_str());
            break;
        }
    }

    if (hayMapa) UnloadTexture(textura);
    if (fuentePropia) UnloadFont(fuente);
    CloseWindow();
}

void ejecutarMapa(Enrutador& e, const std::string& carpetaDatos, const std::string& captura) {
    Mapa mapa(e, carpetaDatos);
    mapa.ejecutar(captura);
}
