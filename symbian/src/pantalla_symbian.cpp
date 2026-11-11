// La imagen de la consola en el N95 (src/pantalla.h) POR CPU, sin el GPU: subir una textura al MBX costaba 36 ms por
// cuadro (el driver la convierte a su formato), mas que emular el cuadro entero.
//
// Direct Screen Access: el emulador dibuja directo en la pantalla desde su proceso. Pixel perfect: 1:1, centrada, sin
// estirar (Mega Bomberman es de 256 columnas). Dos caminos:
//   - DIRECTO (el normal): la imagen se escribe en la memoria de la pantalla (HAL: EDisplayMemoryAddress), girada al
//     horizontal y pasada al formato de la pantalla en una sola pasada (~1-2 ms). La memoria es la del panel de pie
//     (240x320): el giro sale de la orientacion del dispositivo del DSA. config.txt: rotacion=90 / 270 lo fuerza (si
//     la imagen saliera dada vuelta), pantalla=blit usa el otro camino.
//   - BLIT: un bitmap RGB565 copiado con el BitBlt del sistema (~7 ms: convierte y gira pixel por pixel).
// Despues de dibujar, el contenedor llama a Update() (lo que en OpenGL seria el swap).
#include "pantalla.h"
#include "pantalla_symbian.h"
#include "plataforma.h"
#include "w3dlog.h"
#include <w32std.h>
#include <fbs.h>
#include <bitstd.h>
#include <bitdev.h>
#include <hal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
CDirectScreenAccess* gDsa = 0;
CFbsBitmap* gBitmap = 0;
const uint16_t* gImagen = 0;   // la ultima imagen de la consola (la del juego: sigue viva hasta el proximo cuadro)
int gAncho = 0, gAlto = 0;
// lo que ocupo el cuadro anterior: al cambiar de modo (o volver el DSA) se pinta de negro toda la pantalla
int gAntesX = -1, gAntesY = -1, gAntesW = -1, gAntesH = -1;
// lo que cambio en la pantalla desde la ultima presentacion (en el logico; vacio = nada)
TRect gCambiado;
// el cuadro anterior tal como quedo en la pantalla (para escribir solo lo que cambia) y las filas que cambiaron
uint16_t gPrevio[320 * 240];
bool gPrevioValido = false, gTodo = true;
int gPrevioW = 0, gPrevioH = 0;
int gFilaMin = -1, gFilaMax = -1;

// el camino directo
bool gDirecto = false;
uint8_t* gMemoria = 0;          // la memoria de la pantalla
int gFisW = 0, gFisH = 0;       // el panel (de pie: 240 x 320)
int gPaso = 0;                  // bytes por linea del panel
int gBits = 0;                  // 16 o 32
int gGiro = 0;                  // 0 (sin girar), 90 o 270
int gForzarGiro = -1;
bool gForzarBlit = false;

void leer_config() {
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%sconfig.txt", plat_carpeta_datos());
    FILE* f = fopen(ruta, "r");
    if (!f) return;
    char linea[256];
    while (fgets(linea, sizeof(linea), f)) {
        if (!strncmp(linea, "rotacion=", 9)) gForzarGiro = atoi(linea + 9);
        else if (!strncmp(linea, "pantalla=blit", 13)) gForzarBlit = true;
    }
    fclose(f);
}

// lo que hace falta para escribir directo: la memoria del panel, su tamano, su formato y como esta girado
void preparar_directo() {
    gDirecto = false;
    if (gForzarBlit || !gDsa) return;
    TInt direccion = 0, x = 0, y = 0;
    if (HAL::Get(HALData::EDisplayMemoryAddress, direccion) != KErrNone || !direccion) return;
    HAL::Get(HALData::EDisplayXPixels, x);
    HAL::Get(HALData::EDisplayYPixels, y);
    // (los bits por pixel, el largo de la linea y donde empieza el primer pixel se piden PARA UN MODO: el numero del
    // modo actual va como entrada en el mismo valor)
    TInt modoHal = 0;
    HAL::Get(HALData::EDisplayMode, modoHal);
    TInt bitsHal = modoHal, paso = modoHal, primero = modoHal;
    HAL::Get(HALData::EDisplayBitsPerPixel, bitsHal);
    HAL::Get(HALData::EDisplayOffsetBetweenLines, paso);
    if (HAL::Get(HALData::EDisplayOffsetToFirstPixel, primero) != KErrNone) primero = 0;
    CFbsScreenDevice* dispositivo = gDsa->ScreenDevice();
    const TDisplayMode modo = dispositivo->DisplayMode();
    int bits = modo == EColor64K ? 16 : (modo == EColor16MU || modo == EColor16MA) ? 32 : 0;
    if (bitsHal == 16 || bitsHal == 32) bits = bitsHal;
    else if (bitsHal == 24) bits = 32;   // (EColor16MU: 24 bits de color en palabras de 32)
    w3dLogf("pantalla: HAL modo %d, %d bits, primer pixel en +%d", (int)modoHal, (int)bitsHal, (int)primero);
    direccion += primero;
    const TSize logico = dispositivo->SizeInPixels();
    int giro = 0;
    switch (dispositivo->Orientation()) {
    case CFbsBitGc::EGraphicsOrientationRotated90: giro = 90; break;
    case CFbsBitGc::EGraphicsOrientationRotated270: giro = 270; break;
    default:
        // (sin orientacion pero el logico acostado y el panel de pie: girado igual)
        giro = (logico.iWidth > logico.iHeight) != (x > y) ? 90 : 0;
        break;
    }
    // (en el N95 el HAL dice panel de 240x320 pero cada linea de la memoria es de 320 pixeles: con la aplicacion
    // acostada la memoria ya esta acostada, sin girar)
    if (bits && paso / (bits / 8) == logico.iWidth && logico.iWidth != x) {
        x = logico.iWidth;
        y = logico.iHeight;
        giro = 0;
    }
    if (gForzarGiro == 0 || gForzarGiro == 90 || gForzarGiro == 270) giro = gForzarGiro;
    w3dLogf("pantalla: panel %dx%d en 0x%08x, %d bytes por linea, modo %d (%d bits), logico %dx%d, giro %d", (int)x,
            (int)y, (unsigned)direccion, (int)paso, (int)modo, bits, (int)logico.iWidth, (int)logico.iHeight, giro);
    if (!bits || x <= 0 || y <= 0 || paso < x * bits / 8) return;
    // (el logico tiene que ser el panel, girado o no)
    const bool cuadra = giro == 0 ? (logico.iWidth == x && logico.iHeight == y) : (logico.iWidth == y && logico.iHeight == x);
    if (!cuadra) return;
    gMemoria = (uint8_t*)direccion;
    gFisW = x;
    gFisH = y;
    gPaso = paso;
    gBits = bits;
    gGiro = giro;
    gDirecto = true;
}

inline uint32_t a32_calcular(uint32_t p) {
    return 0xff000000u | ((p & 0xf800u) << 8) | ((p & 0xe000u) << 3) | ((p & 0x07e0u) << 5) | ((p & 0x0600u) >> 1) |
           ((p & 0x001fu) << 3) | ((p & 0x001cu) >> 2);
}
// RGB565 -> 32 bits con dos tablas de 256 (2 KB, entran en la cache): cada parte del resultado sale de bits de un
// solo byte de la entrada, asi que a32(p) = alto[p >> 8] | bajo[p & 255]
uint32_t gAlto32[256], gBajo32[256];
bool gTablas = false;
void tablas() {
    for (uint32_t i = 0; i < 256; i++) {
        gAlto32[i] = a32_calcular(i << 8);
        gBajo32[i] = a32_calcular(i);
    }
    gTablas = true;
}
inline uint32_t a32(uint16_t p) { return gAlto32[p >> 8] | gBajo32[p & 255]; }

// el panel entero en negro
void negro_directo() {
    gPrevioValido = false;
    for (int y = 0; y < gFisH; y++) memset(gMemoria + y * gPaso, 0, gFisW * gBits / 8);
}

// la imagen (w x h) en (lx, ly) del logico, girada al panel. Se recorre el panel por filas (escrituras seguidas)
void dibujar_directo(const uint16_t* img, int w, int h, int lx, int ly) {
    if (gGiro == 0) {
        // LA MEMORIA DE LA PANTALLA ES LENTA (no pasa por la cache): escribir los 230 KB del cuadro entero costaba
        // ~6,8 ms. Se guarda una copia del cuadro anterior (en la cache) y de cada fila se escribe solo el tramo que
        // cambio (en Mega Bomberman el escenario queda quieto). Cada tramo se convierte en una fila propia y se copia
        // de una (escrituras en rafaga)
        static uint32_t fila32[512];
        if (!gPrevioValido || w != gPrevioW || h != gPrevioH) {
            memset(gPrevio, 0, sizeof(gPrevio));
            gPrevioW = w;
            gPrevioH = h;
            gPrevioValido = true;
            gTodo = true;
        }
        for (int j = 0; j < h; j++) {
            const uint16_t* s = img + j * w;
            uint16_t* p = gPrevio + j * w;
            int a = 0, b = w;   // el tramo [a, b) que cambio
            if (!gTodo) {
                // (de a dos pixeles: w es par)
                const uint32_t* s2 = (const uint32_t*)s;
                const uint32_t* p2 = (const uint32_t*)p;
                const int n = w / 2;
                int i = 0;
                while (i < n && s2[i] == p2[i]) i++;
                if (i == n) continue;   // (fila igual)
                int k = n - 1;
                while (k > i && s2[k] == p2[k]) k--;
                a = i * 2;
                b = k * 2 + 2;
            }
            memcpy(p + a, s + a, (b - a) * 2);
            uint8_t* fila = gMemoria + (ly + j) * gPaso;
            if (gBits == 16) {
                memcpy(fila + (lx + a) * 2, s + a, (b - a) * 2);
            } else {
                for (int i = a; i < b; i++) fila32[i - a] = a32(s[i]);
                memcpy(fila + (lx + a) * 4, fila32, (b - a) * 4);
            }
            if (gFilaMin < 0 || j < gFilaMin) gFilaMin = j;
            if (j > gFilaMax) gFilaMax = j;
        }
        gTodo = false;
        return;
    }
    // girado: la columna i de la imagen es una fila del panel
    //   90:  logico (x, y) -> panel (W-1-y, x)        270: logico (x, y) -> panel (y, H-1-x)
    // LA MEMORIA DE LA PANTALLA NO PASA POR LA CACHE: escribir de a un pixel costaba ~9 ms el cuadro. Cada fila del
    // panel se arma en una fila propia (en la cache) y se copia entera con memcpy (escrituras en rafaga)
    static uint32_t fila32[512];
    uint16_t* fila16 = (uint16_t*)fila32;
    const int bytes = gBits / 8;
    const int px0 = gGiro == 90 ? gFisW - h - ly : ly;   // donde empieza el tramo de la imagen en la fila del panel
    for (int i = 0; i < w; i++) {
        const int py = gGiro == 90 ? lx + i : gFisH - 1 - (lx + i);
        const uint16_t* s = img + i;
        if (gGiro == 90) {
            // (la fila 0 de la imagen va a la derecha: el tramo se llena de atras para adelante)
            if (gBits == 16) {
                for (int j = 0; j < h; j++) fila16[h - 1 - j] = s[j * w];
            } else {
                for (int j = 0; j < h; j++) fila32[h - 1 - j] = a32(s[j * w]);
            }
        } else {
            if (gBits == 16) {
                for (int j = 0; j < h; j++) fila16[j] = s[j * w];
            } else {
                for (int j = 0; j < h; j++) fila32[j] = a32(s[j * w]);
            }
        }
        memcpy(gMemoria + py * gPaso + px0 * bytes, fila32, h * bytes);
    }
}
}

void pantalla_symbian_dsa(CDirectScreenAccess* dsa) {
    gDsa = dsa;
    gAntesW = -1;   // (la pantalla pudo quedar tapada: negro de nuevo alrededor)
    gPrevioValido = false;
    if (dsa) preparar_directo();
}

void pantalla_iniciar() {
    if (!gTablas) tablas();
    leer_config();
    if (gDsa) preparar_directo();
    gBitmap = new CFbsBitmap;
    if (gBitmap && gBitmap->Create(TSize(320, 240), EColor64K) != KErrNone) {
        delete gBitmap;
        gBitmap = 0;
    }
}

void pantalla_terminar() {
    delete gBitmap;
    gBitmap = 0;
}

void pantalla_subir(const uint16_t* imagen, int ancho, int alto) {
    if (!imagen) return;
    if (ancho > 320) ancho = 320;
    if (alto > 240) alto = 240;
    gAncho = ancho;
    gAlto = alto;
    gImagen = imagen;
    if (gDirecto || !gBitmap) return;   // (el directo la lee de ahi al dibujar)
    const TInt paso = CFbsBitmap::ScanLineLength(320, EColor64K);
    gBitmap->LockHeap();
    uint8_t* destino = (uint8_t*)gBitmap->DataAddress();
    for (int y = 0; y < alto; y++) memcpy(destino + y * paso, imagen + y * ancho, ancho * 2);
    gBitmap->UnlockHeap();
}

void pantalla_dibujar(int pw, int ph) {
    if (!gDsa || gAncho <= 0) return;
    const int x = (pw - gAncho) / 2, y = (ph - gAlto) / 2;
    const bool cambio = x != gAntesX || y != gAntesY || gAncho != gAntesW || gAlto != gAntesH;
    gAntesX = x;
    gAntesY = y;
    gAntesW = gAncho;
    gAntesH = gAlto;
    if (gDirecto && gImagen) {
        if (cambio) negro_directo();
        gFilaMin = gFilaMax = -1;
        dibujar_directo(gImagen, gAncho, gAlto, x, y);
        // (a la pantalla, solo las filas que cambiaron; todo si se pinto el negro alrededor)
        TRect r;
        if (cambio) r = TRect(0, 0, pw, ph);
        else if (gGiro != 0) r = TRect(x, y, x + gAncho, y + gAlto);
        else if (gFilaMin >= 0) r = TRect(x, y + gFilaMin, x + gAncho, y + gFilaMax + 1);
        if (!r.IsEmpty()) {
            if (gCambiado.IsEmpty()) gCambiado = r;
            else gCambiado.BoundingRect(r);
        }
        return;
    }
    if (!gBitmap) return;
    CFbsBitGc* gc = gDsa->Gc();
    if (cambio) {
        gc->SetPenStyle(CGraphicsContext::ENullPen);
        gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
        gc->SetBrushColor(KRgbBlack);
        gc->DrawRect(TRect(0, 0, pw, ph));
    }
    gc->BitBlt(TPoint(x, y), gBitmap, TRect(0, 0, gAncho, gAlto));
}

void pantalla_symbian_presentar() {
    if (!gDsa) return;
    if (gDirecto && !gCambiado.IsEmpty()) {
        RRegion region;
        region.AddRect(gCambiado);
        gDsa->ScreenDevice()->Update(region);
        region.Close();
        gCambiado = TRect();
    } else {
        gDsa->ScreenDevice()->Update();
    }
}

void pantalla_rectangulo(int, int, int x, int y, int w, int h, uint32_t rgba) {
    if (!gDsa) return;
    CFbsBitGc* gc = gDsa->Gc();
    gc->SetPenStyle(CGraphicsContext::ENullPen);
    gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc->SetBrushColor(TRgb((rgba >> 24) & 255, (rgba >> 16) & 255, (rgba >> 8) & 255));
    gc->DrawRect(TRect(x, y, x + w, y + h));
    if (gDirecto) {
        // (lo de abajo del aviso no cambio en la imagen: el cuadro siguiente se escribe entero, asi el aviso no deja
        // rastro cuando se va)
        gPrevioValido = false;
        const TRect r(x, y, x + w, y + h);
        if (gCambiado.IsEmpty()) gCambiado = r;
        else gCambiado.BoundingRect(r);
    }
}
