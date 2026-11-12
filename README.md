# genesis-s60

Emulador de **Sega Mega Drive / Genesis** (y Master System / Game Gear) para **Symbian**: una versión para
**S60 3rd Edition** (hecha para el **Nokia N95**) y otra para **Symbian^3 / Anna / Belle** (Nokia N8, C7, E7, 808...)
con OpenGL ES 2.0, pantalla táctil y salida de TV. Usa el núcleo de [PicoDrive](https://github.com/irixxxx/picodrive) con sus partes en asm ARM, y
suma una interfaz propia pensada para el teléfono: menú de ROMs, hasta **4 mandos de PS3/PS4 por Bluetooth** y el
**Team Player** de Sega para los juegos de 4 jugadores (Mega Bomberman, ...).

## Descargar

Los instaladores están en [`sis/`](sis/), con la versión en el nombre:

- **N95** y otros S60 3rd / 5th: [`sis/segaw3d_n95_2.0.sisx`](sis/segaw3d_n95_2.0.sisx)
- **N8** y otros Symbian^3 / Anna / Belle: [`sis/segaw3d_n8_2.0.sisx`](sis/segaw3d_n8_2.0.sisx)

Las dos usan el mismo nombre en el teléfono: una reemplaza a la otra. Están firmados con un certificado propio: el
teléfono tiene que aceptar aplicaciones sin firmar de Nokia (Administrador de aplicaciones > Configuración >
Instalación de software: Todas). Para los mandos hace falta [bthid-s60](https://github.com/Dante-Leoncini/bthid-s60).

## Qué tiene

- **Imagen pixel perfect**: 1:1, centrada y sin estirar, escrita por CPU directo en la memoria de la pantalla
  (Direct Screen Access, en 16 bits), solo en las filas que cambian.
- **Velocidad real con sonido**: la consola corre siempre a 60 cuadros por segundo (50 en las PAL). Si el teléfono
  no llega a dibujarlos todos, saltea el dibujo de algunos, pero el juego y el sonido no se atrasan.
- **Núcleos en asm ARM** (ARMv6): el 68000 (Cyclone), el Z80 (DrZ80), el sonido FM, la mezcla, la memoria y el
  dibujo de planos y sprites.
- **Hasta 4 mandos** de PS3/PS4 con el driver [bthid-s60](https://github.com/Dante-Leoncini/bthid-s60). El Team Player
  se enchufa solo en los juegos que lo traen (lo dice la cabecera del cartucho).
- **Menú de ROMs** y **opciones**: contador de FPS, dibujo preciso o rápido, multitap, frecuencia y calidad del
  sonido, idioma (castellano o inglés; por defecto, el del teléfono).
- **Partidas guardadas**: 8 ranuras por juego, con la fecha y una captura de la pantalla. Y la memoria a pila de los
  cartuchos que la traen (Sonic 3, Phantasy Star...), que se guarda sola.

### En el N8 (Symbian^3 / Anna / Belle)

- **OpenGL ES 2.0** a 60 fps, con la escala a elección: **1:1**, **entera** (x2 para la TV de 480i), **ajustar**
  (ocupa la pantalla sin deformar) o **estirar**; y la imagen **pixelada** (pixel perfect) o **suave** (filtrada).
- **Controles táctiles** de Mega Drive en la pantalla (cruceta, A B C, Start, menú), con varios dedos a la vez. Se
  muestran solos si no hay mandos conectados.
- **Menú táctil**: filas grandes, arrastrar para desplazar, tocar para elegir.
- **Salida de TV** (RCA 480i o HDMI 720p): al enchufar el cable pregunta si clonar la pantalla, mostrar el juego solo
  en la tele o en la tele con los controles táctiles en el teléfono.

En el N95, Mega Bomberman en batalla de 4 anda a 60 fps. Sonic & Knuckles anda cerca de 50 con el dibujo preciso.

## Controles

| | Mando PS3/PS4 | Teclado del teléfono |
|---|---|---|
| Cruceta | cruceta o stick izquierdo | flechas |
| A / B / C | cuadrado / cruz / círculo | OK o 1 / 2 / 3 |
| X / Y / Z | L1 / triángulo / R1 | 4 / 5 / 6 |
| Start | Start | tecla izquierda de abajo (o 0) |
| Mode | L2 | 7 |
| Pausa y menú de ROMs | Select o PS | tecla derecha de abajo (o C) |

Así vienen de fábrica; se cambian en el menú, en **Controles**.

**Jugando, la tecla derecha de abajo pausa** y abre el menú: guardar la partida, cargar una guardada, cargar otra ROM,
los controles, el volumen y las demás opciones, volver al juego y cerrar el emulador. La misma tecla vuelve al juego.
Sin un juego abierto, el menú es la lista de las ROMs (OK para jugar, **Start** para las opciones). Las teclas de
volumen del costado también cambian el volumen.

**Las partidas**: en "Guardar partida" se elige la ranura (a la derecha se ve la captura de lo que tiene) y OK
guarda. "Cargar partida guardada", o elegir en la lista un juego que tiene partidas, muestra "Partida nueva" y las
guardadas, con su fecha y hora. En el N8, tocar una fila la elige y tocarla otra vez guarda o juega. Las partidas
quedan en `\segaw3d\datos\partidas\`.

**Controles**: se elige el jugador (1 a 4) y aparecen los botones de la Mega Drive con lo que tiene cada uno (las
teclas del teléfono y, después de la barra, el botón del mando). OK en un botón y después se aprieta la tecla o el
botón del mando nuevo; la tecla derecha de abajo cancela (no se puede asignar: es la del menú, igual que PS / Home).
Select del mando va al menú mientras no tenga un botón asignado. El menú se maneja siempre con los controles de
fábrica. Quedan en `\segaw3d\datos\controles.txt`.

**Wiimote** (con bthid-s60 0.10 o más nuevo): 1 / A / 2 = A / B / C, + = Start, Home o - = menú. Para usarlo
acostado, en la app Bluetooth HID se pueden girar sus flechas (0.12). La opción **Inclinar el Wiimote** lo usa como
volante: acostado con la cruceta a la izquierda, derecho no aprieta nada; inclinado va apretando izquierda o
derecha, más seguido cuanto más se inclina, y bien inclinado (unos 33°) queda apretado. Se suma a la cruceta:
inclinado a la derecha y abajo es la diagonal. Si la cruceta aprieta izquierda o derecha, mandan sus costados.

## Las ROMs

Las ROMs **no vienen** con el emulador. Van en `E:\Data\sega\` (`.bin`, `.md`, `.gen`, `.smd`, `.sms`, `.gg` o un
`.zip` con una adentro). La carpeta se cambia en `\segaw3d\datos\config.txt` (`carpeta=...`), en la unidad donde se
instaló.

## Compilar

El árbol supone el ecosistema de [Whisk3D](https://github.com/Dante-Leoncini/Whisk3D-Editor): el Core de Whisk3D (el
log, el mezclador de sonido, los mandos) se toma de `../../Whisk3D/libs/Whisk3DCore`.

```
git clone --recursive https://github.com/Dante-Leoncini/genesis-s60.git
```

### En la PC (para probar)

MSYS2 (mingw64) con SDL2. Usa los núcleos de PicoDrive en C:

```
make
build/segaw3d_pc --datos carpeta/con/roms/
```

Teclado: flechas, Z X C = A B C, Enter = Start, A S D = X Y Z, Borrar = menú, Re Pág / Av Pág = volumen.

### Para el N95 y el N8

Hace falta el SDK de S60 en `C:\epoc32`, el RVCT 2.2, el gcc de Symbian de CodeSourcery (para los .S de PicoDrive,
que están en la sintaxis de GNU as), el g++ de MSYS2 (para generar Cyclone) y Python.

```
python herramientas/asm_symbian.py     los núcleos en asm: genera Cyclone.s, ensambla y deja pdasm.lib en \epoc32
python herramientas/pd_symbian.py      los envoltorios del núcleo en C para abld (symbian/pd/)
symbian\compilar.bat                   segaw3d.exe (N95) y segaw3d_belle.exe (N8): RVCT 2.2, ARMv6 + VFP
python symbian/sis/armar_pkg.py        el instalador del N95: build/segaw3d.sisx (la firma se crea la primera vez)
python symbian/sis/armar_pkg.py --belle      el del N8: build/segaw3d_belle.sisx
python symbian/sis/armar_pkg.py --publicar   los dos, con la versión, en sis/
```

La versión está en `src/version.h` (la usan el menú y el instalador).

Después de cambiar las macros del `.mmp` o de actualizar PicoDrive: `symbian\compilar.bat limpio`.

`symbian/probar.py` instala por App TRK y baja el log con el perfil (`symbian/trk.py`).

## Estructura

- `src/`: el emulador. `emu.c` (el núcleo visto desde el juego: imagen, sonido, mandos), `juego.cpp` (el reloj de
  la consola, los jugadores, el menú, el perfil), `menu.cpp`, `tactil.cpp` (los controles en la pantalla), `pantalla.cpp` (la PC, con OpenGL).
- `src/pc/`: la prueba en la PC con SDL2.
- `symbian/`: la aplicación de S60 (`src/`: la ventana, el Direct Screen Access, las teclas; en el N8, OpenGL ES 2.0,
  el táctil y la TV), los `.mmp` (`segaw3d_belle.mmp` es el del N8), el instalador y las pruebas por cable.
- `sis/`: los instaladores para bajar.
- `herramientas/`: lo que arma el núcleo para Symbian sin tocar `picodrive/`: el asm (`asm_symbian.py`), las
  posiciones de las estructuras para el asm (`offsets_symbian.py`, con el RVCT), los envoltorios (`pd_symbian.py`) y
  la tabla de cartuchos (`carthw_cfg.py`).
- `picodrive/`: PicoDrive, como submódulo, sin cambios.

## Licencia

El núcleo es PicoDrive y sigue su licencia (`picodrive/COPYING`): se puede repartir gratis, pero **no vender ni usar
en algo comercial**, y una versión modificada tiene que repartirse con todo su código fuente.
