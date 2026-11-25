# Motor de Enrutamiento y Movilidad Inter-Sedes (C++17 + raylib)

**Universidad Distrital Francisco José de Caldas** — Ciencias de la Computación I
Sebastian Jaramillo Hernandes (20241020002) · Jhoe Luis Jeanpaul Miranda Alvarez (20241020022)

Aplicación de escritorio que calcula la ruta de menor tiempo entre las sedes de la Universidad Distrital
y la Sede S de la ECCI, sobre las calles reales de Bogotá.

- **Modos:** carro, moto, bicicleta, a pie y TransMilenio.
  - En TransMilenio dice qué servicio tomar, dónde bajarse y dónde hacer transbordo.
- **Simulación de un día:**
  - El tráfico cambia según la hora.
  - TransMilenio opera de 4:30 a 23:00.
  - Las marchas y los cierres pueden tener una franja horaria.
- **Contingencias:** marchas, bloqueos, vías cerradas o congestionadas y estaciones cerradas.
  - Se ponen con clic en el mapa o en el archivo de texto `datos/escenario.txt`.
- **Horario de clases:** en `datos/horario.txt`. El programa dice, para cada cambio de sede, en qué
  modos se llega a tiempo a la siguiente clase.

## Estructuras de datos (todas propias)

De la STL solo se usan `std::string`, los flujos de E/S y `std::filesystem` (para saber si un archivo
cambió). raylib solo dibuja y lee el mouse.

| Estructura | Archivo | Para qué se usa | Complejidad |
|---|---|---|---|
| `Grafo` dirigido ponderado: arreglo de vértices, cada uno con una lista enlazada de `Arista` | `Grafo.h/.cpp` | La red: cruces, sedes, estaciones, servicios de TransMilenio | Espacio O(V+E); vecinos O(grado) |
| `MinHeap` indexado (arreglo + `pos[v]`) | `MinHeap.h/.cpp` | Cola de prioridad de Dijkstra, con `disminuirClave` | O(log V) |
| `TablaHash` (encadenamiento, hash polinomial base 31, rehash con carga > 0.75) | `TablaHash.h/.cpp` | Lugar → vértice, `"estación\|servicio"` → vértice, nombre de vía → índice | O(1) promedio |
| `Pila<T>` (lista enlazada) | `Pila.h` | Invertir los predecesores de Dijkstra | O(k) |
| `Arreglo<T>` (arreglo dinámico que duplica su capacidad) | `Arreglo.h` | Reemplaza a `std::vector` | agregar O(1) amortizado |
| Dijkstra | `Enrutador.cpp` | Ruta de menor tiempo | O((V+E) log V) |
| Ordenamiento por inserción | `Horario.cpp` | Ordenar las clases por día y hora | O(n²), n pequeño |

## Organización del código (`src/`)

| Archivo | Contenido |
|---|---|
| `Arreglo.h`, `Pila.h` | Plantillas de arreglo dinámico y pila |
| `MinHeap.*`, `TablaHash.*`, `Grafo.*` | Estructuras de datos |
| `Enrutador.h` | Clase `Enrutador`: une todo. Sus métodos están en tres archivos |
| `Carga.cpp` | Lectura de los CSV, conexión de sedes y estaciones con la red, vértices `estación\|servicio` |
| `Enrutador.cpp` | Tiempo de cada tramo según el modo y la hora, Dijkstra, alternativas e indicaciones |
| `Contingencias.cpp` | Eventos (marchas, bloqueos, cierres) con franja horaria |
| `Escenario.*` | Lectura de `datos/escenario.txt` |
| `Horario.*` | Lectura de `datos/horario.txt` y cálculo de traslados |
| `Utilidades.*` | Separar campos, leer números y horas, distancias |
| `Mapa.*` | Ventana con raylib |
| `Pruebas.*` | 25 pruebas automáticas |
| `main.cpp` | Arranque y menú de consola |

## Cómo funciona

1. **Red vial sin un nodo en cada cruce.**
   - Solo se usan las vías principales, las ciclorrutas y TransMilenio.
   - Un vértice es un cruce entre esas vías. Las curvas de cada calle se guardan como *trazado* (puntos x,y)
     y solo sirven para dibujar.
2. **Tiempo de cada tramo:** `minutos = metros / velocidad × contingencia`.
   - La velocidad depende del tipo de vía, del modo y de la hora.
   - La tabla `TRAFICO_POR_HORA` (24 valores) dice a qué fracción de la velocidad libre va un carro en
     cada hora: por ejemplo 50 % a las 7 a. m. y 100 % de madrugada.
   - La moto sufre menos el tráfico. La bici y el peatón no lo sufren.
   - Todas las constantes están al inicio de `Enrutador.cpp`.
3. **TransMilenio** se modela como en las apps de transporte público:
   - Cada servicio (B74, H13, F19…) es su propia línea en el grafo.
   - Hay un vértice por cada par estación–servicio, por ejemplo `"Ricaurte|F19 (Portal Américas)"`.
   - **Subirse** a un servicio cuesta la espera del bus (4 a 8 min según la hora), así que cada transbordo
     cuesta otra espera. Por eso Dijkstra prefiere rutas con pocos transbordos sin ninguna regla especial.
   - A la calle solo se sale bajándose de un bus: una estación no sirve de atajo peatonal.
4. **Dijkstra** con el `MinHeap` indexado. Es correcto porque ningún tiempo es negativo, y se detiene
   apenas saca el destino del heap.
5. **Rutas alternativas:** se encarecen ×1.5 los tramos de la ruta hallada y se corre Dijkstra otra vez.
6. **Contingencias:**
   - Se guardan como una lista de eventos. Cada cambio de hora o de eventos reconstruye el estado de la red
     desde cero, aplicando solo los eventos activos a esa hora.
7. **Horario:**
   - Se leen las clases y se ordenan por inserción.
   - Para cada par de clases seguidas en sedes distintas se calcula la ruta en los 5 modos, saliendo al
     terminar la primera clase con el tráfico y los eventos de esa hora.

## Archivos de texto (se editan con el programa abierto y todo se recalcula solo)

`datos/escenario.txt`:
```
hora; 07:00
congestion; Avenida Carrera 30; 2; 06:30-09:00     <- la NQS, el doble de tiempo en la hora pico
marcha; Universidad Nacional; 500; 10:00-12:00     <- lugar (sede, estación o "x,y"), radio, franja
cerrar_estacion; Museo Nacional; 11:00-14:00
bloqueo; Sede Aduanilla de Paiba; 300; 16:00-19:00
cerrar_via; Avenida Carrera 7                      <- sin franja = todo el día
```

`datos/horario.txt`:
```
lunes; 07:00; 09:00; Sede Calle 40 Ingenieria; Ciencias de la Computación I
lunes; 09:30; 11:00; Sede Macarena A; Cálculo Integral
```

Si una línea tiene un error, el programa dice cuál y por qué, por ejemplo
`linea 8: no existe la via 'Avenida Inventada'`.

Los nombres de vía son los de OpenStreetMap:
- La Caracas es "Avenida Carrera 14".
- La NQS es "Avenida Carrera 30".
- La Séptima es "Avenida Carrera 7".

## Datos (`datos/`)

Todas las coordenadas están en píxeles de `mapa.png` (1 píxel ≈ 4.76 m).

- `sedes.csv`: `nombre;direccion;x;y`. Para agregar una sede, se pasa el mouse sobre el mapa, se copia el
  `x,y` que aparece abajo y se escribe una línea nueva.
- `nodos.csv`, `tramos.csv`: la red vial.
  - En `tramos.csv`, `ida` y `vuelta` dicen quién puede pasar en cada sentido: `C` carro/moto,
    `B` bici, `P` peatón.
- `tm_estaciones.csv`, `tm_tramos.csv`: estaciones y tramos entre estaciones vecinas, con los servicios
  que pasan por cada tramo.

Se generaron una vez desde OpenStreetMap con `herramientas/preparar_datos.py` (Python). Ese script no
hace parte del programa: solo convierte los datos a CSV.

## Instalar y compilar en otro computador (Windows)

El programa necesita dos cosas: **g++ con C++17** y la librería **raylib**. `enrutador.exe` no está en el
repositorio: se genera al compilar.

### 1. Descargar el proyecto

```
git clone <url-del-repositorio>
```

También sirve *Code → Download ZIP* en GitHub. Hay que descomprimirlo en una carpeta normal; no se puede
ejecutar desde dentro del ZIP.

### 2. Instalar el compilador y raylib (escoger una opción)

**Opción A: instalador de raylib (la más fácil)**

1. Descargar el instalador de Windows de https://raysan5.itch.io/raylib. Para bajarlo sin pagar, clic en
   *Download* y luego en *No thanks, just take me to the downloads*.
2. Instalarlo en la ruta que trae por defecto, `C:\raylib`. Ese instalador trae su propio compilador, así
   que no hay que instalar nada más.

**Opción B: dos descargas a mano (la que usamos nosotros)**

1. Crear la carpeta `C:\Users\<su usuario>\herramientas_c`.
2. **w64devkit** (g++ para Windows): de https://github.com/skeeto/w64devkit/releases bajar
   `w64devkit-x64-<versión>.7z.exe`, ejecutarlo y extraerlo dentro de `herramientas_c`.
   - Debe quedar `herramientas_c\w64devkit\bin\g++.exe`.
3. **raylib**: de https://github.com/raysan5/raylib/releases bajar `raylib-<versión>_win64_mingw-w64.zip`
   y descomprimirlo dentro de `herramientas_c`.
   - Debe quedar `herramientas_c\raylib-<versión>_win64_mingw-w64\include\raylib.h`.

Si g++ ya está instalado de otra forma (por ejemplo MSYS2) y raylib está en otra carpeta, se le indica dónde
está antes de compilar:

```
set RAYLIB_DIR=C:\ruta\a\raylib-6.2_win64_mingw-w64
compilar.bat
```

### 3. Compilar y comprobar

1. Doble clic en **`compilar.bat`**. Compila y abre la ventana.
2. Para comprobar que todo quedó bien, en una terminal dentro de la carpeta del proyecto:

```
enrutador.exe --pruebas
```

Debe terminar con `TODAS LAS PRUEBAS PASARON (0 fallas)`.

### Problemas comunes

| Mensaje o síntoma | Solución |
|---|---|
| `No se encontro g++` | El compilador no está en ninguna de las rutas del paso 2. Revisar que exista `...\w64devkit\bin\g++.exe`. |
| `raylib.h: No such file or directory` | raylib no está donde se busca. Revisar el nombre de la carpeta o usar `set RAYLIB_DIR=...`. |
| La ventana no encuentra los datos | Ejecutar `enrutador.exe` desde la carpeta del proyecto. Busca `datos\` en la carpeta actual, en `..` y en `..\..`. |
| Windows dice que protegió el equipo | Pasa con los ejecutables recién compilados. Clic en *Más información → Ejecutar de todas formas*. |
| VS Code marca errores rojos en el código, pero compila | `compile_flags.txt` tiene las rutas de nuestro computador. Cambiar `C:/Users/User` por la carpeta de su usuario y `16.2.0` por la versión que muestra `g++ -dumpversion`. Solo afecta al editor, no a la compilación. |

### Linux o WSL

Con raylib instalado se compila con `make`, y `make pruebas` corre las pruebas. También está `CMakeLists.txt`.

### Volver a generar los datos (opcional)

No hace falta para usar el programa: los CSV y el mapa ya están en `datos/`. Solo sirve para cambiar la zona o
agregar sedes nuevas desde latitud y longitud.

1. Instalar Python 3 y las librerías: `pip install requests pillow`.
2. Ejecutar `python herramientas/preparar_datos.py`. Descarga todo de OpenStreetMap y tarda varios minutos
   la primera vez.

### Uso

```
enrutador.exe             ventana con el mapa
enrutador.exe --consola   menú de consola (interfaz de respaldo)
enrutador.exe --pruebas   25 pruebas automáticas
```

**Controles:**

- Modo: botones o teclas `1`–`5`.
- Origen y destino: clic en la lista o en la sede del mapa (clic derecho = destino).
- Hora: botones `−`/`+` o flechas ←/→. *Simular día* o la tecla `Espacio` adelantan el reloj 15 min cada 0.4 s.
- Gráfica "Tiempo según la hora de salida": clic en una barra para salir a esa hora.
- Contingencias: *Marcha* (`M`) o *Bloqueo* (`B`) y clic en el mapa; clic dentro de una zona la quita.
  *Estación* (`E`) y clic en una estación la cierra (queda gris); otro clic la abre.
- Pestaña **Mi horario**: clic en un traslado para verlo en el mapa.
- Mapa: rueda para zoom, arrastrar para mover, `F` para centrar. `R` recarga los archivos.

## Limitaciones

- Las calles residenciales no están en el grafo. La conexión entre cada sede y la vía principal más
  cercana es una línea recta, con su distancia alargada un 30 %.
- No se modelan el SITP, los alimentadores ni el pico y placa.
- La tabla de tráfico por hora es una estimación, no son mediciones.
- Los servicios de TransMilenio vienen de OpenStreetMap. Algunos están incompletos, así que unas pocas
  rutas (3 de 380) salen con 3 transbordos.
- La Sede Ensueño tiene una ubicación aproximada.

Mapa y datos: © colaboradores de OpenStreetMap.
