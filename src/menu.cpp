#include "menu.h"
#include "pantalla.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" unsigned char fontdata8x8[64 * 16];   // (picodrive/platform/libpicofe/fonts.c: 128 letras de 8x8)

namespace {
const int MAX_ROMS = 512;

struct Rom {
    char ruta[256];
    char nombre[64];
};
Rom* gRoms = 0;
int gCantidad = 0;        // (con la fila de "Salir", si esta)
bool gFilaSalir = false;  // la ultima fila es "Salir" (con teclas; con el dedo es un boton del pie)
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

// UNA LISTA (la de las ROMs o la de las opciones): cual esta elegida y cuanto se corrio (en pixeles)
struct Lista {
    int elegida;
    int desp;
};
Lista gRomsL = { 0, 0 }, gOpcL = { 0, 0 };

// las opciones (la pantalla que abre Start)
MenuOpciones* gOpciones = 0;
bool gEnOpciones = false;
enum { OPC_FPS, OPC_ESCALA, OPC_SUAVE, OPC_TACTIL, OPC_DIBUJO, OPC_MULTITAP, OPC_HZ, OPC_FM, OPC_FILTRO, OPC_IDIOMA,
       OPC_VOLVER, OPC_CERRAR };
int gOpcs[16];   // las que se muestran (OPC_*), en orden
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

// las opciones que se muestran: las de pantalla solo donde se pueden elegir
void armar_opciones() {
    gOpcsN = 0;
    gOpcs[gOpcsN++] = OPC_FPS;
    if (gPantalla) {
        gOpcs[gOpcsN++] = OPC_ESCALA;
        gOpcs[gOpcsN++] = OPC_SUAVE;
    }
    if (gTactil) gOpcs[gOpcsN++] = OPC_TACTIL;
    gOpcs[gOpcsN++] = OPC_DIBUJO;
    gOpcs[gOpcsN++] = OPC_MULTITAP;
    gOpcs[gOpcsN++] = OPC_HZ;
    gOpcs[gOpcsN++] = OPC_FM;
    gOpcs[gOpcsN++] = OPC_FILTRO;
    gOpcs[gOpcsN++] = OPC_IDIOMA;
    gOpcs[gOpcsN++] = OPC_VOLVER;
    gOpcs[gOpcsN++] = OPC_CERRAR;
}

// cambia la opcion 'o' un paso (+1 / -1); OPC_VOLVER cierra las opciones y OPC_CERRAR el emulador
MenuAccion cambiar_opcion(int o, int paso) {
    gCambio = true;
    if (o == OPC_CERRAR) return MENU_SALIR;
    if (o == OPC_VOLVER) {
        gEnOpciones = false;
        return MENU_NADA;
    }
    if (!gOpciones) return MENU_NADA;
    MenuOpciones& p = *gOpciones;
    switch (o) {
    case OPC_FPS: p.fps = !p.fps; break;
    case OPC_ESCALA: p.escala = (p.escala + ESCALAS + paso) % ESCALAS; break;
    case OPC_SUAVE: p.suave = !p.suave; break;
    case OPC_TACTIL: p.tactil = (p.tactil + 3 + paso) % 3; break;
    case OPC_DIBUJO: p.rapido = !p.rapido; break;
    case OPC_MULTITAP: p.multitap = (p.multitap + 3 + paso) % 3; break;
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
    valor[0] = 0;
    const char* k = "";
    const char* v = "";
    switch (o) {
    case OPC_FPS: k = tr("Contador de FPS", "FPS counter"); v = si_no(p && p->fps); break;
    case OPC_ESCALA: k = tr("Escala", "Scale"); v = escala_nombre(p ? p->escala : 0); break;
    case OPC_SUAVE: k = tr("Imagen", "Image"); v = p && p->suave ? tr("SUAVE", "SMOOTH") : tr("PIXELADA", "PIXELATED"); break;
    case OPC_TACTIL: k = tr("Controles en pantalla", "On-screen controls"); v = tactil_nombre(p ? p->tactil : 0); break;
    case OPC_DIBUJO:
        k = tr("Dibujo", "Rendering");
        v = p && p->rapido ? tr("RAPIDO", "FAST") : tr("PRECISO", "ACCURATE");
        break;
    case OPC_MULTITAP: k = tr("Multitap (4 jug.)", "Multitap (4 pl.)"); v = multitap_nombre(p ? p->multitap : 0); break;
    case OPC_HZ: k = tr("Sonido", "Sound"); v = kHz[p ? p->hz : 0]; break;
    case OPC_FM: k = tr("FM alta calidad", "High quality FM"); v = si_no(p && p->fmAlta); break;
    case OPC_FILTRO: k = tr("Filtro de sonido", "Sound filter"); v = si_no(p && p->filtro); break;
    case OPC_IDIOMA: k = "Idioma / Language"; v = idioma_nombre(p ? p->idioma : 0); break;
    case OPC_CERRAR: k = tr("Cerrar emulador", "Exit emulator"); break;
    default: k = tr("Volver", "Back"); break;
    }
    snprintf(nombre, n, "%s", k);
    snprintf(valor, n, "%s", v);
}

MenuAccion actualizar_opciones(int botones, int nuevos, uint64_t ahora) {
    if (nuevos & (MENU_OPCIONES | MENU_VOLVER)) {
        gEnOpciones = false;
        gCambio = true;
        return MENU_NADA;
    }
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO | MENU_IZQ | MENU_DER);
    const bool elegir = (nuevos & MENU_ELEGIR) != 0;
    if (!elegir && !mover(nuevos, mueve, ahora)) return MENU_NADA;
    gCambio = true;
    if (!elegir && (mueve & (MENU_ARRIBA | MENU_ABAJO))) {
        gOpcL.elegida = (gOpcL.elegida + ((mueve & MENU_ABAJO) ? 1 : gOpcsN - 1)) % gOpcsN;
        seguir(gOpcL, gOpcsN);
        return MENU_NADA;
    }
    // OK, izquierda o derecha cambian la opcion elegida (cerrar el emulador, solo con OK)
    if (!elegir && gOpcs[gOpcL.elegida] == OPC_CERRAR) return MENU_NADA;
    return cambiar_opcion(gOpcs[gOpcL.elegida], (mueve & MENU_IZQ) ? -1 : 1);
}

// LAS FILAS de una lista (con el desplazamiento, recortadas a la zona de la lista)
void dibujar_filas(uint16_t* img, const Lista& l, int n, bool opciones) {
    const int y0 = lista_y0(), y1 = y0 + lista_alto();
    char nombre[64], valor[32];
    for (int i = 0; i < n; i++) {
        const int y = y0 + i * gFila - l.desp;
        if (y + gFila <= y0 || y >= y1) continue;
        const bool elegida = i == l.elegida;
        if (elegida) rectangulo(img, 0, y, gW, gFila - 1, BARRA, y0, y1);
        else if (gTactil) rectangulo(img, 0, y + gFila - 1, gW, 1, 0x18E3, y0, y1);   // (una raya entre filas)
        const int ty = y + (gFila - 8) / 2;
        if (ty < y0 || ty + 8 > y1) continue;   // (la letra entera o nada)
        const uint16_t c = elegida ? ELEGIDA : TEXTO;
        if (opciones) {
            opcion_texto(gOpcs[i], nombre, valor, sizeof(valor));
            texto(img, 8, ty, nombre, c, 26);
            texto(img, gW - 8 - (int)strlen(valor) * 8, ty, valor, elegida ? TITULO : c);
        } else {
            texto(img, 8, ty, gRoms[i].nombre, c, (gW - 16) / 8);
        }
    }
}

// la cabecera: el titulo y (con el dedo) el boton de la derecha
void dibujar_cabecera(uint16_t* img, const char* titulo, const char* derecha, const char* boton_der) {
    texto(img, 8, (gCab - 8) / 2, titulo, TITULO);
    if (gTactil && boton_der) {
        const int w = (int)strlen(boton_der) * 8 + 16;
        boton(img, gW - w - 2, 2, w, gCab - 4, boton_der);
    } else if (derecha) {
        texto(img, gW - 8 - (int)strlen(derecha) * 8, (gCab - 8) / 2, derecha, PIE);
    }
    rectangulo(img, 0, gCab, gW, 1, PIE);
}

const char* boton_cabecera() { return gEnOpciones ? tr("LISTO", "DONE") : tr("OPCIONES", "OPTIONS"); }

// el ancho del boton de la cabecera (para saber si se toco)
int boton_cabecera_x() { return gW - ((int)strlen(boton_cabecera()) * 8 + 16) - 2; }

// LOS BOTONES DEL PIE (con el dedo): "volver al juego" [x0, x1) y "salir" [x1 + 4, x2); a la derecha, los mandos
const char* texto_salir() { return tr("SALIR", "EXIT"); }
void botones_pie(int* volverX0, int* salirX0, int* salirX1) {
    *salirX1 = gW - 8 - 4 * 12 - 6;
    *salirX0 = *salirX1 - ((int)strlen(texto_salir()) * 8 + 24);
    *volverX0 = 2;
}
}

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
    gEnOpciones = false;
    if (!gOpcsN) armar_opciones();
    gAntes = ~0;   // (lo que ya venia apretado -el boton que abrio el menu- no cuenta hasta soltarlo)
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
        gCantidad++;
    }
    gRomsL.elegida = 0;
    for (int i = 0; ultima && ultima[0] && i < gCantidad; i++)
        if (!strcmp(gRoms[i].ruta, ultima)) gRomsL.elegida = i;
    // (la elegida a la mitad de la lista)
    gRomsL.desp = gRomsL.elegida * gFila - lista_alto() / 2;
    recortar(gRomsL, gCantidad);
}

int menu_cantidad() { return gCantidad - (gFilaSalir ? 1 : 0); }   // (sin la fila de salir)

void menu_redibujar() { gCambio = true; }

const char* menu_elegida() { return gCantidad ? gRoms[gRomsL.elegida].ruta : ""; }

void menu_mandos(int conectados) {
    if (conectados != gMandos) gCambio = true;
    gMandos = conectados;
}

MenuAccion menu_actualizar(int botones, uint64_t ahora) {
    const int nuevos = botones & ~gAntes;
    gAntes = botones;
    if (gEnOpciones) return actualizar_opciones(botones, nuevos, ahora);
    if (nuevos & MENU_OPCIONES) {
        gEnOpciones = true;
        gOpcL.elegida = 0;
        gOpcL.desp = 0;
        gCambio = true;
        return MENU_NADA;
    }
    if ((nuevos & MENU_VOLVER) && gHayJuego) return MENU_SEGUIR;
    if ((nuevos & MENU_ELEGIR) && gCantidad) return gRoms[gRomsL.elegida].ruta[0] ? MENU_JUGAR : MENU_SALIR;
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
    Lista& l = gEnOpciones ? gOpcL : gRomsL;
    const int n = gEnOpciones ? gOpcsN : gCantidad;
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
    if (gZona == ZONA_CAB) {
        if (gToqueX0 >= boton_cabecera_x()) {
            if (gEnOpciones) {
                gEnOpciones = false;
            } else {
                gEnOpciones = true;
                gOpcL.elegida = 0;
                gOpcL.desp = 0;
            }
        }
        return MENU_NADA;
    }
    if (gZona == ZONA_PIE) {
        if (gEnOpciones) return MENU_NADA;
        int volverX0, salirX0, salirX1;
        botones_pie(&volverX0, &salirX0, &salirX1);
        if (gToqueX0 >= salirX0 && gToqueX0 < salirX1) return MENU_SALIR;
        return gHayJuego && gToqueX0 < salirX0 ? MENU_SEGUIR : MENU_NADA;
    }
    const int i = (y - lista_y0() + l.desp) / gFila;
    if (y < lista_y0() || i < 0 || i >= n) return MENU_NADA;
    l.elegida = i;
    if (gEnOpciones) return cambiar_opcion(gOpcs[i], 1);
    return gRoms[i].ruta[0] ? MENU_JUGAR : MENU_SALIR;
}

bool menu_dibujar(uint16_t* img) {
    if (!gCambio) return false;
    gCambio = false;
    rectangulo(img, 0, 0, gW, gH, FONDO);
    if (gEnOpciones) {
        dibujar_cabecera(img, tr("OPCIONES", "OPTIONS"), 0, boton_cabecera());
        dibujar_filas(img, gOpcL, gOpcsN, true);
        rectangulo(img, 0, gH - gPie, gW, 1, PIE);
        texto(img, 8, gH - gPie + (gPie - 8) / 2,
              gTactil ? tr("Tocar una opcion la cambia", "Tap an option to change it")
                      : tr("OK: cambiar   Start: volver", "OK: change   Start: back"),
              PIE);
        return true;
    }
    if (gFilaSalir) strcpy(gRoms[gCantidad - 1].nombre, tr("[ Salir ]", "[ Exit ]"));
    char linea[64];
    snprintf(linea, sizeof(linea), "%d", menu_cantidad());
    dibujar_cabecera(img, "MEGA DRIVE W3D", linea, boton_cabecera());
    if (menu_cantidad() == 0) {
        texto(img, 8, lista_y0() + 24, tr("No hay ROMs en", "No ROMs in"), TEXTO);
        texto(img, 8, lista_y0() + 36, gCarpeta, TEXTO, 39);
    }
    dibujar_filas(img, gRomsL, gCantidad, false);
    // el pie: como se elige (con el dedo, los botones volver al juego y salir) y los mandos conectados
    rectangulo(img, 0, gH - gPie, gW, 1, PIE);
    if (gTactil) {
        int volverX0, salirX0, salirX1;
        botones_pie(&volverX0, &salirX0, &salirX1);
        if (gHayJuego)
            boton(img, volverX0, gH - gPie + 2, salirX0 - 4 - volverX0, gPie - 4, tr("VOLVER AL JUEGO", "BACK TO GAME"));
        else
            texto(img, 8, gH - gPie + (gPie - 8) / 2, tr("Toca un juego", "Tap a game"), PIE);
        boton(img, salirX0, gH - gPie + 2, salirX1 - salirX0, gPie - 4, texto_salir());
    } else {
        texto(img, 8, gH - gPie + (gPie - 8) / 2,
              gHayJuego ? tr("OK:jugar Start:opc. Der:volver", "OK:play Start:opt. Right:back")
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
