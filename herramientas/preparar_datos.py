#!/usr/bin/env python3
"""
preparar_datos.py - Herramienta de apoyo para EXTRAER LOS DATOS (se ejecuta una sola vez).
No hace parte del programa en C++: solo convierte datos de OpenStreetMap a archivos CSV sencillos.

Genera (todas las coordenadas en pixeles de datos/mapa.png):
    datos/nodos.csv           id;x;y;nucleo_carro;nucleo_bici;nucleo_pie   (solo cruces entre vias)
    datos/tramos.csv          origen;destino;metros;tipo;ciclorruta;ida;vuelta;nombre;trazado
    datos/tm_estaciones.csv   nombre;x;y
    datos/tm_tramos.csv       estacion_a;estacion_b;metros;servicios;trazado
    datos/sedes.csv           nombre;direccion;x;y        (desde herramientas/sedes_latlon.csv)
    datos/mapa.png            imagen de fondo (teselas de OpenStreetMap, zoom 15)

Idea clave: un nodo del grafo es SOLO un punto donde se cruzan vias. Los puntos intermedios de una calle
(curvas) no son nodos: se guardan como el "trazado" de la arista, que el programa usa para dibujar la ruta
sobre la calle real.

Uso:   python herramientas/preparar_datos.py            (usa la cache si ya se descargo antes)
       python herramientas/preparar_datos.py --refrescar
Requiere: requests, Pillow.
"""
import argparse
import collections
import heapq
import json
import math
import os
import sys
import time

import requests
from PIL import Image

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATOS = os.path.join(RAIZ, "datos")
CACHE = os.path.join(RAIZ, "herramientas", "cache")

# Recuadro que contiene todas las sedes (sur, oeste, norte, este)
SUR, OESTE, NORTE, ESTE = 4.565, -74.200, 4.660, -74.050
BBOX = f"({SUR},{OESTE},{NORTE},{ESTE})"
ZOOM_MAPA = 15
MAX_SALTO_TM_M = 5000

SERVIDORES = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.private.coffee/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
]
CABECERAS = {"User-Agent": "proyecto-academico-rutas-bogota/1.0"}

# Bits de permisos (uso interno del script; en los CSV se escriben como letras C, B, P)
CARRO, MOTO, BICI, PIE, TM = 1, 2, 4, 8, 16
VEHICULO = CARRO | MOTO

# Tipos de via (en los CSV se escriben como texto: troncal, principal, ...)
C_AUTOPISTA, C_TRONCAL, C_PRINCIPAL, C_SECUNDARIA, C_TERCIARIA, C_PEATONAL, C_CICLORRUTA, C_CONECTOR = range(8)
C_BUSWAY = 99  # solo se usa aqui para trazar TransMilenio; no se exporta

CLASE_DE = {
    "motorway": C_AUTOPISTA, "motorway_link": C_AUTOPISTA,
    "trunk": C_TRONCAL, "trunk_link": C_TRONCAL,
    "primary": C_PRINCIPAL, "primary_link": C_PRINCIPAL,
    "secondary": C_SECUNDARIA, "secondary_link": C_SECUNDARIA,
    "tertiary": C_TERCIARIA, "tertiary_link": C_TERCIARIA,
    "pedestrian": C_PEATONAL, "footway": C_PEATONAL,
    "cycleway": C_CICLORRUTA, "path": C_CICLORRUTA,
    "busway": C_BUSWAY,
}


# ----------------------------------------------------------------------------- utilidades
def distancia_m(lat1, lon1, lat2, lon2):
    r = 6371000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def largo_trazado(pts):
    return sum(distancia_m(*pts[i], *pts[i + 1]) for i in range(len(pts) - 1))


def overpass(nombre, consulta, refrescar):
    os.makedirs(CACHE, exist_ok=True)
    ruta = os.path.join(CACHE, nombre + ".json")
    if os.path.exists(ruta) and not refrescar:
        with open(ruta, encoding="utf-8") as f:
            return json.load(f)
    ultimo = None
    for intento in range(12):
        url = SERVIDORES[intento % len(SERVIDORES)]
        try:
            r = requests.post(url, data={"data": consulta}, headers=CABECERAS, timeout=900)
            if r.status_code == 200 and r.text.lstrip().startswith("{"):
                datos = r.json()
                with open(ruta, "w", encoding="utf-8") as f:
                    json.dump(datos, f)
                return datos
            ultimo = f"{url} -> HTTP {r.status_code}"
        except requests.RequestException as e:
            ultimo = f"{url} -> {e}"
        print("   reintentando:", ultimo)
        time.sleep(10)
    sys.exit(f"No se pudo descargar '{nombre}': {ultimo}")


def limpiar(texto):
    """El CSV usa ';' como separador: se evita que aparezca dentro de un campo."""
    return (texto or "").replace(";", ",").replace("\n", " ").strip()


class Rejilla:
    """Indice espacial sencillo para buscar el nodo mas cercano."""

    def __init__(self, celda=0.002):
        self.celda = celda
        self.c = collections.defaultdict(list)

    def clave(self, lat, lon):
        return (int(lat / self.celda), int(lon / self.celda))

    def agregar(self, ident, lat, lon):
        self.c[self.clave(lat, lon)].append((ident, lat, lon))

    def cercanos(self, lat, lon, radio_celdas=1):
        ci, cj = self.clave(lat, lon)
        for di in range(-radio_celdas, radio_celdas + 1):
            for dj in range(-radio_celdas, radio_celdas + 1):
                yield from self.c.get((ci + di, cj + dj), ())

    def mas_cercano(self, lat, lon, filtro=None, radio_celdas=3):
        mejor, md = None, float("inf")
        for ident, la, lo in self.cercanos(lat, lon, radio_celdas):
            if filtro and not filtro(ident):
                continue
            d = distancia_m(lat, lon, la, lo)
            if d < md:
                mejor, md = ident, d
        return mejor, md


# ----------------------------------------------------------------------------- red vial
def permisos_de(tags):
    hw = tags.get("highway")
    clase = CLASE_DE[hw]
    if clase == C_AUTOPISTA:
        p = VEHICULO
    elif clase == C_TRONCAL:
        p = VEHICULO | BICI | PIE
    elif clase in (C_PRINCIPAL, C_SECUNDARIA, C_TERCIARIA):
        p = VEHICULO | BICI | PIE
    elif clase == C_PEATONAL:
        p = PIE | BICI
    elif clase == C_CICLORRUTA:
        p = BICI | PIE
    else:  # busway: carril exclusivo de TransMilenio
        p = 0

    def no(v):
        return v in ("no", "private", "use_sidepath")

    if no(tags.get("access", "")) and clase != C_BUSWAY:
        p = 0
    if no(tags.get("motor_vehicle", "")) or no(tags.get("motorcar", "")) or no(tags.get("vehicle", "")):
        p &= ~VEHICULO
    if no(tags.get("bicycle", "")):
        p &= ~BICI
    elif tags.get("bicycle") in ("yes", "designated", "permissive"):
        p |= BICI
    if no(tags.get("foot", "")):
        p &= ~PIE
    elif tags.get("foot") in ("yes", "designated", "permissive"):
        p |= PIE

    ida = vuelta = p
    sentido = tags.get("oneway", "no")
    unico = sentido in ("yes", "true", "1") or tags.get("junction") in ("roundabout", "circular") \
        or (hw in ("motorway", "motorway_link") and sentido != "no")
    if sentido == "-1":
        ida = p & ~(VEHICULO | BICI)  # el peaton camina en ambos sentidos
    elif unico:
        vuelta = p & ~VEHICULO
        if tags.get("oneway:bicycle") != "no":
            vuelta &= ~BICI
    return clase, ida, vuelta


def tiene_ciclorruta(tags):
    if tags.get("highway") in ("cycleway",):
        return 1
    if tags.get("highway") == "path" and tags.get("bicycle") == "designated":
        return 1
    for k in ("cycleway", "cycleway:right", "cycleway:left", "cycleway:both"):
        if tags.get(k) in ("lane", "track", "opposite_lane", "opposite_track", "shared_busway"):
            return 1
    return 0


def nombre_de(tags, clase):
    n = tags.get("name") or tags.get("official_name") or tags.get("ref") or ""
    if not n and clase == C_CICLORRUTA:
        n = "Ciclorruta"
    return limpiar(n)


def construir_red(refrescar):
    print("1/4  Descargando red vial principal y ciclorrutas de OpenStreetMap...")
    consulta = f"""[out:json][timeout:900];
(
  way["highway"~"^(motorway|motorway_link|trunk|trunk_link|primary|primary_link|secondary|secondary_link|tertiary|tertiary_link|pedestrian|cycleway|busway)$"]{BBOX};
  way["highway"="footway"]["footway"="crossing"]{BBOX};
  way["highway"="path"]["bicycle"="designated"]{BBOX};
);
out geom;"""
    datos = overpass("red_vial", consulta, refrescar)
    vias = [e for e in datos["elements"] if e["type"] == "way" and "geometry" in e]
    print(f"     {len(vias)} vias descargadas")

    coord = {}
    usos = collections.Counter()
    extremos = set()
    for w in vias:
        if w["tags"].get("area") == "yes":
            continue
        for nid, g in zip(w["nodes"], w["geometry"]):
            if g is None:
                continue
            coord[nid] = (g["lat"], g["lon"])
            usos[nid] += 1
        extremos.add(w["nodes"][0])
        extremos.add(w["nodes"][-1])

    # Un nodo del grafo es un extremo de via o un punto compartido por dos o mas vias (cruce)
    cruces = {n for n, c in usos.items() if c >= 2} | extremos

    # Partir cada via en tramos entre cruces
    tramos = []  # [a, b, puntos, clase, ciclo, ida, vuelta, nombre]
    for w in vias:
        t = w["tags"]
        if t.get("area") == "yes":
            continue
        clase, ida, vuelta = permisos_de(t)
        ciclo = tiene_ciclorruta(t)
        nombre = nombre_de(t, clase)
        nodos = [n for n in w["nodes"] if n in coord]
        if len(nodos) < 2:
            continue
        inicio = 0
        for i in range(1, len(nodos)):
            if nodos[i] in cruces or i == len(nodos) - 1:
                seg = nodos[inicio:i + 1]
                if len(seg) >= 2 and seg[0] != seg[-1]:
                    tramos.append([seg[0], seg[-1], [coord[n] for n in seg], clase, ciclo, ida, vuelta, nombre])
                inicio = i

    tramos = contraer_grado_dos(tramos)
    return tramos, coord


def contraer_grado_dos(tramos):
    """Une tramos consecutivos de la misma via cuando el nodo intermedio no es un cruce real."""
    vivos = {i: t for i, t in enumerate(tramos)}
    incid = collections.defaultdict(set)
    for i, t in vivos.items():
        incid[t[0]].add(i)
        incid[t[1]].add(i)

    def orientar(t, nodo_final):
        """Devuelve el tramo orientado para que termine en nodo_final."""
        if t[1] == nodo_final:
            return list(t)
        return [t[1], t[0], t[2][::-1], t[3], t[4], t[6], t[5], t[7]]

    cambios = True
    unidos = 0
    while cambios:
        cambios = False
        for n in list(incid.keys()):
            ids = incid.get(n)
            if not ids or len(ids) != 2:
                continue
            i, j = tuple(ids)
            a = orientar(vivos[i], n)          # ... -> n
            b = orientar(vivos[j], n)          # ... -> n
            b = [b[1], b[0], b[2][::-1], b[3], b[4], b[6], b[5], b[7]]  # n -> ...
            if a[0] == b[1]:
                continue  # formaria un ciclo
            if a[3:] != b[3:]:
                continue  # cambia la clase, el nombre o los permisos: es un limite real
            nuevo = [a[0], b[1], a[2] + b[2][1:], a[3], a[4], a[5], a[6], a[7]]
            del vivos[i], vivos[j]
            k = len(tramos) + unidos
            vivos[k] = nuevo
            for x in (a[0], b[1]):
                incid[x].discard(i)
                incid[x].discard(j)
                incid[x].add(k)
            del incid[n]
            unidos += 1
            cambios = True
    print(f"     {unidos} uniones de tramos sin cruce real")
    return list(vivos.values())


def componente_principal(tramos, coord):
    """Conecta piezas sueltas (p. ej. ciclorrutas paralelas) a la red y descarta las que quedan aisladas."""
    exportables = [t for t in tramos if t[3] != C_BUSWAY and (t[5] or t[6])]
    padre = {}

    def raiz(x):
        while padre.setdefault(x, x) != x:
            padre[x] = padre[padre[x]]
            x = padre[x]
        return x

    def unir(x, y):
        padre[raiz(x)] = raiz(y)

    for t in exportables:
        unir(t[0], t[1])
    tam = collections.Counter(raiz(n) for n in list(padre))
    principal = tam.most_common(1)[0][0]

    rej = Rejilla()
    for n in padre:
        if raiz(n) == principal:
            rej.agregar(n, *coord[n])

    conectores = 0
    for n in list(padre):
        if raiz(n) != principal:
            m, d = rej.mas_cercano(*coord[n], radio_celdas=1)
            if m is not None and d <= 60:
                exportables.append([n, m, [coord[n], coord[m]], C_CONECTOR, 0, BICI | PIE, BICI | PIE, ""])
                conectores += 1
    padre.clear()
    for t in exportables:
        unir(t[0], t[1])
    tam = collections.Counter(raiz(n) for n in list(padre))
    principal = tam.most_common(1)[0][0]
    final = [t for t in exportables if raiz(t[0]) == principal]
    print(f"     {conectores} conectores agregados; {len(exportables) - len(final)} tramos aislados descartados")
    return final


# ----------------------------------------------------------------------------- TransMilenio
def normalizar_estacion(nombre):
    n = (nombre or "").strip()
    for pref in ("Estación ", "Estacion "):
        if n.startswith(pref):
            n = n[len(pref):]
    return limpiar(n)


def descomponer_expresos(pares, estaciones):
    """Un expreso A->C que pasa por la estacion B (sin parar) se reemplaza por A->B y B->C, y sus
    servicios se suman a esos tramos. Asi cada tramo une estaciones vecinas y el mismo
    servicio queda seguido por varias paradas (menos transbordos)."""
    def d(a, b):
        return distancia_m(estaciones[a][1], estaciones[a][2], estaciones[b][1], estaciones[b][2])

    salientes = collections.defaultdict(set)
    for a, b in pares:
        salientes[a].add(b)
    cambio = True
    descompuestos = 0
    while cambio:
        cambio = False
        for (a, c) in sorted(pares, key=lambda p: -d(*p)):      # primero los saltos mas largos
            if (a, c) not in pares:
                continue
            mejor = None
            for b in salientes[a]:
                if b != c and (b, c) in pares and d(a, b) + d(b, c) <= 1.12 * d(a, c) + 100:
                    if mejor is None or d(a, b) < d(a, mejor):
                        mejor = b
            if mejor is None:
                continue
            servicios = pares.pop((a, c))
            salientes[a].discard(c)
            pares[(a, mejor)] |= servicios
            pares[(mejor, c)] |= servicios
            descompuestos += 1
            cambio = True
    print(f"     {descompuestos} tramos expresos descompuestos en tramos entre estaciones vecinas")
    return pares


def orientacion_invertida(sec, tags, estaciones, paradas):
    """True si la secuencia de estaciones va en sentido contrario al destino del servicio.
    'paradas' son (lat, lon, nombre) de todas las paradas de la relacion, incluso fuera del mapa."""
    def buscar(nombre):
        n = normalizar_estacion(nombre)
        if not n:
            return None
        for e in estaciones:
            if e[0] == n:
                return e
        for la, lo, nom in paradas:
            if normalizar_estacion(nom) == n:
                return (n, la, lo)
        return None

    pasos = [(a, b) for a, b in zip(sec, sec[1:]) if a is not None and b is not None and a != b]
    for etiqueta, signo in (("to", 1), ("from", -1)):
        ref = buscar(tags.get(etiqueta, ""))
        if ref is None:
            continue
        votos = 0
        for a, b in pasos:
            da = distancia_m(estaciones[a][1], estaciones[a][2], ref[1], ref[2])
            db = distancia_m(estaciones[b][1], estaciones[b][2], ref[1], ref[2])
            if abs(da - db) < 50 or abs(da - db) > MAX_SALTO_TM_M:
                continue  # paso dudoso o salto falso: no vota
            votos += signo if db < da else -signo
        if votos != 0:
            return votos < 0
    return False


def construir_transmilenio(refrescar, tramos_todos, coord):
    print("2/4  Descargando servicios y estaciones de TransMilenio...")
    consulta = f"""[out:json][timeout:600];
rel["route"="bus"]["network"~"TransMilenio",i]{BBOX}->.r;
.r out body;
node(r.r)["public_transport"]; out;
way(r.r)["public_transport"]; out center;"""
    datos = overpass("transmilenio", consulta, refrescar)
    puntos = {}
    for e in datos["elements"]:
        if e["type"] == "node":
            puntos[("node", e["id"])] = (e["lat"], e["lon"], e["tags"].get("name", ""))
        elif e["type"] == "way" and "center" in e:
            puntos[("way", e["id"])] = (e["center"]["lat"], e["center"]["lon"], e["tags"].get("name", ""))

    margen = -0.002  # solo estaciones dentro de la imagen del mapa
    dentro = lambda la, lo: SUR - margen <= la <= NORTE + margen and OESTE - margen <= lo <= ESTE + margen

    # Agrupar paradas en estaciones: mismo nombre y a menos de 450 m
    estaciones = []  # [nombre, lat, lon, n]
    estacion_de = {}

    def estacion_para(lat, lon, nombre):
        nom = normalizar_estacion(nombre)
        mejor, md = None, 1e18
        for i, (en, ela, elo, _) in enumerate(estaciones):
            d = distancia_m(lat, lon, ela, elo)
            if (nom and en == nom and d < 450) or (d < 60):
                if d < md:
                    mejor, md = i, d
        if mejor is None:
            if not nom:
                return None
            estaciones.append([nom, lat, lon, 1])
            return len(estaciones) - 1
        e = estaciones[mejor]
        e[1] = (e[1] * e[3] + lat) / (e[3] + 1)
        e[2] = (e[2] * e[3] + lon) / (e[3] + 1)
        e[3] += 1
        return mejor

    secuencias = []
    invertidas = 0
    for rel in (e for e in datos["elements"] if e["type"] == "relation"):
        ref = limpiar(rel["tags"].get("ref", ""))
        hacia = limpiar(rel["tags"].get("to", ""))
        if ref and hacia:
            ref = f"{ref} ({hacia})"   # p. ej. "B74 (Portal Norte)"
        ref = ref.replace(",", " ")    # la coma separa servicios en el CSV
        sec = []
        for m in rel["members"]:
            if not m["role"].startswith("stop") and m["role"] != "platform":
                continue
            p = puntos.get((m["type"], m["ref"]))
            clave = (m["type"], m["ref"])
            if p and dentro(p[0], p[1]) and clave not in estacion_de:
                estacion_de[clave] = estacion_para(*p)
            ei = estacion_de.get(clave) if p and dentro(p[0], p[1]) else None
            if ei is None:
                # Parada sin datos o fuera del recuadro: se corta la secuencia para no inventar un salto
                if sec and sec[-1] is not None:
                    sec.append(None)
                continue
            if not sec or sec[-1] != ei:
                sec.append(ei)
        # En OSM las paradas no siempre estan en el orden del recorrido. Se orienta la secuencia
        # con geometria: al avanzar, la distancia a la estacion destino ("to") debe bajar en la
        # mayoria de los pasos (o la distancia al origen "from" debe subir).
        # La relacion puede venir partida en pedazos desordenados: se separa en pedazos
        # (por paradas faltantes o saltos imposibles) y cada pedazo se orienta por separado.
        paradas = [puntos[(m["type"], m["ref"])] for m in rel["members"] if (m["type"], m["ref"]) in puntos]
        pedazos, actual = [], []
        for x in sec:
            salto = (x is not None and actual and
                     distancia_m(estaciones[actual[-1]][1], estaciones[actual[-1]][2],
                                 estaciones[x][1], estaciones[x][2]) > MAX_SALTO_TM_M)
            if x is None or salto:
                if actual:
                    pedazos.append(actual)
                actual = [] if x is None else [x]
            else:
                actual.append(x)
        if actual:
            pedazos.append(actual)
        sec = []
        for p in pedazos:
            if orientacion_invertida(p, rel["tags"], estaciones, paradas):
                p.reverse()
                invertidas += 1
            sec.extend(p + [None])
        if len([x for x in sec if x is not None]) >= 2:
            secuencias.append((ref, sec))

    # Nombres unicos
    vistos = collections.Counter()
    for e in estaciones:
        vistos[e[0]] += 1
        if vistos[e[0]] > 1:
            e[0] = f"{e[0]} ({vistos[e[0]]})"

    pares = {}
    for ref, sec in secuencias:
        for a, b in zip(sec, sec[1:]):
            if a is None or b is None or a == b:
                continue
            ea, eb = estaciones[a], estaciones[b]
            if distancia_m(ea[1], ea[2], eb[1], eb[2]) > MAX_SALTO_TM_M:
                continue  # datos incompletos: ningun expreso salta tanto sin parar
            pares.setdefault((a, b), set()).add(ref)
    pares = descomponer_expresos(pares, estaciones)
    print(f"     {len(estaciones)} estaciones, {len(pares)} tramos entre estaciones ({len(secuencias)} servicios, {invertidas} corregidos por orden invertido)")

    # Trazado de cada tramo TM siguiendo las vias (prefiere carriles exclusivos busway)
    ady = collections.defaultdict(list)
    for t in tramos_todos:
        if t[3] in (C_AUTOPISTA, C_TRONCAL, C_PRINCIPAL, C_SECUNDARIA, C_BUSWAY):
            peso = largo_trazado(t[2]) * (0.6 if t[3] == C_BUSWAY else 1.0)
            ady[t[0]].append((t[1], peso, t[2]))
            ady[t[1]].append((t[0], peso, t[2][::-1]))
    rej = Rejilla()
    for n in ady:
        rej.agregar(n, *coord[n])

    def camino(origen, destino, limite):
        dist = {origen: 0.0}
        prev = {}
        cola = [(0.0, origen)]
        while cola:
            d, u = heapq.heappop(cola)
            if u == destino:
                break
            if d > dist.get(u, 1e18) or d > limite:
                continue
            for v, w, pts in ady[u]:
                nd = d + w
                if nd < dist.get(v, 1e18):
                    dist[v] = nd
                    prev[v] = (u, pts)
                    heapq.heappush(cola, (nd, v))
        if destino not in prev:
            return None
        partes = []
        x = destino
        while x != origen:
            u, pts = prev[x]
            partes.append(pts)
            x = u
        puntos = []
        for pts in reversed(partes):
            puntos.extend(pts if not puntos else pts[1:])
        return puntos

    tm_tramos = []
    for (a, b), refs in pares.items():
        ea, eb = estaciones[a], estaciones[b]
        recta = distancia_m(ea[1], ea[2], eb[1], eb[2])
        na, _ = rej.mas_cercano(ea[1], ea[2])
        nb, _ = rej.mas_cercano(eb[1], eb[2])
        pts = None
        if na is not None and nb is not None and na != nb:
            pts = camino(na, nb, recta * 3 + 500)
        if pts:
            pts = [(ea[1], ea[2])] + pts + [(eb[1], eb[2])]
            if largo_trazado(pts) > 2.2 * recta + 400:
                pts = None
        if not pts:
            pts = [(ea[1], ea[2]), (eb[1], eb[2])]
        tm_tramos.append((ea[0], eb[0], largo_trazado(pts), ",".join(sorted(r for r in refs if r)), pts))
    return estaciones, tm_tramos


# ----------------------------------------------------------------------------- mapa de fondo
def mapa_de_fondo(refrescar):
    print("3/4  Descargando mapa de fondo (teselas de OpenStreetMap, zoom 15, una sola vez)...")
    z = ZOOM_MAPA
    n = 2 ** z

    def tx(lon):
        return (lon + 180.0) / 360.0 * n

    def ty(lat):
        return (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n

    x0, x1 = int(tx(OESTE)), int(tx(ESTE))
    y0, y1 = int(ty(NORTE)), int(ty(SUR))
    ancho, alto = (x1 - x0 + 1) * 256, (y1 - y0 + 1) * 256
    imagen = Image.new("RGB", (ancho, alto), (240, 240, 240))
    carpeta = os.path.join(CACHE, "teselas", str(z))
    os.makedirs(carpeta, exist_ok=True)
    total = (x1 - x0 + 1) * (y1 - y0 + 1)
    hechas = 0
    sesion = requests.Session()
    sesion.headers.update(CABECERAS)
    for x in range(x0, x1 + 1):
        for y in range(y0, y1 + 1):
            ruta = os.path.join(carpeta, f"{x}_{y}.png")
            if not os.path.exists(ruta) or refrescar:
                url = f"https://tile.openstreetmap.org/{z}/{x}/{y}.png"
                for intento in range(4):
                    try:
                        r = sesion.get(url, timeout=60)
                        if r.status_code == 200 and r.headers.get("Content-Type", "").startswith("image/"):
                            with open(ruta, "wb") as f:
                                f.write(r.content)
                            break
                    except requests.RequestException:
                        pass
                    time.sleep(2)
                time.sleep(0.2)  # descarga pausada y con cache: respeta la politica de uso de teselas OSM
            if os.path.exists(ruta):
                imagen.paste(Image.open(ruta).convert("RGB"), ((x - x0) * 256, (y - y0) * 256))
            hechas += 1
            if hechas % 40 == 0:
                print(f"     {hechas}/{total} teselas")
    tamanos = collections.Counter(os.path.getsize(os.path.join(carpeta, f"{x}_{y}.png"))
                                  for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)
                                  if os.path.exists(os.path.join(carpeta, f"{x}_{y}.png")))
    if tamanos and tamanos.most_common(1)[0][1] > total // 2:
        sys.exit("Las teselas descargadas son todas iguales (posible aviso del servidor). Borre herramientas/cache/teselas y revise.")
    imagen = imagen.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    imagen.save(os.path.join(DATOS, "mapa.png"), optimize=True)
    print(f"     mapa.png de {ancho}x{alto} px  (1 pixel = {156543.03 * math.cos(math.radians(4.61)) / 2 ** z:.2f} m)")


# ----------------------------------------------------------------------------- escritura
def _tesela_x(lon):
    return (lon + 180.0) / 360.0 * 2 ** ZOOM_MAPA


def _tesela_y(lat):
    return (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * 2 ** ZOOM_MAPA


def a_pixel(lat, lon):
    """Latitud/longitud -> pixel de datos/mapa.png (proyeccion Web Mercator de las teselas).
    Asi el programa en C++ trabaja solo con coordenadas x,y en pixeles, como un plano."""
    x0, y0 = int(_tesela_x(OESTE)), int(_tesela_y(NORTE))
    return (_tesela_x(lon) - x0) * 256.0, (_tesela_y(lat) - y0) * 256.0


def fmt_trazado(pts):
    return " ".join("{:.1f},{:.1f}".format(*a_pixel(la, lo)) for la, lo in pts)


NOMBRE_TIPO = {C_AUTOPISTA: "autopista", C_TRONCAL: "troncal", C_PRINCIPAL: "principal",
               C_SECUNDARIA: "secundaria", C_TERCIARIA: "terciaria", C_PEATONAL: "peatonal",
               C_CICLORRUTA: "ciclorruta", C_CONECTOR: "conector"}


def letras(permisos):
    """Permisos en texto: C = carro y moto, B = bicicleta, P = peaton, '-' = nadie."""
    s = ("C" if permisos & CARRO else "") + ("B" if permisos & BICI else "") + ("P" if permisos & PIE else "")
    return s or "-"


def escribir_sedes():
    origen = os.path.join(RAIZ, "herramientas", "sedes_latlon.csv")
    with open(origen, encoding="utf-8") as f, \
            open(os.path.join(DATOS, "sedes.csv"), "w", encoding="utf-8", newline="\n") as g:
        g.write("# Sedes. Formato: nombre;direccion;x;y   (x,y = pixel en datos/mapa.png)\n")
        g.write("# Para agregar una sede: abra el programa, ponga el mouse sobre el lugar y copie el x,y\n")
        g.write("# que aparece abajo del mapa.\n")
        for linea in f:
            if linea.startswith("#") or not linea.strip():
                continue
            nombre, direccion, lat, lon = linea.rstrip("\n").split(";")
            x, y = a_pixel(float(lat), float(lon))
            g.write(f"{nombre};{direccion};{x:.0f};{y:.0f}\n")


def nucleo_por_modo(tramos):
    """Para cada modo, la componente fuertemente conexa mas grande (Kosaraju iterativo): desde esos
    cruces se puede ir y volver a cualquier otro respetando los sentidos viales. Las sedes se conectan
    solo a cruces del nucleo, para no quedar en una 'isla' de calles de un solo sentido."""
    nucleo = collections.defaultdict(int)
    for bit in (CARRO, BICI, PIE):
        sig = collections.defaultdict(list)
        ant = collections.defaultdict(list)
        for a, b, _, _, _, ida, vuelta, _ in tramos:
            if ida & bit:
                sig[a].append(b)
                ant[b].append(a)
            if vuelta & bit:
                sig[b].append(a)
                ant[a].append(b)
        nodos = set(sig) | set(ant)
        orden, visto = [], set()
        for s in nodos:
            if s in visto:
                continue
            visto.add(s)
            pila = [(s, iter(sig[s]))]
            while pila:
                u, it = pila[-1]
                avanzo = False
                for v in it:
                    if v not in visto:
                        visto.add(v)
                        pila.append((v, iter(sig[v])))
                        avanzo = True
                        break
                if not avanzo:
                    pila.pop()
                    orden.append(u)
        comp = {}
        mejor, tam_mejor = None, 0
        for s in reversed(orden):
            if s in comp:
                continue
            comp[s] = s
            pila, tam = [s], 0
            while pila:
                u = pila.pop()
                tam += 1
                for v in ant[u]:
                    if v not in comp:
                        comp[v] = s
                        pila.append(v)
            if tam > tam_mejor:
                mejor, tam_mejor = s, tam
        for n, c in comp.items():
            if c == mejor:
                nucleo[n] |= bit | (MOTO if bit == CARRO else 0)
        print(f"     nucleo fuertemente conexo para el bit {bit}: {tam_mejor} cruces")
    return nucleo


def escribir(tramos, coord, estaciones, tm_tramos):
    print("4/4  Escribiendo archivos CSV...")
    usados = set()
    for t in tramos:
        usados.add(t[0])
        usados.add(t[1])
    nucleo = nucleo_por_modo(tramos)
    with open(os.path.join(DATOS, "nodos.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Cruces de la red vial principal (OpenStreetMap). Formato: id;x;y;nucleo_carro;nucleo_bici;nucleo_pie\n")
        f.write("# nucleo_* = 1 si desde ese cruce se puede ir y volver a toda la red en ese modo (respetando sentidos).\n")
        f.write("# Solo esos cruces se usan para conectar las sedes.\n")
        for n in sorted(usados):
            x, y = a_pixel(*coord[n])
            k = nucleo.get(n, 0)
            f.write(f"{n};{x:.1f};{y:.1f};{int(bool(k & CARRO))};{int(bool(k & BICI))};{int(bool(k & PIE))}\n")
    with open(os.path.join(DATOS, "tramos.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Tramos entre cruces. Formato:\n")
        f.write("# origen;destino;metros;tipo;ciclorruta;ida;vuelta;nombre;trazado\n")
        f.write("# ida/vuelta: quien puede ir de origen a destino y de destino a origen (C carro/moto, B bici, P peaton)\n")
        f.write("# trazado: puntos intermedios 'x,y x,y ...' (forma real de la calle, solo para dibujar)\n")
        for a, b, pts, clase, ciclo, ida, vuelta, nombre in tramos:
            f.write(f"{a};{b};{largo_trazado(pts):.0f};{NOMBRE_TIPO[clase]};{ciclo};{letras(ida)};{letras(vuelta)};"
                    f"{nombre};{fmt_trazado(pts[1:-1])}\n")
    with open(os.path.join(DATOS, "tm_estaciones.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Estaciones de TransMilenio (OpenStreetMap). Formato: nombre;x;y\n")
        for nom, la, lo, _ in estaciones:
            x, y = a_pixel(la, lo)
            f.write(f"{nom};{x:.1f};{y:.1f}\n")
    with open(os.path.join(DATOS, "tm_tramos.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Tramos entre estaciones consecutivas de algun servicio. Formato:\n")
        f.write("# estacion_a;estacion_b;metros;servicios;trazado\n")
        for a, b, m, refs, pts in tm_tramos:
            f.write(f"{a};{b};{m:.0f};{refs};{fmt_trazado(pts[1:-1])}\n")
    escribir_sedes()
    print(f"     nodos={len(usados)}  tramos={len(tramos)}  estaciones TM={len(estaciones)}  tramos TM={len(tm_tramos)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--refrescar", action="store_true", help="vuelve a descargar todo ignorando la cache")
    ap.add_argument("--sin-mapa", action="store_true", help="no descarga la imagen de fondo")
    args = ap.parse_args()
    os.makedirs(DATOS, exist_ok=True)

    tramos_todos, coord = construir_red(args.refrescar)
    estaciones, tm_tramos = construir_transmilenio(args.refrescar, tramos_todos, coord)
    tramos = componente_principal(tramos_todos, coord)
    if not args.sin_mapa:
        mapa_de_fondo(args.refrescar)
    escribir(tramos, coord, estaciones, tm_tramos)
    print("Listo.")


if __name__ == "__main__":
    main()
