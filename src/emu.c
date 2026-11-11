/* ---------------------------------------------------------------------------------------------------------------
 *  emu.h sobre el nucleo de PicoDrive: lo que el nucleo le pide a la plataforma (lprintf, la memoria, el cambio de
 *  modo de video, lo del Mega CD que no se usa) y la cola del sonido entre el hilo del juego y el de audio.
 * --------------------------------------------------------------------------------------------------------------- */
#include "emu.h"
/* (pico_int.h tiene macros con "...": el cpp viejo con el que makmake busca las dependencias en Symbian no las
 * entiende y corta la generacion del makefile. Lo ven el armcc y el gcc de la PC, no ese) */
#if defined(__CC_ARM) || !defined(__SYMBIAN32__)
#ifdef __CC_ARM
/* (con --gnu el RVCT 2.2 dice ser gcc pero no tiene __builtin_expect: likely()/unlikely() de PicoDrive) */
#define __builtin_expect(x, esperado) (x)
#endif
#include <pico/pico_int.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LA IMAGEN: el nucleo dibuja de a una linea, en RGB565, con el paso que se le da. Con el paso = el ancho del modo
 * (320 o 256) la imagen queda con las filas seguidas: se sube entera a la textura de una vez */
#define VID_ANCHO 320
#define VID_ALTO 240
static uint16_t gVideo[VID_ANCHO * (VID_ALTO + 8)];
static int gLineaInicio = 8, gLineas = 224, gColumnas = 320;
static uint32_t gNumero = 0;

/* EL SONIDO: lo que el nucleo genera en cada cuadro (estereo, 16 bits) va a una cola circular; el hilo de audio la
 * vacia. Un solo escritor y un solo lector: los indices solo crecen y cada uno escribe el suyo */
#define COLA 8192 /* muestras estereo (potencia de 2) */
static int16_t gCola[COLA * 2];
static volatile uint32_t gEscrito = 0, gLeido = 0;
static uint32_t gFraccion = 0; /* 16.16: la parte de muestra que el lector ya avanzo */
static int16_t gSonidoCuadro[2048 * 2];
static int gHz = 22050;
static volatile int gVolumen = 256; /* 0..256 */
static int gCargado = 0; /* hay una ROM cargada (para rehacer el sonido al cambiar la calidad) */
static void aplicar_dibujo(void);
static int gRapido = 0, gRapidoActivo = 0; /* el dibujo rapido: pedido / puesto (ver aplicar_dibujo) */

static void sonido_escribir(int bytes) {
    const int n = bytes / 4;
    const uint32_t e = gEscrito;
    int i;
    if ((int)(e - gLeido) + n > COLA) return; /* (llena: el audio no esta corriendo) */
    for (i = 0; i < n; i++) {
        const uint32_t k = (e + i) & (COLA - 1);
        gCola[k * 2] = gSonidoCuadro[i * 2];
        gCola[k * 2 + 1] = gSonidoCuadro[i * 2 + 1];
    }
    gEscrito = e + n;
}

int emu_sonido_en_cola(void) { return (int)(gEscrito - gLeido); }

void emu_sonido_volumen(int v) { gVolumen = v < 0 ? 0 : (v > 256 ? 256 : v); }

/* el lector: lo que pide el mezclador, con interpolacion lineal. La consola genera a gHz y la salida de audio va a
 * gHzSalida (fija: cambiar la frecuencia de la consola no toca el dispositivo); el paso base es la razon entre las
 * dos y se corre hasta un 1 % para que la cola se quede en lo que pide de una vez mas ~2 cuadros (el video manda: la
 * consola corre a su reloj y el sonido lo sigue sin cortes). En el N95 el mezclador pide 1024 muestras juntas */
static int gPedidoMax = 0;
static int gHzSalida = 44100;
void emu_sonido_mezclar(int* acc, int frames) {
    const int hz = gHz;
    const uint32_t base = (uint32_t)(((uint64_t)hz << 16) / (uint32_t)gHzSalida);
    int objetivo;
    int hay = (int)(gEscrito - gLeido);
    if (frames > gPedidoMax) gPedidoMax = frames;
    objetivo = (int)((int64_t)gPedidoMax * hz / gHzSalida) + hz / 30;
    int32_t paso;
    uint32_t pos = gLeido, frac = gFraccion;
    const int vol = gVolumen;
    int i;
    {
        const int desvio = hay - objetivo;
        int ajuste = (int)((int64_t)desvio * (int32_t)base / (objetivo * 16));
        const int tope = (int)(base / 100);
        if (ajuste > tope) ajuste = tope;
        if (ajuste < -tope) ajuste = -tope;
        paso = (int32_t)base + ajuste;
    }
    for (i = 0; i < frames; i++) {
        uint32_t k0, k1;
        int l0, r0, l1, r1;
        if ((int)(gEscrito - pos) < 2) break; /* (vacia: lo que falta queda en silencio) */
        k0 = pos & (COLA - 1);
        k1 = (pos + 1) & (COLA - 1);
        l0 = gCola[k0 * 2];
        r0 = gCola[k0 * 2 + 1];
        l1 = gCola[k1 * 2];
        r1 = gCola[k1 * 2 + 1];
        acc[i * 2] += ((l0 + (((l1 - l0) * (int)(frac >> 1)) >> 15)) * vol) >> 8;
        acc[i * 2 + 1] += ((r0 + (((r1 - r0) * (int)(frac >> 1)) >> 15)) * vol) >> 8;
        frac += (uint32_t)paso;
        pos += frac >> 16;
        frac &= 0xffff;
    }
    gFraccion = frac;
    gLeido = pos;
}

/* ------------------------------------------------------------------------------------- lo que pide el nucleo */
void lprintf(const char* fmt, ...) {
    char texto[256];
    va_list va;
    va_start(va, fmt);
    vsnprintf(texto, sizeof(texto), fmt, va);
    va_end(va);
    texto[sizeof(texto) - 1] = 0;
    {
        size_t n = strlen(texto);
        while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) texto[--n] = 0;
    }
    emu_mensaje(texto);
}

void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count) {
    (void)start_col;
    if (col_count <= 0 || col_count > VID_ANCHO) col_count = VID_ANCHO;
    if (line_count <= 0 || line_count > VID_ALTO) line_count = VID_ALTO;
    if (start_line < 0 || start_line + line_count > VID_ALTO) start_line = 0;
    gLineaInicio = start_line;
    gLineas = line_count;
    gColumnas = col_count;
    memset(gVideo, 0, sizeof(gVideo));
    /* (con el dibujo rapido no: PicoDrawSetOutBuf tambien mueve su imagen de 8 bits, Draw2FB, y la pondria en gVideo
     * con otro ancho; ese queda en su buffer propio y convertir_rapido lo pasa a gVideo) */
    if (!gRapidoActivo) PicoDrawSetOutBuf(gVideo, col_count * 2);
}

void emu_32x_startup(void) {}

void* plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed) {
    void* p;
    (void)addr;
    (void)need_exec;
    (void)is_fixed;
    p = malloc(size);
    if (p) memset(p, 0, size);
    return p;
}
void* plat_mremap(void* ptr, size_t oldsize, size_t newsize) {
    (void)oldsize;
    return realloc(ptr, newsize);
}
void plat_munmap(void* ptr, size_t size) {
    (void)size;
    free(ptr);
}
void* plat_mem_get_for_drc(size_t size) {
    (void)size;
    return NULL;
}
int plat_mem_set_exec(void* ptr, size_t size) {
    (void)ptr;
    (void)size;
    return 0;
}
void cache_flush_d_inval_i(void* start_addr, void* end_addr) {
    (void)start_addr;
    (void)end_addr;
}

/* el Mega CD (no se usa: sin las pistas de audio comprimidas) */
int mp3_get_bitrate(void* f, int size) {
    (void)f;
    (void)size;
    return -1;
}
void mp3_start_play(void* f, int pos) {
    (void)f;
    (void)pos;
}
void mp3_update(s32* buffer, int length, int stereo) {
    (void)buffer;
    (void)length;
    (void)stereo;
}
int ogg_get_length(void* f) {
    (void)f;
    return -1;
}
void ogg_start_play(void* f, int sample_offset) {
    (void)f;
    (void)sample_offset;
}
void ogg_stop_play(void) {}
void ogg_update(s32* buffer, int length, int stereo) {
    (void)buffer;
    (void)length;
    (void)stereo;
}

/* ------------------------------------------------------------------------------------------------- emu.h */
void emu_iniciar(int hzSalida, int hz) {
    gHzSalida = hzSalida > 0 ? hzSalida : hz;
    gHz = hz;
    PicoIn.opt = POPT_EN_STEREO | POPT_EN_FM | POPT_EN_PSG | POPT_EN_Z80 | POPT_DIS_32C_BORDER;
    PicoIn.sndRate = hz;
    PicoIn.autoRgnOrder = 0x184; /* USA, Europa, Japon */
    PicoInit();
    PicoDrawSetOutFormat(PDF_RGB555, 0); /* (sin USE_BGR555 es RGB565) */
    PicoDrawSetOutBuf(gVideo, VID_ANCHO * 2);
}

void emu_terminar(void) { PicoExit(); }

int emu_cargar(const char* ruta) {
    /* (la tabla de cartuchos especiales es la de adentro del nucleo: un archivo que no existe la elige) */
    const enum media_type_e tipo = PicoLoadMedia(ruta, NULL, 0, "carthw.cfg", NULL, NULL, NULL);
    if (tipo != PM_MD_CART && tipo != PM_MARK3 && tipo != PM_PICO) {
        lprintf("emu: no se pudo cargar %s (tipo %d)", ruta, (int)tipo);
        return 0;
    }
    lprintf("emu: cartucho cargado (tipo %d)", (int)tipo);
    PicoLoopPrepare();
    lprintf("emu: listo para correr");
    memset(gSonidoCuadro, 0, sizeof(gSonidoCuadro));
    PicoIn.sndOut = gSonidoCuadro;
    PicoIn.writeSound = sonido_escribir;
    PsndRerate(0);
    gCargado = 1;
    aplicar_dibujo();
    emu_entrada(emu_soporta_team_player() ? EMU_TEAM_PLAYER_1 : EMU_DOS_MANDOS);
    return 1;
}

void emu_entrada(int entrada) {
    switch (entrada) {
    case EMU_TEAM_PLAYER_1:
        PicoSetInputDevice(0, PICO_INPUT_PAD_TEAM);
        PicoSetInputDevice(1, PICO_INPUT_PAD_3BTN);
        break;
    case EMU_TEAM_PLAYER_2:
        PicoSetInputDevice(0, PICO_INPUT_PAD_3BTN);
        PicoSetInputDevice(1, PICO_INPUT_PAD_TEAM);
        break;
    case EMU_EA_4WAY:
        PicoSetInputDevice(0, PICO_INPUT_PAD_4WAY);
        PicoSetInputDevice(1, PICO_INPUT_PAD_4WAY);
        break;
    default:
        PicoSetInputDevice(0, PICO_INPUT_PAD_6BTN);
        PicoSetInputDevice(1, PICO_INPUT_PAD_6BTN);
        break;
    }
}

int emu_soporta_team_player(void) {
    int i;
    if (PicoIn.AHW & PAHW_SMS) return 0;
    /* (los bytes pueden venir de a pares dados vuelta: alcanza con buscar el '4' en los 16) */
    for (i = 0x90; i < 0xa0; i++)
        if (media_id_header[i] == '4') return 1;
    return 0;
}

void emu_sonido_calidad(int filtro, int fmAlta) {
    PicoIn.opt &= ~(POPT_EN_SNDFILTER | POPT_EN_FM_FILTER);
    if (filtro) PicoIn.opt |= POPT_EN_SNDFILTER;
    if (fmAlta) PicoIn.opt |= POPT_EN_FM_FILTER;
    PicoIn.sndFilterAlpha = 60 * 0x10000 / 100;
    if (gCargado) PsndRerate(1);
}

void emu_sonido_hz(int hz) {
    /* (la salida de audio sigue igual: el lector convierte de la frecuencia nueva a la de la salida) */
    gHz = hz;
    PicoIn.sndRate = hz;
    if (gCargado) PsndRerate(1);
}

/* EL DIBUJO RAPIDO (el de PicoDrive por tiles: el cuadro entero de una vez, en 8 bits con paleta en Pico.est.Draw2FB;
 * mucho mas barato que el preciso de a una linea, pero sin los cambios a mitad de pantalla). Solo Mega Drive */
static void aplicar_dibujo(void) {
    const int rapido = gRapido && !(PicoIn.AHW & PAHW_8BIT);
    if (rapido) {
        PicoIn.opt |= POPT_ALT_RENDERER;
        PicoDrawSetOutFormat(PDF_NONE, 0);
    } else {
        PicoIn.opt &= ~POPT_ALT_RENDERER;
        PicoDrawSetOutFormat(PDF_RGB555, 0);
        PicoDrawSetOutBuf(gVideo, gColumnas * 2);
    }
    gRapidoActivo = rapido;
    Pico.m.dirtyPal = 1;
}

void emu_dibujo_rapido(int si) {
    gRapido = si != 0;
    if (gCargado) aplicar_dibujo();
}

/* la imagen de 8 bits del dibujo rapido -> gVideo (RGB565 con la paleta del cuadro). Draw2FB tiene 328 de ancho: 8
 * columnas de margen a la izquierda */
static void convertir_rapido(void) {
    const unsigned char* ps = Pico.est.Draw2FB + gLineaInicio * 328 + 8;
    const unsigned short* pal = Pico.est.HighPal;
    uint16_t* pd = gVideo + gLineaInicio * gColumnas;
    int y, x;
    if (Pico.m.dirtyPal) PicoDrawUpdateHighPal();
    for (y = 0; y < gLineas; y++, ps += 328, pd += gColumnas)
        for (x = 0; x < gColumnas; x++) pd[x] = pal[ps[x]];
}

void emu_reiniciar(void) { PicoReset(); }

void emu_cuadro(const uint16_t pads[4], int dibujar) {
    int i;
    for (i = 0; i < 4; i++) PicoIn.pad[i] = pads[i];
    PicoIn.skipFrame = dibujar ? 0 : 1;
    PicoFrame();
    if (dibujar) {
        if (gRapidoActivo) convertir_rapido();
        gNumero++;
    }
}

int emu_hz_video(void) {
    pint_ret_t r;
    PicoGetInternal(PI_ISPAL, &r);
    return r.vint ? 50 : 60;
}

const uint16_t* emu_imagen(int* ancho, int* alto) {
    *ancho = gColumnas;
    *alto = gLineas;
    return gVideo + gLineaInicio * gColumnas;
}

uint32_t emu_imagen_numero(void) { return gNumero; }
