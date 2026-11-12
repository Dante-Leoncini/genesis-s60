#include "menu.h"
#include "controles.h"
#include "juego.h"
#include "pantalla.h"
#include "version.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" unsigned char fontdata8x8[64 * 16];   // (picodrive/platform/libpicofe/fonts.c: 128 letras de 8x8)

namespace {
const int MAX_ROMS = 512;

// LAS PANTALLAS: la lista de las ROMs (abajo de todo), las opciones (con un juego cargado, la pausa) y encima de
// ellas, las partidas y los controles (al cerrarlas se vuelve a la de abajo)

// las filas de la lista: las ROMs y "Salir" (la ultima, con teclas)
enum { FILA_ROM, FILA_SALIR };
struct Rom {
    char ruta[256];
    char nombre[64];
    int tipo;   // FILA_*
};
Rom* gRoms = 0;
int gCantidad = 0;        // (con la fila de "Salir", si esta)
bool gFilaSalir = false;  // la ultima fila es "Salir" (con teclas; con el dedo es un boton del pie)
char gRomJuego[256] = "";   // la ROM del juego cargado
char gJugar[256] = "";      // la que hay que jugar (MENU_JUGAR)

// LAS PARTIDAS: la pantalla que elige la ranura (para guardar, o para cargar). Sus filas son ranuras (-1 = partida
// nueva); a la derecha, la captura de la elegida
const MenuFuentePartidas* gFuente = 0;
bool gEnPartidas = false, gPartGuardar = false;
char gPartRom[256] = "";
MenuPartida gPart[MENU_RANURAS];
int gPartFilas[MENU_RANURAS + 1];
int gPartN = 0;
int gRanura = -1;
uint16_t gMini[MENU_MINI_ANCHO * MENU_MINI_ALTO];
int gMiniW = 0, gMiniH = 0, gMiniDe = -2;   // (gMiniDe: la ranura de la captura cargada; -2 = ninguna)

// LOS CONTROLES: primero el jugador, despues sus botones; OK en un boton espera la tecla nueva
bool gEnControles = false;
int gCtrlJugador = -1;   // -1 = la lista de los jugadores
bool gEsperando = false;
enum { CTRL_FILA_FABRICA = CTRL_BOTONES, CTRL_FILA_VOLVER };

// LA PREGUNTA DE LA TELE: tres filas (clonar, solo la tele, tele + controles); encima de todo
bool gEnTv = false;
int gTvSalida = 0;
char gTvSolo[40] = "";
const int kTvSalidas[3] = { SALIDA_CLONAR, SALIDA_TV, SALIDA_TV_CONTROLES };

// el aviso ("Partida guardada"): con teclas en el pie, con el dedo en la cabecera
char gAviso[48] = "";
uint64_t gAvisoHasta = 0;
bool gEspanol = true;     // el idioma del sistema
bool gHayJuego = false;
bool gCambio = true;
char gCarpeta[256] = "";
int gMandos = 0;
// la repeticion de arriba/abajo mantenidos
int gAntes = 0;
uint64_t gRepetirEn = 0;

// EL TAMANO: la imagen, el alto de una fila, la cabecera y el pie. Con el dedo: filas altas y botones
int gW = MENU_ANCHO, gH = MENU_ALTO, gFila = 9, gCab = 15, gPie = 14;
bool gTactil = false, gPantalla = false;

// UNA LISTA: cual esta elegida y cuanto se corrio (en pixeles)
struct Lista {
    int elegida;
    int desp;
};
Lista gRomsL = { 0, 0 }, gOpcL = { 0, 0 }, gPartL = { 0, 0 }, gCtrlL = { 0, 0 }, gTvL = { 1, 0 };

// las opciones (con un juego cargado, la pausa: arriba las acciones de la partida)
MenuOpciones* gOpciones = 0;
bool gEnOpciones = false;
enum { OPC_GUARDAR, OPC_CARGAR, OPC_ROMS, OPC_CONTROLES, OPC_VOLUMEN, OPC_FPS, OPC_ESCALA, OPC_SUAVE, OPC_TACTIL,
       OPC_DIBUJO, OPC_MULTITAP, OPC_INCLINACION, OPC_HZ, OPC_FM, OPC_FILTRO, OPC_IDIOMA, OPC_VOLVER, OPC_CERRAR };
int gOpcs[24];   // las que se muestran (OPC_*), en orden
int gOpcsN = 0;

// el dedo
int gToqueX0 = 0, gToqueY0 = 0, gDespIni = 0, gZona = 0;
bool gArrastra = false;
enum { ZONA_CAB, ZONA_LISTA, ZONA_PIE };

// los colores (RGB565)
const uint16_t FONDO = 0x0841, TITULO = 0xFFE0, TEXTO = 0xC618, ELEGIDA = 0xFFFF, BARRA = 0x2A7F, PIE = 0x8410,
               BOTON = 0x3186;

bool termina_en(const char* s, const char* ext) {
    const size_t n = strlen(s), m = strlen(ext);
    if (n < m) return false;
    for (size_t i = 0; i < m; i++)
        if (tolower((unsigned char)s[n - m + i]) != ext[i]) return false;
    return true;
}

bool es_rom(const char* nombre) {
    static const char* const ext[] = { ".md", ".bin", ".gen", ".smd", ".sms", ".gg", ".zip", ".68k", ".sgd", 0 };
    for (int i = 0; ext[i]; i++)
        if (termina_en(nombre, ext[i])) return true;
    return false;
}

int comparar(const void* a, const void* b) {
    const char* x = ((const Rom*)a)->nombre;
    const char* y = ((const Rom*)b)->nombre;
    for (;; x++, y++) {
        const int cx = tolower((unsigned char)*x), cy = tolower((unsigned char)*y);
        if (cx != cy || !cx) return cx - cy;
    }
}

void texto(uint16_t* img, int x, int y, const char* s, uint16_t color, int maximo = 40) {
    char corto[64];
    strncpy(corto, s, sizeof(corto) - 1);
    corto[sizeof(corto) - 1] = 0;
    if (maximo < (int)sizeof(corto)) corto[maximo] = 0;
    menu_texto(img, gW, gH, x, y, corto, color);
}

// un rectangulo recortado a la imagen y a las filas [y0, y1)
void rectangulo(uint16_t* img, int x, int y, int w, int h, uint16_t color, int y0 = 0, int y1 = 1 << 30) {
    if (y0 < 0) y0 = 0;
    if (y1 > gH) y1 = gH;
    for (int j = y; j < y + h; j++) {
        if (j < y0 || j >= y1) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < gW) img[j * gW + i] = color;
    }
}

// un boton de la cabecera o del pie (con el dedo): un recuadro con el texto al centro
void boton(uint16_t* img, int x, int y, int w, int h, const char* s) {
    rectangulo(img, x, y, w, h, BOTON);
    texto(img, x + (w - (int)strlen(s) * 8) / 2, y + (h - 8) / 2, s, ELEGIDA);
}

const char* tr(const char* es, const char* en) { return menu_tr(es, en); }
const char* si_no(bool b) { return b ? tr("SI", "YES") : "NO"; }

const char* multitap_nombre(int m) {
    return m == MULTITAP_SIEMPRE ? tr("SIEMPRE", "ALWAYS") : m == MULTITAP_NUNCA ? tr("NUNCA", "NEVER") : "AUTO";
}
const char* escala_nombre(int e) {
    static const char* const es[ESCALAS] = { "1:1", "ENTERA", "AJUSTAR", "ESTIRAR" };
    static const char* const en[ESCALAS] = { "1:1", "INTEGER", "FIT", "STRETCH" };
    return e >= 0 && e < ESCALAS ? tr(es[e], en[e]) : "?";
}
const char* tactil_nombre(int t) { return t == TACTIL_SI ? si_no(true) : t == TACTIL_NO ? "NO" : "AUTO"; }
const char* idioma_nombre(int i) { return i == IDIOMA_ES ? "ESPANOL" : i == IDIOMA_EN ? "ENGLISH" : "AUTO"; }

// la zona de las filas y cuantas filas entran
int lista_y0() { return gCab + 2; }
int lista_alto() { return gH - gCab - gPie - 4; }

// que la elegida se vea (las teclas): el desplazamiento justo
void seguir(Lista& l, int n) {
    const int alto = lista_alto(), top = l.elegida * gFila;
    if (top < l.desp) l.desp = top;
    if (top + gFila > l.desp + alto) l.desp = top + gFila - alto;
    const int maximo = n * gFila - alto;
    if (l.desp > maximo) l.desp = maximo;
    if (l.desp < 0) l.desp = 0;
}

// el desplazamiento dentro de los bordes (el dedo)
void recortar(Lista& l, int n) {
    const int maximo = n * gFila - lista_alto();
    if (l.desp > maximo) l.desp = maximo;
    if (l.desp < 0) l.desp = 0;
}

// la repeticion: true si el movimiento cuenta este cuadro (al apretar; mantenido, despues de 0,35 s cada 0,07 s)
bool mover(int nuevos, int mueve, uint64_t ahora) {
    if (nuevos & mueve) {
        gRepetirEn = ahora + 350000;
        return true;
    }
    if (mueve && ahora >= gRepetirEn) {
        gRepetirEn = ahora + 70000;
        return true;
    }
    return false;
}

// arriba / abajo en una lista de n filas (dan la vuelta)
void mover_lista(Lista& l, int n, int mueve) {
    if (n <= 0) return;
    l.elegida = (l.elegida + ((mueve & MENU_ABAJO) ? 1 : n - 1)) % n;
    seguir(l, n);
    gCambio = true;
}

// ------------------------------------------------------------------------------------------- las partidas
// el ancho de las filas en la pantalla de las partidas (a la derecha va la captura)
int partidas_ancho() { return gW - MENU_MINI_ANCHO - 8; }

void abrir_partidas(bool guardar, const char* rom) {
    gEnPartidas = true;
    gPartGuardar = guardar;
    gCambio = true;
    strncpy(gPartRom, rom, sizeof(gPartRom) - 1);
    gPartRom[sizeof(gPartRom) - 1] = 0;
    memset(gPart, 0, sizeof(gPart));
    if (gFuente) gFuente->listar(gPartRom, gPart);
    gPartN = 0;
    if (!guardar) gPartFilas[gPartN++] = -1;   // (cargar: primero, la partida nueva; despues, las guardadas)
    // (guardar: todas las ranuras, la primera libre elegida; cargar: las guardadas, la mas nueva elegida)
    int elegida = -1;
    for (int r = 0; r < MENU_RANURAS; r++) {
        if (!guardar && !gPart[r].usada) continue;
        if (guardar && elegida < 0 && !gPart[r].usada) elegida = gPartN;
        gPartFilas[gPartN++] = r;
    }
    if (!guardar) {
        elegida = gPartN > 1 ? 1 : 0;
        for (int i = 1; i < gPartN; i++) {
            const int* a = gPart[gPartFilas[i]].fecha;
            const int* b = gPart[gPartFilas[elegida]].fecha;
            int k = 0;
            while (k < 5 && a[k] == b[k]) k++;
            if (a[k] > b[k]) elegida = i;
        }
    }
    gPartL.elegida = elegida < 0 ? 0 : elegida;
    gPartL.desp = 0;
    seguir(gPartL, gPartN);
    gMiniDe = -2;
}

// la captura de la fila elegida (se lee al cambiar de fila)
void cargar_mini() {
    const int r = gPartFilas[gPartL.elegida];
    if (r == gMiniDe) return;
    gMiniDe = r;
    gMiniW = gMiniH = 0;
    if (r >= 0 && gPart[r].usada && gFuente && !gFuente->captura(gPartRom, r, gMini, &gMiniW, &gMiniH)) gMiniW = gMiniH = 0;
    if (gMiniW > MENU_MINI_ANCHO || gMiniH > MENU_MINI_ALTO) gMiniW = gMiniH = 0;
}

void partida_texto(int fila, char* s, int n) {
    const int r = gPartFilas[fila];
    if (r < 0) {
        snprintf(s, n, "%s", tr("Partida nueva", "New game"));
        return;
    }
    const MenuPartida& p = gPart[r];
    if (!p.usada) snprintf(s, n, "%d  %s", r + 1, tr("(libre)", "(empty)"));
    else
        snprintf(s, n, "%d  %02d/%02d/%02d %02d:%02d", r + 1, p.fecha[2], p.fecha[1], p.fecha[0] % 100, p.fecha[3],
                 p.fecha[4]);
}

// elegir la fila de las partidas: guardar ahi, o jugar (nueva o cargando esa ranura)
MenuAccion elegir_partida(int fila) {
    gRanura = gPartFilas[fila];
    gEnPartidas = false;
    gCambio = true;
    if (gPartGuardar) return MENU_GUARDAR;
    snprintf(gJugar, sizeof(gJugar), "%s", gPartRom);
    return MENU_JUGAR;
}

MenuAccion actualizar_partidas(int botones, int nuevos, uint64_t ahora) {
    if (nuevos & (MENU_OPCIONES | MENU_VOLVER)) {
        gEnPartidas = false;
        gCambio = true;
        return MENU_NADA;
    }
    if (nuevos & MENU_ELEGIR) return gPartN ? elegir_partida(gPartL.elegida) : MENU_NADA;
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO);
    if (mover(nuevos, mueve, ahora)) mover_lista(gPartL, gPartN, mueve);
    return MENU_NADA;
}

// ------------------------------------------------------------------------------------------- los controles
int controles_filas() { return gCtrlJugador < 0 ? CTRL_JUGADORES + 1 : CTRL_BOTONES + 2; }

void abrir_controles() {
    gEnControles = true;
    gCtrlJugador = -1;
    gEsperando = false;
    gCtrlL.elegida = 0;
    gCtrlL.desp = 0;
    gCambio = true;
}

// volver un paso: de los botones a los jugadores, de los jugadores a las opciones
void controles_atras() {
    gEsperando = false;
    gCambio = true;
    if (gCtrlJugador >= 0) {
        gCtrlL.elegida = gCtrlJugador;
        gCtrlJugador = -1;
        gCtrlL.desp = 0;
        seguir(gCtrlL, controles_filas());
    } else {
        gEnControles = false;
    }
}

MenuAccion elegir_control(int fila) {
    gCambio = true;
    if (gCtrlJugador < 0) {
        if (fila >= CTRL_JUGADORES) {
            controles_atras();
            return MENU_NADA;
        }
        gCtrlJugador = fila;
        gCtrlL.elegida = 0;
        gCtrlL.desp = 0;
        return MENU_NADA;
    }
    if (fila == CTRL_FILA_VOLVER) {
        controles_atras();
        return MENU_NADA;
    }
    if (fila == CTRL_FILA_FABRICA) {
        controles_defecto(gCtrlJugador);
        return MENU_CAMBIO_CONTROLES;
    }
    gEsperando = true;
    controles_esperar_empezar();
    return MENU_NADA;
}

MenuAccion actualizar_controles(int botones, int nuevos, uint64_t ahora) {
    if (gEsperando) {
        const int r = controles_esperar(gCtrlJugador, gCtrlL.elegida);
        if (!r) return MENU_NADA;
        gEsperando = false;
        gCambio = true;
        return r > 0 ? MENU_CAMBIO_CONTROLES : MENU_NADA;
    }
    if (nuevos & (MENU_OPCIONES | MENU_VOLVER)) {
        controles_atras();
        return MENU_NADA;
    }
    if (nuevos & MENU_ELEGIR) return elegir_control(gCtrlL.elegida);
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO);
    if (mover(nuevos, mueve, ahora)) mover_lista(gCtrlL, controles_filas(), mueve);
    return MENU_NADA;
}

void control_texto(int fila, char* nombre, char* valor, int n) {
    valor[0] = 0;
    if (gCtrlJugador < 0) {
        if (fila < CTRL_JUGADORES) snprintf(nombre, n, "%s %d", tr("Jugador", "Player"), fila + 1);
        else snprintf(nombre, n, "%s", tr("Volver", "Back"));
        return;
    }
    if (fila == CTRL_FILA_FABRICA) snprintf(nombre, n, "%s", tr("Restaurar los de fabrica", "Restore defaults"));
    else if (fila == CTRL_FILA_VOLVER) snprintf(nombre, n, "%s", tr("Volver", "Back"));
    else {
        snprintf(nombre, n, "%s", controles_nombre(fila));
        if (gEsperando && fila == gCtrlL.elegida) snprintf(valor, n, "%s", tr("...apretar", "...press"));
        else controles_texto(gCtrlJugador, fila, !gTactil, valor, n);
    }
}

// ------------------------------------------------------------------------------------------- la tele
const char* tv_texto(int fila) {
    if (fila == 1) return gTvSolo;
    return fila == 0 ? tr("Clonar la pantalla", "Mirror the screen") : tr("Tele + controles", "TV + controls");
}

MenuAccion elegir_tv(int fila) {
    gEnTv = false;
    gCambio = true;
    gTvSalida = kTvSalidas[fila < 0 || fila > 2 ? 0 : fila];
    return MENU_TV;
}

MenuAccion actualizar_tv(int botones, int nuevos, uint64_t ahora) {
    if (nuevos & MENU_VOLVER) return elegir_tv(0);
    if (nuevos & MENU_ELEGIR) return elegir_tv(gTvL.elegida);
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO);
    if (mover(nuevos, mueve, ahora)) mover_lista(gTvL, 3, mueve);
    return MENU_NADA;
}

// ------------------------------------------------------------------------------------------- las opciones
// las que se muestran: con un juego cargado, primero las de la partida; las de pantalla solo donde se pueden elegir
void armar_opciones() {
    gOpcsN = 0;
    if (gHayJuego) {
        gOpcs[gOpcsN++] = OPC_GUARDAR;
        gOpcs[gOpcsN++] = OPC_CARGAR;
        gOpcs[gOpcsN++] = OPC_ROMS;
    }
    gOpcs[gOpcsN++] = OPC_CONTROLES;
    gOpcs[gOpcsN++] = OPC_VOLUMEN;
    gOpcs[gOpcsN++] = OPC_FPS;
    if (gPantalla) {
        gOpcs[gOpcsN++] = OPC_ESCALA;
        gOpcs[gOpcsN++] = OPC_SUAVE;
    }
    if (gTactil) gOpcs[gOpcsN++] = OPC_TACTIL;
    gOpcs[gOpcsN++] = OPC_DIBUJO;
    gOpcs[gOpcsN++] = OPC_MULTITAP;
    gOpcs[gOpcsN++] = OPC_INCLINACION;
    gOpcs[gOpcsN++] = OPC_HZ;
    gOpcs[gOpcsN++] = OPC_FM;
    gOpcs[gOpcsN++] = OPC_FILTRO;
    gOpcs[gOpcsN++] = OPC_IDIOMA;
    gOpcs[gOpcsN++] = OPC_VOLVER;
    gOpcs[gOpcsN++] = OPC_CERRAR;
}

// las que no tienen un valor: se eligen solo con OK
bool es_accion(int o) {
    return o == OPC_GUARDAR || o == OPC_CARGAR || o == OPC_ROMS || o == OPC_CONTROLES || o == OPC_VOLVER ||
           o == OPC_CERRAR;
}

// salir de las opciones: con un juego, a la partida; si no, a la lista de las ROMs
MenuAccion opciones_volver() {
    gCambio = true;
    if (gHayJuego) return MENU_SEGUIR;
    gEnOpciones = false;
    return MENU_NADA;
}

// cambia la opcion 'o' un paso (+1 / -1), o hace lo que dice
MenuAccion cambiar_opcion(int o, int paso) {
    gCambio = true;
    switch (o) {
    case OPC_CERRAR: return MENU_SALIR;
    case OPC_VOLVER: return opciones_volver();
    case OPC_GUARDAR: abrir_partidas(true, gRomJuego); return MENU_NADA;
    case OPC_CARGAR: abrir_partidas(false, gRomJuego); return MENU_NADA;
    case OPC_ROMS: gEnOpciones = false; return MENU_NADA;
    case OPC_CONTROLES: abrir_controles(); return MENU_NADA;
    default: break;
    }
    if (!gOpciones) return MENU_NADA;
    MenuOpciones& p = *gOpciones;
    switch (o) {
    case OPC_VOLUMEN:
        p.volumen += paso;
        if (p.volumen < 0) p.volumen = 0;
        if (p.volumen > 10) p.volumen = 10;
        break;
    case OPC_FPS: p.fps = !p.fps; break;
    case OPC_ESCALA: p.escala = (p.escala + ESCALAS + paso) % ESCALAS; break;
    case OPC_SUAVE: p.suave = !p.suave; break;
    case OPC_TACTIL: p.tactil = (p.tactil + 3 + paso) % 3; break;
    case OPC_DIBUJO: p.rapido = !p.rapido; break;
    case OPC_MULTITAP: p.multitap = (p.multitap + 3 + paso) % 3; break;
    case OPC_INCLINACION: p.inclinacion = !p.inclinacion; break;
    case OPC_HZ: p.hz = (p.hz + 3 + paso) % 3; break;
    case OPC_FM: p.fmAlta = !p.fmAlta; break;
    case OPC_FILTRO: p.filtro = !p.filtro; break;
    case OPC_IDIOMA: p.idioma = (p.idioma + 3 + paso) % 3; break;
    default: return MENU_NADA;
    }
    return MENU_CAMBIO_OPCIONES;
}

// el texto de la opcion y su valor
void opcion_texto(int o, char* nombre, char* valor, int n) {
    static const char* const kHz[3] = { "22 kHz", "32 kHz", "44 kHz" };
    const MenuOpciones* p = gOpciones;
    char vol[24];
    valor[0] = 0;
    const char* k = "";
    const char* v = "";
    switch (o) {
    case OPC_GUARDAR: k = tr("Guardar partida", "Save game"); break;
    case OPC_CARGAR: k = tr("Cargar partida guardada", "Load saved game"); break;
    case OPC_ROMS: k = tr("Cargar otra ROM", "Load another ROM"); break;
    case OPC_CONTROLES: k = tr("Controles", "Controls"); break;
    case OPC_VOLUMEN: {
        // (una barrita: 10 rayas)
        const int n10 = p ? p->volumen : 10;
        for (int i = 0; i < 10; i++) vol[i] = i < n10 ? '#' : '.';
        snprintf(vol + 10, sizeof(vol) - 10, " %2d", n10);
        k = tr("Volumen", "Volume");
        v = vol;
        break;
    }
    case OPC_FPS: k = tr("Contador de FPS", "FPS counter"); v = si_no(p && p->fps); break;
    case OPC_ESCALA: k = tr("Escala", "Scale"); v = escala_nombre(p ? p->escala : 0); break;
    case OPC_SUAVE: k = tr("Imagen", "Image"); v = p && p->suave ? tr("SUAVE", "SMOOTH") : tr("PIXELADA", "PIXELATED"); break;
    case OPC_TACTIL: k = tr("Controles en pantalla", "On-screen controls"); v = tactil_nombre(p ? p->tactil : 0); break;
    case OPC_DIBUJO:
        k = tr("Dibujo", "Rendering");
        v = p && p->rapido ? tr("RAPIDO", "FAST") : tr("PRECISO", "ACCURATE");
        break;
    case OPC_MULTITAP: k = tr("Multitap (4 jug.)", "Multitap (4 pl.)"); v = multitap_nombre(p ? p->multitap : 0); break;
    case OPC_INCLINACION: k = tr("Inclinar el Wiimote", "Wiimote tilt"); v = si_no(p && p->inclinacion); break;
    case OPC_HZ: k = tr("Sonido", "Sound"); v = kHz[p ? p->hz : 0]; break;
    case OPC_FM: k = tr("FM alta calidad", "High quality FM"); v = si_no(p && p->fmAlta); break;
    case OPC_FILTRO: k = tr("Filtro de sonido", "Sound filter"); v = si_no(p && p->filtro); break;
    case OPC_IDIOMA: k = "Idioma / Language"; v = idioma_nombre(p ? p->idioma : 0); break;
    case OPC_CERRAR: k = tr("Cerrar emulador", "Exit emulator"); break;
    default: k = gHayJuego ? tr("Volver al juego", "Back to game") : tr("Volver", "Back"); break;
    }
    snprintf(nombre, n, "%s", k);
    snprintf(valor, n, "%s", v);
}

MenuAccion actualizar_opciones(int botones, int nuevos, uint64_t ahora) {
    if (nuevos & (MENU_OPCIONES | MENU_VOLVER)) return opciones_volver();
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO | MENU_IZQ | MENU_DER);
    const bool elegir = (nuevos & MENU_ELEGIR) != 0;
    if (!elegir && !mover(nuevos, mueve, ahora)) return MENU_NADA;
    gCambio = true;
    if (!elegir && (mueve & (MENU_ARRIBA | MENU_ABAJO))) {
        mover_lista(gOpcL, gOpcsN, mueve);
        return MENU_NADA;
    }
    // OK, izquierda o derecha cambian la opcion elegida (las acciones, solo con OK)
    const int o = gOpcs[gOpcL.elegida];
    if (!elegir && es_accion(o)) return MENU_NADA;
    return cambiar_opcion(o, (mueve & MENU_IZQ) ? -1 : 1);
}

// ------------------------------------------------------------------------------------------- las ROMs
// elegir una fila de la lista de las ROMs
MenuAccion elegir_rom(int i) {
    if (i < 0 || i >= gCantidad) return MENU_NADA;
    const Rom& r = gRoms[i];
    if (r.tipo == FILA_SALIR) return MENU_SALIR;
    // (si la ROM tiene partidas guardadas, primero se elige: nueva o una de esas)
    MenuPartida p[MENU_RANURAS];
    if (gFuente && gFuente->listar(r.ruta, p) > 0) {
        abrir_partidas(false, r.ruta);
        return MENU_NADA;
    }
    gRanura = -1;
    snprintf(gJugar, sizeof(gJugar), "%s", r.ruta);
    return MENU_JUGAR;
}

// ------------------------------------------------------------------------------------------- el dibujo
// LAS FILAS de una lista (con el desplazamiento, recortadas a la zona de la lista)
enum { FILAS_ROMS, FILAS_OPCIONES, FILAS_PARTIDAS, FILAS_CONTROLES, FILAS_TV };
void dibujar_filas(uint16_t* img, const Lista& l, int n, int cuales) {
    const int y0 = lista_y0(), y1 = y0 + lista_alto();
    const int ancho = cuales == FILAS_PARTIDAS ? partidas_ancho() : gW;
    char nombre[64], valor[40];
    for (int i = 0; i < n; i++) {
        const int y = y0 + i * gFila - l.desp;
        if (y + gFila <= y0 || y >= y1) continue;
        const bool elegida = i == l.elegida;
        if (elegida) rectangulo(img, 0, y, ancho, gFila - 1, BARRA, y0, y1);
        else if (gTactil) rectangulo(img, 0, y + gFila - 1, ancho, 1, 0x18E3, y0, y1);   // (una raya entre filas)
        const int ty = y + (gFila - 8) / 2;
        if (ty < y0 || ty + 8 > y1) continue;   // (la letra entera o nada)
        const uint16_t c = elegida ? ELEGIDA : TEXTO;
        if (cuales == FILAS_OPCIONES || cuales == FILAS_CONTROLES) {
            if (cuales == FILAS_OPCIONES) opcion_texto(gOpcs[i], nombre, valor, sizeof(valor));
            else control_texto(i, nombre, valor, sizeof(valor));
            const int lv = (int)strlen(valor);
            texto(img, 8, ty, nombre, c, lv ? (gW - 24) / 8 - lv : 38);
            texto(img, gW - 8 - lv * 8, ty, valor, elegida ? TITULO : c);
        } else if (cuales == FILAS_TV) {
            texto(img, 8, ty, tv_texto(i), c, (gW - 16) / 8);
        } else if (cuales == FILAS_PARTIDAS) {
            partida_texto(i, nombre, sizeof(nombre));
            texto(img, 6, ty, nombre, c, (ancho - 8) / 8);
        } else {
            texto(img, 8, ty, gRoms[i].nombre, c, (gW - 16) / 8);
        }
    }
}

// la captura de la partida elegida, a la derecha de las filas (o un recuadro con lo que es)
void dibujar_mini(uint16_t* img) {
    cargar_mini();
    const int x0 = partidas_ancho() + 4, y0 = lista_y0() + 2, alto = lista_alto() - 4;
    const int w = gMiniW ? gMiniW : MENU_MINI_ANCHO, h = gMiniH ? gMiniH : (alto < 112 ? alto : 112);
    const int x = x0 + (MENU_MINI_ANCHO - w) / 2, y = y0 + (alto - h) / 2;
    rectangulo(img, x - 1, y - 1, w + 2, h + 2, PIE);
    if (!gMiniW) {
        rectangulo(img, x, y, w, h, 0x0000);
        const int r = gPartFilas[gPartL.elegida];
        const char* s = r < 0 ? tr("NUEVA", "NEW") : tr("LIBRE", "EMPTY");
        texto(img, x + (w - (int)strlen(s) * 8) / 2, y + (h - 8) / 2, s, PIE);
        return;
    }
    for (int j = 0; j < h && y + j < gH; j++) memcpy(img + (y + j) * gW + x, gMini + j * gMiniW, w * 2);
}

// el boton de la cabecera (con el dedo) de cada pantalla
const char* boton_cabecera() {
    if (gEnTv) return tr("CLONAR", "MIRROR");
    if (gEnPartidas || gEnControles) return tr("VOLVER", "BACK");
    if (gEnOpciones) return gHayJuego ? tr("JUGAR", "PLAY") : tr("LISTO", "DONE");
    return gHayJuego ? "MENU" : tr("OPCIONES", "OPTIONS");
}

// el ancho del boton de la cabecera (para saber si se toco)
int boton_cabecera_x() { return gW - ((int)strlen(boton_cabecera()) * 8 + 16) - 2; }

// la cabecera: el titulo (o el aviso, con el dedo) y el boton de la derecha (con el dedo) o un texto
void dibujar_cabecera(uint16_t* img, const char* titulo, const char* derecha) {
    texto(img, 8, (gCab - 8) / 2, gAvisoHasta && gTactil ? gAviso : titulo, TITULO);
    if (gTactil) {
        const int w = (int)strlen(boton_cabecera()) * 8 + 16;
        boton(img, gW - w - 2, 2, w, gCab - 4, boton_cabecera());
    } else if (derecha) {
        texto(img, gW - 8 - (int)strlen(derecha) * 8, (gCab - 8) / 2, derecha, PIE);
    }
    rectangulo(img, 0, gCab, gW, 1, PIE);
}

// el pie con un texto (o el aviso, con teclas)
void dibujar_pie(uint16_t* img, const char* s) {
    rectangulo(img, 0, gH - gPie, gW, 1, PIE);
    if (gAvisoHasta && !gTactil) texto(img, 8, gH - gPie + (gPie - 8) / 2, gAviso, TITULO, 38);
    else texto(img, 8, gH - gPie + (gPie - 8) / 2, s, PIE, 38);
}

// LOS BOTONES DEL PIE de la lista (con el dedo): "volver al juego" [x0, x1) y "salir" [x1 + 4, x2); a la derecha,
// los mandos
const char* texto_salir() { return tr("SALIR", "EXIT"); }
void botones_pie(int* volverX0, int* salirX0, int* salirX1) {
    *salirX1 = gW - 8 - 4 * 12 - 6;
    *salirX0 = *salirX1 - ((int)strlen(texto_salir()) * 8 + 24);
    *volverX0 = 2;
}

// el boton de la cabecera, tocado (con el dedo)
MenuAccion tocar_cabecera() {
    gCambio = true;
    if (gEnTv) return elegir_tv(0);
    if (gEnControles) {
        controles_atras();
        return MENU_NADA;
    }
    if (gEnPartidas) {
        gEnPartidas = false;
        return MENU_NADA;
    }
    if (gEnOpciones) return opciones_volver();
    gEnOpciones = true;
    gOpcL.elegida = 0;
    gOpcL.desp = 0;
    return MENU_NADA;
}
}   // namespace

void menu_idioma_sistema(bool espanol) {
    gEspanol = espanol;
    gCambio = true;
}

bool menu_en_ingles() {
    const int i = gOpciones ? gOpciones->idioma : IDIOMA_AUTO;
    return i == IDIOMA_EN || (i == IDIOMA_AUTO && !gEspanol);
}

void menu_texto(uint16_t* img, int ancho, int alto, int x, int y, const char* s, uint16_t color) {
    for (int n = 0; s[n]; n++, x += 8) {
        const unsigned char k = (unsigned char)s[n];
        if (k >= 128 || x < 0 || y < 0 || x + 8 > ancho || y + 8 > alto) continue;
        for (int l = 0; l < 8; l++) {
            const unsigned char fila = fontdata8x8[k * 8 + l];
            uint16_t* p = img + (y + l) * ancho + x;
            for (int b = 0; b < 8; b++)
                if (fila & (0x80 >> b)) p[b] = color;
        }
    }
}

void menu_opciones(MenuOpciones* o) { gOpciones = o; }

void menu_partidas(const MenuFuentePartidas* fuente) { gFuente = fuente; }

void menu_preguntar_tv(int ancho, int alto) {
    snprintf(gTvSolo, sizeof(gTvSolo), menu_tr("Solo la tele (%dx%d)", "TV only (%dx%d)"), ancho, alto);
    gEnTv = true;
    gTvL.elegida = 1;
    gTvL.desp = 0;
    gAntes = ~0;   // (lo que venia apretado no cuenta hasta soltarlo)
    gCambio = true;
}

int menu_tv_salida() { return gTvSalida; }

int menu_ranura() { return gRanura; }

void menu_aviso(const char* texto, uint64_t hastaUs) {
    snprintf(gAviso, sizeof(gAviso), "%s", texto ? texto : "");
    gAvisoHasta = hastaUs;
    gCambio = true;
}

void menu_configurar(int ancho, int alto, int fila, bool tactil, bool pantalla) {
    gW = ancho > MENU_ANCHO ? MENU_ANCHO : ancho;
    gH = alto > MENU_ALTO ? MENU_ALTO : alto;
    gFila = fila < 9 ? 9 : fila;
    gTactil = tactil;
    gPantalla = pantalla;
    gCab = tactil ? gFila : 15;
    gPie = tactil ? gFila : 14;
    armar_opciones();
    gCambio = true;
}

void menu_tamano(int* ancho, int* alto) {
    *ancho = gW;
    *alto = gH;
}

void menu_abrir(const char* carpeta, const char* ultima, bool hayJuego) {
    gHayJuego = hayJuego;
    gCambio = true;
    gEnPartidas = false;
    gEnControles = false;
    gEsperando = false;
    gAvisoHasta = 0;
    armar_opciones();
    gAntes = ~0;   // (lo que ya venia apretado -el boton que abrio el menu- no cuenta hasta soltarlo)
    // con un juego cargado, la pausa; si no, la lista de las ROMs
    gEnOpciones = hayJuego;
    gOpcL.elegida = 0;
    gOpcL.desp = 0;
    if (hayJuego && ultima) snprintf(gRomJuego, sizeof(gRomJuego), "%s", ultima);
    if (!gRoms) gRoms = (Rom*)malloc(sizeof(Rom) * MAX_ROMS);
    if (!gRoms) return;
    strncpy(gCarpeta, carpeta, sizeof(gCarpeta) - 1);
    gCantidad = 0;
    DIR* d = opendir(carpeta);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != 0 && gCantidad < MAX_ROMS - 1) {
            if (!es_rom(e->d_name)) continue;
            Rom& r = gRoms[gCantidad++];
            r.tipo = FILA_ROM;
            snprintf(r.ruta, sizeof(r.ruta), "%s%s", carpeta, e->d_name);
            // el nombre sin la extension
            strncpy(r.nombre, e->d_name, sizeof(r.nombre) - 1);
            r.nombre[sizeof(r.nombre) - 1] = 0;
            char* punto = strrchr(r.nombre, '.');
            if (punto && punto != r.nombre) *punto = 0;
        }
        closedir(d);
    }
    qsort(gRoms, gCantidad, sizeof(Rom), comparar);
    // con teclas, la ultima fila: salir del emulador (ruta vacia; el nombre se pone al dibujar, por el idioma)
    gFilaSalir = !gTactil;
    if (gFilaSalir) {
        gRoms[gCantidad].ruta[0] = 0;
        gRoms[gCantidad].nombre[0] = 0;
        gRoms[gCantidad].tipo = FILA_SALIR;
        gCantidad++;
    }
    gRomsL.elegida = 0;
    for (int i = 0; ultima && ultima[0] && i < gCantidad; i++)
        if (gRoms[i].tipo == FILA_ROM && !strcmp(gRoms[i].ruta, ultima)) gRomsL.elegida = i;
    // (la elegida a la mitad de la lista)
    gRomsL.desp = gRomsL.elegida * gFila - lista_alto() / 2;
    recortar(gRomsL, gCantidad);
}

int menu_cantidad() { return gCantidad - (gFilaSalir ? 1 : 0); }   // (solo las ROMs)

void menu_redibujar() { gCambio = true; }

const char* menu_elegida() { return gJugar; }

void menu_mandos(int conectados) {
    if (conectados != gMandos) gCambio = true;
    gMandos = conectados;
}

MenuAccion menu_actualizar(int botones, uint64_t ahora) {
    const int nuevos = botones & ~gAntes;
    gAntes = botones;
    if (gAvisoHasta && ahora > gAvisoHasta) {   // (se termino el aviso)
        gAvisoHasta = 0;
        gCambio = true;
    }
    if (gEnTv) return actualizar_tv(botones, nuevos, ahora);
    if (gEnControles) return actualizar_controles(botones, nuevos, ahora);
    if (gEnPartidas) return actualizar_partidas(botones, nuevos, ahora);
    if (gEnOpciones) return actualizar_opciones(botones, nuevos, ahora);
    // LA LISTA DE LAS ROMS: Start, las opciones (con un juego, la pausa); volver: a la pausa
    if (nuevos & MENU_OPCIONES) {
        gEnOpciones = true;
        gOpcL.elegida = 0;
        gOpcL.desp = 0;
        gCambio = true;
        return MENU_NADA;
    }
    if ((nuevos & MENU_VOLVER) && gHayJuego) {
        gEnOpciones = true;
        gCambio = true;
        return MENU_NADA;
    }
    if ((nuevos & MENU_ELEGIR) && gCantidad) return elegir_rom(gRomsL.elegida);
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO | MENU_IZQ | MENU_DER);
    if (!mover(nuevos, mueve, ahora)) return MENU_NADA;
    const int pagina = lista_alto() / gFila > 1 ? lista_alto() / gFila : 1;
    int paso = 0;
    if (mueve & MENU_ARRIBA) paso = -1;
    if (mueve & MENU_ABAJO) paso = 1;
    if (mueve & MENU_IZQ) paso = -pagina;
    if (mueve & MENU_DER) paso = pagina;
    if (!gCantidad || !paso) return MENU_NADA;
    // (arriba/abajo dan la vuelta; de a pagina se frena en los bordes)
    int e = gRomsL.elegida + paso;
    if (paso == 1 || paso == -1) e = (e + gCantidad) % gCantidad;
    if (e < 0) e = 0;
    if (e >= gCantidad) e = gCantidad - 1;
    if (e != gRomsL.elegida) {
        gRomsL.elegida = e;
        seguir(gRomsL, gCantidad);
        gCambio = true;
    }
    return MENU_NADA;
}

MenuAccion menu_toque(int tipo, int x, int y) {
    Lista& l = gEnTv ? gTvL : gEnControles ? gCtrlL : gEnPartidas ? gPartL : gEnOpciones ? gOpcL : gRomsL;
    const int n = gEnTv ? 3 : gEnControles ? controles_filas() : gEnPartidas ? gPartN : gEnOpciones ? gOpcsN : gCantidad;
    if (tipo == TOQUE_APOYA) {
        gToqueX0 = x;
        gToqueY0 = y;
        gDespIni = l.desp;
        gArrastra = false;
        gZona = y < gCab ? ZONA_CAB : y >= gH - gPie ? ZONA_PIE : ZONA_LISTA;
        return MENU_NADA;
    }
    if (tipo == TOQUE_MUEVE) {
        const int dy = y - gToqueY0;
        if (gZona == ZONA_LISTA && (dy > 6 || dy < -6)) gArrastra = true;
        if (gArrastra) {
            l.desp = gDespIni - dy;
            recortar(l, n);
            gCambio = true;
        }
        return MENU_NADA;
    }
    // TOQUE_SUELTA: si no se arrastro, es tocar
    if (gArrastra) return MENU_NADA;
    gCambio = true;
    if (gZona == ZONA_CAB) return gToqueX0 >= boton_cabecera_x() ? tocar_cabecera() : MENU_NADA;
    if (gZona == ZONA_PIE) {
        if (gEnOpciones || gEnPartidas || gEnControles || gEnTv) return MENU_NADA;
        int volverX0, salirX0, salirX1;
        botones_pie(&volverX0, &salirX0, &salirX1);
        if (gToqueX0 >= salirX0 && gToqueX0 < salirX1) return MENU_SALIR;
        return gHayJuego && gToqueX0 < salirX0 ? MENU_SEGUIR : MENU_NADA;
    }
    const int i = (y - lista_y0() + l.desp) / gFila;
    if (y < lista_y0() || i < 0 || i >= n) return MENU_NADA;
    if (gEnTv) {
        l.elegida = i;
        return elegir_tv(i);
    }
    // las partidas: el primer toque la elige (se ve la captura), el segundo en la misma la confirma
    if (gEnPartidas) {
        if (gToqueX0 >= partidas_ancho()) return MENU_NADA;   // (la captura)
        if (i != l.elegida) {
            l.elegida = i;
            return MENU_NADA;
        }
        return elegir_partida(i);
    }
    if (gEnControles) {
        if (gEsperando) return MENU_NADA;
        l.elegida = i;
        return elegir_control(i);
    }
    l.elegida = i;
    if (gEnOpciones) return cambiar_opcion(gOpcs[i], 1);
    return elegir_rom(i);
}

bool menu_dibujar(uint16_t* img) {
    // (esperando una tecla: el menu no cambia solo, pero hay que dibujarlo una vez)
    if (!gCambio) return false;
    gCambio = false;
    rectangulo(img, 0, 0, gW, gH, FONDO);
    const char* version = "MEGA DRIVE W3D " SEGAW3D_VERSION;
    if (gEnTv) {
        dibujar_cabecera(img, tr("PANTALLA EXTERNA", "EXTERNAL DISPLAY"), 0);
        dibujar_filas(img, gTvL, 3, FILAS_TV);
        dibujar_pie(img, gTactil ? tr("Toca una opcion", "Tap an option")
                                 : tr("OK: elegir   Der: clonar", "OK: choose   Right: mirror"));
        return true;
    }
    if (gEnControles) {
        char titulo[32];
        if (gCtrlJugador < 0) snprintf(titulo, sizeof(titulo), "%s", tr("CONTROLES", "CONTROLS"));
        else snprintf(titulo, sizeof(titulo), "%s %d", tr("CONTROLES: JUGADOR", "CONTROLS: PLAYER"), gCtrlJugador + 1);
        dibujar_cabecera(img, titulo, 0);
        dibujar_filas(img, gCtrlL, controles_filas(), FILAS_CONTROLES);
        const char* pie;
        if (gEsperando)
            pie = gTactil ? tr("Apreta un boton del mando", "Press a pad button")
                          : tr("Apreta la tecla (Der: cancelar)", "Press the key (Right: cancel)");
        else if (gCtrlJugador < 0)
            pie = gTactil ? tr("Toca un jugador", "Tap a player") : tr("OK: elegir   Der: volver", "OK: choose   Right: back");
        else
            pie = gTactil ? tr("Toca un boton para cambiarlo", "Tap a button to change it")
                          : tr("OK: cambiar   Der: volver", "OK: change   Right: back");
        dibujar_pie(img, pie);
        return true;
    }
    if (gEnPartidas) {
        dibujar_cabecera(img, gPartGuardar ? tr("GUARDAR PARTIDA", "SAVE GAME") : tr("PARTIDAS", "SAVED GAMES"), 0);
        dibujar_filas(img, gPartL, gPartN, FILAS_PARTIDAS);
        dibujar_mini(img);
        dibujar_pie(img, gTactil ? (gPartGuardar ? tr("Toca otra vez para guardar", "Tap again to save")
                                                 : tr("Toca otra vez para jugar", "Tap again to play"))
                                 : (gPartGuardar ? tr("OK: guardar   Der: volver", "OK: save   Right: back")
                                                 : tr("OK: jugar   Der: volver", "OK: play   Right: back")));
        return true;
    }
    if (gEnOpciones) {
        dibujar_cabecera(img, gHayJuego ? version : tr("OPCIONES", "OPTIONS"), gHayJuego ? tr("PAUSA", "PAUSED") : 0);
        dibujar_filas(img, gOpcL, gOpcsN, FILAS_OPCIONES);
        dibujar_pie(img, gTactil ? tr("Toca una opcion", "Tap an option")
                                 : gHayJuego ? tr("OK: elegir   Der: volver al juego", "OK: choose   Right: back to game")
                                             : tr("OK: cambiar   Start: volver", "OK: change   Start: back"));
        return true;
    }
    if (gFilaSalir) strcpy(gRoms[gCantidad - 1].nombre, tr("[ Salir ]", "[ Exit ]"));
    char linea[64];
    snprintf(linea, sizeof(linea), "%d", menu_cantidad());
    dibujar_cabecera(img, version, linea);
    if (menu_cantidad() == 0) {
        texto(img, 8, lista_y0() + 24, tr("No hay ROMs en", "No ROMs in"), TEXTO);
        texto(img, 8, lista_y0() + 36, gCarpeta, TEXTO, 39);
    }
    dibujar_filas(img, gRomsL, gCantidad, FILAS_ROMS);
    // el pie: como se elige (con el dedo, los botones volver al juego y salir) y los mandos conectados; o el aviso
    rectangulo(img, 0, gH - gPie, gW, 1, PIE);
    if (gAvisoHasta && !gTactil) {
        texto(img, 8, gH - gPie + (gPie - 8) / 2, gAviso, TITULO, (gW - 8 - 4 * 12 - 16) / 8);
    } else if (gTactil) {
        int volverX0, salirX0, salirX1;
        botones_pie(&volverX0, &salirX0, &salirX1);
        if (gHayJuego)
            boton(img, volverX0, gH - gPie + 2, salirX0 - 4 - volverX0, gPie - 4, tr("VOLVER AL JUEGO", "BACK TO GAME"));
        else
            texto(img, 8, gH - gPie + (gPie - 8) / 2, tr("Toca un juego", "Tap a game"), PIE);
        boton(img, salirX0, gH - gPie + 2, salirX1 - salirX0, gPie - 4, texto_salir());
    } else {
        texto(img, 8, gH - gPie + (gPie - 8) / 2,
              gHayJuego ? tr("OK:jugar Start:menu Der:volver", "OK:play Start:menu Right:back")
                        : tr("OK:jugar  Start:opciones", "OK:play  Start:options"),
              PIE, 32);
    }
    for (int m = 0; m < 4; m++) {
        static const uint16_t colores[4] = { 0xFFFF, 0x4208, 0xF800, 0x221F };
        const int x = gW - 8 - (4 - m) * 12, y = gH - gPie + (gPie - 10) / 2;
        if (gMandos & (1 << m)) rectangulo(img, x, y, 10, 10, colores[m]);
        else rectangulo(img, x + 4, y + 4, 2, 2, PIE);
    }
    return true;
}
