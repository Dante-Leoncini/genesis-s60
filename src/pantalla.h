// ---------------------------------------------------------------------------------------------------------------
//  La imagen de la consola (o la del menu) en la pantalla, centrada:
//    - en la PC, con OpenGL (pantalla.cpp)
//    - en el N95, por CPU directo en la pantalla (symbian/src/pantalla_symbian.cpp): siempre 1:1, pixel por pixel
//    - en Symbian^3 / Belle (N8), con OpenGL ES 2.0 (symbian/src/pantalla_gles2.cpp), en el telefono y en la TV
//  La escala y el filtro los elige el juego (las opciones); los sprites son los controles tactiles.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

// LA ESCALA: 1:1 (pixel por pixel); entera (la mas grande que entra: x1 en el telefono, x2 en una TV de 480 lineas,
// x3 en 720p); ajustar (lo mas grande que entra conservando la forma); estirar (toda la pantalla)
enum { ESCALA_1, ESCALA_ENTERA, ESCALA_AJUSTAR, ESCALA_ESTIRAR, ESCALAS };

void pantalla_iniciar();
void pantalla_terminar();
// la escala (ESCALA_*) y el filtro: 'suave' = la imagen filtrada (se ve borrosa al agrandar); 0 = pixel perfect
void pantalla_modo(int escala, int suave);
// sube la imagen nueva (ancho x alto, filas seguidas) si cambio
void pantalla_subir(const uint16_t* imagen, int ancho, int alto);
// dibuja la ultima imagen en una pantalla de pw x ph (la del telefono: de ahi salen las cuentas de los dedos)
void pantalla_dibujar(int pw, int ph);
// lo mismo en la pantalla externa (la TV): sin tocar las cuentas de los dedos
void pantalla_dibujar_externa(int pw, int ph);
// la pantalla en negro (la del telefono, cuando el juego va a la TV)
void pantalla_limpiar(int pw, int ph);
// un rectangulo de color encima (los avisos de los jugadores): x, y, w, h en pixeles de la pantalla
void pantalla_rectangulo(int pw, int ph, int x, int y, int w, int h, uint32_t rgba);
// un punto de la ultima pantalla dibujada -> el pixel de la imagen que se ve ahi (false: afuera de la imagen)
bool pantalla_a_imagen(int sx, int sy, int* ix, int* iy);

// LOS SPRITES (RGBA8888, con transparencia: los controles tactiles). Donde no hay (el N95), crear devuelve -1
int pantalla_sprite_crear(const uint32_t* rgba, int w, int h);
// dibuja el sprite 'id' en x, y, w, h (pixeles de la pantalla) con esa opacidad (0..1)
void pantalla_sprite(int id, int x, int y, int w, int h, float alfa);

// donde va una imagen de iw x ih en una pantalla de pw x ph con esa escala (lo usan todas las implementaciones)
inline void pantalla_geometria(int escala, int iw, int ih, int pw, int ph, int* x, int* y, int* w, int* h) {
    int ww = iw, hh = ih;
    if (escala == ESCALA_ESTIRAR) {
        ww = pw;
        hh = ph;
    } else if (escala == ESCALA_AJUSTAR) {
        // (en enteros: lo mas grande que entra con la misma forma)
        if ((long)pw * ih <= (long)ph * iw) {
            ww = pw;
            hh = (int)((long)ih * pw / iw);
        } else {
            hh = ph;
            ww = (int)((long)iw * ph / ih);
        }
    } else if (escala == ESCALA_ENTERA) {
        int k = pw / iw < ph / ih ? pw / iw : ph / ih;
        if (k < 1) k = 1;
        ww = iw * k;
        hh = ih * k;
    }
    *w = ww;
    *h = hh;
    *x = (pw - ww) / 2;
    *y = (ph - hh) / 2;
}
