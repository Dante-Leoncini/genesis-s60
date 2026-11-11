#include "tactil.h"
#include "emu.h"
#include "pantalla.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern "C" unsigned char fontdata8x8[64 * 16];   // (picodrive/platform/libpicofe/fonts.c)

namespace {
enum { SP_CRUZ, SP_A, SP_B, SP_C, SP_START, SP_MENU, SPRITES };
int gSprites[SPRITES];
bool gListo = false;

// lo apretado en el ultimo tactil_leer (para dibujarlo mas fuerte)
uint16_t gApretado = 0;
bool gMenuApretado = false;

// QUE MANEJA CADA DEDO: se decide al apoyarlo (donde empezo) y no cambia hasta que se levanta
enum { ROL_NADA, ROL_CRUZ, ROL_BOTONES, ROL_MENU };
struct Dedo {
    int id;
    int rol;
    bool vivo;
};
Dedo gDedos[10];
int gDedosN = 0;

// LA DISPOSICION, en funcion del tamano de la pantalla: 'u' es la unidad (un quinto de lo mas corto)
struct Circulo {
    float x, y, r;
};
struct Disposicion {
    Circulo cruz;
    Circulo boton[3];   // A B C
    float startX, startY, startW, startH;
    float menuX, menuY, menuL;
};

Disposicion disponer(int pw, int ph) {
    Disposicion d;
    const float u = (float)(pw < ph ? pw : ph) / 4.5f;
    d.cruz.x = 1.25f * u;
    d.cruz.y = ph - 1.25f * u;
    d.cruz.r = 1.05f * u;
    // A B C subiendo hacia la derecha, como en el mando de 3 botones
    const float rb = 0.42f * u;
    d.boton[0].x = pw - 2.95f * u; d.boton[0].y = ph - 0.75f * u; d.boton[0].r = rb;
    d.boton[1].x = pw - 1.9f * u;  d.boton[1].y = ph - 1.0f * u;  d.boton[1].r = rb;
    d.boton[2].x = pw - 0.85f * u; d.boton[2].y = ph - 1.25f * u; d.boton[2].r = rb;
    d.startW = 1.3f * u;
    d.startH = 0.5f * u;
    d.startX = pw * 0.5f - d.startW * 0.5f;
    d.startY = ph - d.startH - 0.15f * u;
    d.menuL = 0.7f * u;
    d.menuX = pw - d.menuL - 0.15f * u;
    d.menuY = 0.15f * u;
    return d;
}

float distancia(float x, float y, const Circulo& c) { return sqrtf((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y)); }

// ---------------------------------------------------------------------------------------------- los graficos
// (RGBA en memoria: R G B A, en ese orden de bytes)
uint32_t rgba(int r, int g, int b, int a) {
    uint8_t p[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

// un pixel encima de otro (los dos con alfa normal)
void mezclar(uint32_t* d, int r, int g, int b, float a) {
    if (a <= 0) return;
    if (a > 1) a = 1;
    uint8_t p[4];
    memcpy(p, d, 4);
    const float ad = p[3] / 255.0f, ao = a + ad * (1 - a);
    if (ao <= 0) return;
    p[0] = (uint8_t)((r * a + p[0] * ad * (1 - a)) / ao);
    p[1] = (uint8_t)((g * a + p[1] * ad * (1 - a)) / ao);
    p[2] = (uint8_t)((b * a + p[2] * ad * (1 - a)) / ao);
    p[3] = (uint8_t)(ao * 255);
    memcpy(d, p, 4);
}

// un circulo lleno con borde, suavizado
void circulo(uint32_t* img, int lado, float cx, float cy, float r, int cr, int cg, int cb, float a, float borde) {
    for (int y = 0; y < lado; y++)
        for (int x = 0; x < lado; x++) {
            const float d = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            const float dentro = r - d + 0.5f;   // (1 adentro, 0 afuera, intermedio en el borde)
            if (dentro <= 0) continue;
            const float k = dentro > 1 ? 1 : dentro;
            mezclar(&img[y * lado + x], cr, cg, cb, a * k);
            if (borde > 0 && d > r - borde) mezclar(&img[y * lado + x], 255, 255, 255, 0.8f * k);
        }
}

// un rectangulo con las puntas redondeadas
void caja(uint32_t* img, int w, int h, float x0, float y0, float x1, float y1, float radio, int cr, int cg, int cb,
          float a) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float qx = px < x0 + radio ? x0 + radio : px > x1 - radio ? x1 - radio : px;
            const float qy = py < y0 + radio ? y0 + radio : py > y1 - radio ? y1 - radio : py;
            const float d = sqrtf((px - qx) * (px - qx) + (py - qy) * (py - qy));
            const float dentro = radio - d + 0.5f;
            if (dentro <= 0) continue;
            mezclar(&img[y * w + x], cr, cg, cb, a * (dentro > 1 ? 1 : dentro));
        }
}

// texto con la letra de 8x8 agrandada 'k' veces, centrado en (cx, cy)
void letras(uint32_t* img, int w, int h, const char* s, float cx, float cy, int k, int cr, int cg, int cb) {
    const int n = (int)strlen(s);
    const int x0 = (int)(cx - n * 8 * k * 0.5f), y0 = (int)(cy - 8 * k * 0.5f);
    for (int c = 0; c < n; c++)
        for (int l = 0; l < 8; l++) {
            const unsigned char fila = fontdata8x8[(unsigned char)s[c] * 8 + l];
            for (int b = 0; b < 8; b++) {
                if (!(fila & (0x80 >> b))) continue;
                for (int yy = 0; yy < k; yy++)
                    for (int xx = 0; xx < k; xx++) {
                        const int x = x0 + (c * 8 + b) * k + xx, y = y0 + l * k + yy;
                        if (x >= 0 && y >= 0 && x < w && y < h) mezclar(&img[y * w + x], cr, cg, cb, 1);
                    }
            }
        }
}

int sprite_boton(const char* letra) {
    const int L = 64;
    uint32_t* img = (uint32_t*)calloc(L * L, 4);
    if (!img) return -1;
    circulo(img, L, L * 0.5f, L * 0.5f, L * 0.5f - 1, 40, 40, 48, 0.85f, 2.5f);
    letras(img, L, L, letra, L * 0.5f, L * 0.5f, 4, 255, 255, 255);
    const int id = pantalla_sprite_crear(img, L, L);
    free(img);
    return id;
}

int sprite_cruz() {
    const int L = 128;
    uint32_t* img = (uint32_t*)calloc(L * L, 4);
    if (!img) return -1;
    circulo(img, L, L * 0.5f, L * 0.5f, L * 0.5f - 1, 40, 40, 48, 0.45f, 2.0f);
    // la cruz: dos barras redondeadas
    const float a = L * 0.17f;
    caja(img, L, L, L * 0.5f - a, L * 0.1f, L * 0.5f + a, L * 0.9f, 6, 25, 25, 30, 0.95f);
    caja(img, L, L, L * 0.1f, L * 0.5f - a, L * 0.9f, L * 0.5f + a, 6, 25, 25, 30, 0.95f);
    // las flechas (triangulos chicos)
    for (int t = 0; t < 4; t++)
        for (int y = 0; y < L; y++)
            for (int x = 0; x < L; x++) {
                float u = x + 0.5f - L * 0.5f, v = y + 0.5f - L * 0.5f;   // (girado para cada lado)
                float uu = t == 0 ? u : t == 1 ? -u : t == 2 ? v : -v;
                float vv = t == 0 ? -v : t == 1 ? v : t == 2 ? u : -u;
                // apuntando hacia arriba en (uu, vv): punta en vv = -0.36L, base en vv = -0.24L
                const float base = -0.24f * L, punta = -0.36f * L;
                if (vv < punta || vv > base) continue;
                const float ancho = (vv - punta) / (base - punta) * 0.09f * L;
                if (uu < -ancho || uu > ancho) continue;
                mezclar(&img[y * L + x], 210, 210, 220, 0.9f);
            }
    const int id = pantalla_sprite_crear(img, L, L);
    free(img);
    return id;
}

int sprite_start() {
    const int W = 128, H = 48;
    uint32_t* img = (uint32_t*)calloc(W * H, 4);
    if (!img) return -1;
    caja(img, W, H, 1, 1, W - 1, H - 1, H * 0.5f - 1, 40, 40, 48, 0.85f);
    letras(img, W, H, "START", W * 0.5f, H * 0.5f, 2, 255, 255, 255);
    const int id = pantalla_sprite_crear(img, W, H);
    free(img);
    return id;
}

int sprite_menu() {
    const int L = 64;
    uint32_t* img = (uint32_t*)calloc(L * L, 4);
    if (!img) return -1;
    caja(img, L, L, 1, 1, L - 1, L - 1, 10, 40, 40, 48, 0.8f);
    for (int i = 0; i < 3; i++)   // (tres rayas: el menu)
        caja(img, L, L, L * 0.25f, L * (0.3f + i * 0.17f), L * 0.75f, L * (0.3f + i * 0.17f) + 5, 2.5f, 255, 255, 255,
             1);
    const int id = pantalla_sprite_crear(img, L, L);
    free(img);
    return id;
}
}

bool tactil_iniciar() {
    if (gListo) return true;
    gSprites[SP_CRUZ] = sprite_cruz();
    if (gSprites[SP_CRUZ] < 0) return false;   // (la pantalla no tiene sprites)
    gSprites[SP_A] = sprite_boton("A");
    gSprites[SP_B] = sprite_boton("B");
    gSprites[SP_C] = sprite_boton("C");
    gSprites[SP_START] = sprite_start();
    gSprites[SP_MENU] = sprite_menu();
    gListo = true;
    return true;
}

uint16_t tactil_leer(const Toque* t, int n, int pw, int ph, bool* menu) {
    const Disposicion d = disponer(pw, ph);
    uint16_t pad = 0;
    bool menuAhora = false;
    for (int k = 0; k < gDedosN; k++) gDedos[k].vivo = false;
    for (int i = 0; i < n; i++) {
        const float x = (float)t[i].x, y = (float)t[i].y;
        // el rol del dedo: el que tenia, o segun donde se apoyo
        Dedo* dedo = 0;
        for (int k = 0; k < gDedosN; k++)
            if (gDedos[k].id == t[i].id) dedo = &gDedos[k];
        if (!dedo && gDedosN < 10) {
            dedo = &gDedos[gDedosN++];
            dedo->id = t[i].id;
            if (distancia(x, y, d.cruz) < d.cruz.r * 1.3f) dedo->rol = ROL_CRUZ;
            else if (x >= d.menuX - d.menuL * 0.3f && y <= d.menuY + d.menuL * 1.3f) dedo->rol = ROL_MENU;
            else dedo->rol = ROL_BOTONES;
        }
        if (!dedo) continue;
        dedo->vivo = true;
        if (dedo->rol == ROL_CRUZ) {
            // la direccion: ocho sectores, con una zona muerta al centro
            const float dx = x - d.cruz.x, dy = y - d.cruz.y;
            const float largo = sqrtf(dx * dx + dy * dy);
            if (largo < d.cruz.r * 0.22f) continue;
            if (fabsf(dx) > 0.414f * fabsf(dy)) pad |= dx > 0 ? MD_DER : MD_IZQ;
            if (fabsf(dy) > 0.414f * fabsf(dx)) pad |= dy > 0 ? MD_ABAJO : MD_ARRIBA;
        } else if (dedo->rol == ROL_MENU) {
            menuAhora = true;
        } else {
            // el boton mas cercano debajo del dedo (se puede deslizar de uno a otro)
            static const uint16_t bits[3] = { MD_A, MD_B, MD_C };
            int mejor = -1;
            float mejorD = 1e9f;
            for (int b = 0; b < 3; b++) {
                const float dd = distancia(x, y, d.boton[b]);
                if (dd < d.boton[b].r * 1.4f && dd < mejorD) {
                    mejor = b;
                    mejorD = dd;
                }
            }
            if (mejor >= 0) pad |= bits[mejor];
            else if (x >= d.startX - 8 && x <= d.startX + d.startW + 8 && y >= d.startY - 8 &&
                     y <= d.startY + d.startH + 8)
                pad |= MD_START;
        }
    }
    // los dedos que se levantaron se olvidan
    int j = 0;
    for (int k = 0; k < gDedosN; k++)
        if (gDedos[k].vivo) gDedos[j++] = gDedos[k];
    gDedosN = j;
    // el menu: al soltar el boton (como un boton de verdad)
    *menu = gMenuApretado && !menuAhora;
    gMenuApretado = menuAhora;
    gApretado = pad;
    return pad;
}

void tactil_dibujar(int pw, int ph) {
    if (!gListo) return;
    const Disposicion d = disponer(pw, ph);
    const float suave = 0.45f, fuerte = 0.85f;
    const int cr = (int)d.cruz.r;
    const bool cruz = (gApretado & (MD_ARRIBA | MD_ABAJO | MD_IZQ | MD_DER)) != 0;
    pantalla_sprite(gSprites[SP_CRUZ], (int)d.cruz.x - cr, (int)d.cruz.y - cr, 2 * cr, 2 * cr, cruz ? fuerte : suave);
    static const uint16_t bits[3] = { MD_A, MD_B, MD_C };
    for (int b = 0; b < 3; b++) {
        const int r = (int)d.boton[b].r;
        pantalla_sprite(gSprites[SP_A + b], (int)d.boton[b].x - r, (int)d.boton[b].y - r, 2 * r, 2 * r,
                        (gApretado & bits[b]) ? fuerte : suave);
    }
    pantalla_sprite(gSprites[SP_START], (int)d.startX, (int)d.startY, (int)d.startW, (int)d.startH,
                    (gApretado & MD_START) ? fuerte : suave);
    pantalla_sprite(gSprites[SP_MENU], (int)d.menuX, (int)d.menuY, (int)d.menuL, (int)d.menuL,
                    gMenuApretado ? fuerte : suave);
}
