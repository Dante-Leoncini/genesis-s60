# genesis-s60

Emulador de **Sega Mega Drive / Genesis** (y Master System / Game Gear) para **Symbian S60 3rd Edition**, hecho para
el **Nokia N95**. Usa el núcleo de [PicoDrive](https://github.com/irixxxx/picodrive) con sus partes en asm ARM, y
suma una interfaz propia pensada para el teléfono: menú de ROMs, hasta **4 mandos de PS3/PS4 por Bluetooth** y el
**Team Player** de Sega para los juegos de 4 jugadores (Mega Bomberman, ...).

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
  sonido.

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

En el menú: arriba y abajo para elegir, OK / cruz para jugar, **Start** para las opciones. La tecla derecha de abajo
vuelve a la partida y la última fila, "[ Salir ]", cierra el emulador. Las teclas de volumen del costado cambian el
volumen.

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

### Para el N95

Hace falta el SDK de S60 en `C:\epoc32`, el RVCT 2.2, el gcc de Symbian de CodeSourcery (para los .S de PicoDrive,
que están en la sintaxis de GNU as), el g++ de MSYS2 (para generar Cyclone) y Python.

```
python herramientas/asm_symbian.py     los núcleos en asm: genera Cyclone.s, ensambla y deja pdasm.lib en \epoc32
python herramientas/pd_symbian.py      los envoltorios del núcleo en C para abld (symbian/pd/)
symbian\compilar.bat                   segaw3d.exe (RVCT 2.2, ARMv6 + VFP)
python symbian/sis/armar_pkg.py        el instalador: build/segaw3d.sisx (la firma se crea la primera vez)
```

Después de cambiar las macros del `.mmp` o de actualizar PicoDrive: `symbian\compilar.bat limpio`.

`symbian/probar.py` instala por App TRK y baja el log con el perfil (`symbian/trk.py`).

## Estructura

- `src/`: el emulador. `emu.c` (el núcleo visto desde el juego: imagen, sonido, mandos), `juego.cpp` (el reloj de
  la consola, los jugadores, el menú, el perfil), `menu.cpp`, `pantalla.cpp` (la PC, con OpenGL).
- `src/pc/`: la prueba en la PC con SDL2.
- `symbian/`: la aplicación de S60 (`src/`: la ventana, el Direct Screen Access, las teclas), el `.mmp`, el
  instalador y las pruebas por cable.
- `herramientas/`: lo que arma el núcleo para Symbian sin tocar `picodrive/`: el asm (`asm_symbian.py`), las
  posiciones de las estructuras para el asm (`offsets_symbian.py`, con el RVCT), los envoltorios (`pd_symbian.py`) y
  la tabla de cartuchos (`carthw_cfg.py`).
- `picodrive/`: PicoDrive, como submódulo, sin cambios.

## Licencia

El núcleo es PicoDrive y sigue su licencia (`picodrive/COPYING`): se puede repartir gratis, pero **no vender ni usar
en algo comercial**, y una versión modificada tiene que repartirse con todo su código fuente.
