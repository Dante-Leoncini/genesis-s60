#include "menu.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" unsigned char fontdata8x8[64 * 16];   // (picodrive/platform/libpicofe/fonts.c: 128 letras de 8x8)

namespace {
const int MAX_ROMS = 512;
const int FILAS = 22;   // los nombres que entran en la pantalla (de a 8 pixeles, entre el titulo y el pie)

struct Rom {
    char ruta[256];
    char nombre[64];
};
Rom* gRoms = 0;
int gCantidad = 0;   // (con la fila de "Salir")
int gElegida = 0, gPrimera = 0;
bool gHayJuego = false;
bool gCambio = true;
char gCarpeta[256] = "";
int gMandos = 0;
// la repeticion de arriba/abajo mantenidos
int gAntes = 0;
uint64_t gRepetirEn = 0;

// las opciones (la pantalla que abre Start)
MenuOpciones* gOpciones = 0;
bool gEnOpciones = false;
int gOpcion = 0;
enum { OPC_FPS, OPC_DIBUJO, OPC_MULTITAP, OPC_HZ, OPC_FM, OPC_FILTRO, OPC_VOLVER, OPCIONES };

// los colores (RGB565)
const uint16_t FONDO = 0x0841, TITULO = 0xFFE0, TEXTO = 0xC618, ELEGIDA = 0xFFFF, BARRA = 0x2A7F, PIE = 0x8410;

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
    menu_texto(img, MENU_ANCHO, MENU_ALTO, x, y, corto, color);
}

void rectangulo(uint16_t* img, int x, int y, int w, int h, uint16_t color) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) img[j * MENU_ANCHO + i] = color;
}

const char* multitap_nombre(int m) {
    return m == MULTITAP_SIEMPRE ? "SIEMPRE" : m == MULTITAP_NUNCA ? "NUNCA" : "AUTO";
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

MenuAccion actualizar_opciones(int botones, int nuevos, uint64_t ahora) {
    if (nuevos & (MENU_OPCIONES | MENU_VOLVER)) {
        gEnOpciones = false;
        gCambio = true;
        return MENU_NADA;
    }
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO | MENU_IZQ | MENU_DER);
    const bool cambiar = (nuevos & MENU_ELEGIR) != 0;
    if (!cambiar && !mover(nuevos, mueve, ahora)) return MENU_NADA;
    gCambio = true;
    if (!cambiar && (mueve & (MENU_ARRIBA | MENU_ABAJO))) {
        gOpcion = (gOpcion + ((mueve & MENU_ABAJO) ? 1 : OPCIONES - 1)) % OPCIONES;
        return MENU_NADA;
    }
    // OK, izquierda o derecha cambian la opcion elegida
    const int paso = (mueve & MENU_IZQ) ? -1 : 1;
    if (!gOpciones) return MENU_NADA;
    switch (gOpcion) {
    case OPC_FPS:
        gOpciones->fps = !gOpciones->fps;
        return MENU_CAMBIO_OPCIONES;
    case OPC_MULTITAP:
        gOpciones->multitap = (gOpciones->multitap + 3 + paso) % 3;
        return MENU_CAMBIO_OPCIONES;
    case OPC_DIBUJO:
        gOpciones->rapido = !gOpciones->rapido;
        return MENU_CAMBIO_OPCIONES;
    case OPC_HZ:
        gOpciones->hz = (gOpciones->hz + 3 + paso) % 3;
        return MENU_CAMBIO_OPCIONES;
    case OPC_FILTRO:
        gOpciones->filtro = !gOpciones->filtro;
        return MENU_CAMBIO_OPCIONES;
    case OPC_FM:
        gOpciones->fmAlta = !gOpciones->fmAlta;
        return MENU_CAMBIO_OPCIONES;
    default:
        if (cambiar) gEnOpciones = false;
        return MENU_NADA;
    }
}

void dibujar_opciones(uint16_t* img) {
    texto(img, 8, 4, "OPCIONES", TITULO);
    char linea[64];
    for (int i = 0; i < OPCIONES; i++) {
        const int y = 32 + i * 16;
        if (i == gOpcion) rectangulo(img, 0, y - 3, MENU_ANCHO, 14, BARRA);
        const uint16_t c = i == gOpcion ? ELEGIDA : TEXTO;
        if (i == OPC_FPS) snprintf(linea, sizeof(linea), "Contador de FPS:  %s", gOpciones && gOpciones->fps ? "SI" : "NO");
        else if (i == OPC_MULTITAP)
            snprintf(linea, sizeof(linea), "Multitap (4 jug.): %s", multitap_nombre(gOpciones ? gOpciones->multitap : 0));
        else if (i == OPC_DIBUJO)
            snprintf(linea, sizeof(linea), "Dibujo:            %s", gOpciones && gOpciones->rapido ? "RAPIDO" : "PRECISO");
        else if (i == OPC_HZ) {
            static const char* const kHz[3] = { "22 kHz", "32 kHz", "44 kHz" };
            snprintf(linea, sizeof(linea), "Sonido:            %s", kHz[gOpciones ? gOpciones->hz : 0]);
        } else if (i == OPC_FILTRO)
            snprintf(linea, sizeof(linea), "Filtro de sonido:  %s", gOpciones && gOpciones->filtro ? "SI" : "NO");
        else if (i == OPC_FM)
            snprintf(linea, sizeof(linea), "FM alta calidad:   %s", gOpciones && gOpciones->fmAlta ? "SI" : "NO");
        else snprintf(linea, sizeof(linea), "Volver");
        texto(img, 8, y, linea, c);
    }
    texto(img, 8, 148, "Dibujo RAPIDO: mucho mas liviano,", PIE);
    texto(img, 8, 158, "pero sin efectos de mitad de", PIE);
    texto(img, 8, 168, "pantalla (agua, paleta por linea).", PIE);
    texto(img, 8, 182, "Multitap AUTO: solo en los juegos", PIE);
    texto(img, 8, 192, "que lo traen (Mega Bomberman).", PIE);
    texto(img, 8, 206, "Mas kHz / FM alta: mejor y lento.", PIE);
    rectangulo(img, 0, MENU_ALTO - 14, MENU_ANCHO, 1, PIE);
    texto(img, 8, MENU_ALTO - 10, "OK: cambiar   Start: volver", PIE);
}
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

void menu_abrir(const char* carpeta, const char* ultima, bool hayJuego) {
    gHayJuego = hayJuego;
    gCambio = true;
    gEnOpciones = false;
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
    // la ultima fila: salir del emulador (ruta vacia)
    gRoms[gCantidad].ruta[0] = 0;
    strcpy(gRoms[gCantidad].nombre, "[ Salir ]");
    gCantidad++;
    gElegida = 0;
    for (int i = 0; ultima && ultima[0] && i < gCantidad; i++)
        if (!strcmp(gRoms[i].ruta, ultima)) gElegida = i;
    gPrimera = gElegida - FILAS / 2;
}

int menu_cantidad() { return gCantidad - 1; }   // (sin la fila de salir)

void menu_redibujar() { gCambio = true; }

const char* menu_elegida() { return gCantidad ? gRoms[gElegida].ruta : ""; }

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
        gOpcion = 0;
        gCambio = true;
        return MENU_NADA;
    }
    if ((nuevos & MENU_VOLVER) && gHayJuego) return MENU_SEGUIR;
    if ((nuevos & MENU_ELEGIR) && gCantidad) return gRoms[gElegida].ruta[0] ? MENU_JUGAR : MENU_SALIR;
    const int mueve = botones & (MENU_ARRIBA | MENU_ABAJO | MENU_IZQ | MENU_DER);
    if (!mover(nuevos, mueve, ahora)) return MENU_NADA;
    int paso = 0;
    if (mueve & MENU_ARRIBA) paso = -1;
    if (mueve & MENU_ABAJO) paso = 1;
    if (mueve & MENU_IZQ) paso = -FILAS;
    if (mueve & MENU_DER) paso = FILAS;
    if (!gCantidad || !paso) return MENU_NADA;
    // (arriba/abajo dan la vuelta; de a pagina se frena en los bordes)
    int e = gElegida + paso;
    if (paso == 1 || paso == -1) e = (e + gCantidad) % gCantidad;
    if (e < 0) e = 0;
    if (e >= gCantidad) e = gCantidad - 1;
    if (e != gElegida) {
        gElegida = e;
        gCambio = true;
    }
    return MENU_NADA;
}

bool menu_dibujar(uint16_t* img) {
    if (!gCambio) return false;
    gCambio = false;
    rectangulo(img, 0, 0, MENU_ANCHO, MENU_ALTO, FONDO);
    rectangulo(img, 0, 14, MENU_ANCHO, 1, PIE);
    if (gEnOpciones) {
        dibujar_opciones(img);
        return true;
    }
    texto(img, 8, 4, "MEGA DRIVE W3D", TITULO);
    char linea[64];
    snprintf(linea, sizeof(linea), "%d", gCantidad - 1);
    texto(img, MENU_ANCHO - 8 - (int)strlen(linea) * 8, 4, linea, PIE);
    if (gCantidad <= 1) {
        texto(img, 8, 40, "No hay ROMs en", TEXTO);
        texto(img, 8, 52, gCarpeta, TEXTO, 39);
    }
    // la ventana de nombres sigue a la elegida
    if (gElegida < gPrimera) gPrimera = gElegida;
    if (gElegida >= gPrimera + FILAS) gPrimera = gElegida - FILAS + 1;
    if (gPrimera > gCantidad - FILAS) gPrimera = gCantidad - FILAS;
    if (gPrimera < 0) gPrimera = 0;
    for (int f = 0; f < FILAS && gPrimera + f < gCantidad; f++) {
        const int i = gPrimera + f, y = 20 + f * 9;
        if (i == gElegida) rectangulo(img, 0, y - 1, MENU_ANCHO, 10, BARRA);
        texto(img, 8, y, gRoms[i].nombre, i == gElegida ? ELEGIDA : TEXTO, 38);
    }
    // el pie: como se elige y los mandos conectados
    rectangulo(img, 0, MENU_ALTO - 14, MENU_ANCHO, 1, PIE);
    texto(img, 8, MENU_ALTO - 10, gHayJuego ? "OK:jugar Start:opc. Der:volver" : "OK:jugar  Start:opciones", PIE, 32);
    for (int m = 0; m < 4; m++) {
        static const uint16_t colores[4] = { 0xFFFF, 0x4208, 0xF800, 0x221F };
        const int x = MENU_ANCHO - 8 - (4 - m) * 12, y = MENU_ALTO - 11;
        if (gMandos & (1 << m)) rectangulo(img, x, y, 10, 10, colores[m]);
        else rectangulo(img, x + 4, y + 4, 2, 2, PIE);
    }
    return true;
}
