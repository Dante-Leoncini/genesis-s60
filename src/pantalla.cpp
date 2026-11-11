// La imagen de la consola en la PC (src/pantalla.h) con OpenGL 1.x: una textura RGB565 y un rectangulo, PIXEL PERFECT:
// escala entera, sin filtro y sin estirar (un juego de 256 columnas queda de 256, centrado). En el N95 la imagen va
// por CPU (symbian/src/pantalla_symbian.cpp): subir una textura al MBX cuesta 36 ms por cuadro.
#include "pantalla.h"
#include <SDL2/SDL_opengl.h>

namespace {
const int TEX_W = 512, TEX_H = 256;
GLuint gTextura = 0;
int gAncho = 320, gAlto = 224;   // lo que ocupa la imagen en la textura
}

void pantalla_iniciar() {
    glGenTextures(1, &gTextura);
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_W, TEX_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glClearColor(0, 0, 0, 1);
}

void pantalla_terminar() {
    if (gTextura) glDeleteTextures(1, &gTextura);
    gTextura = 0;
}

void pantalla_subir(const uint16_t* imagen, int ancho, int alto) {
    if (!imagen || ancho <= 0 || alto <= 0) return;
    if (ancho > TEX_W) ancho = TEX_W;
    if (alto > TEX_H) alto = TEX_H;
    gAncho = ancho;
    gAlto = alto;
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ancho, alto, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, imagen);
}

void pantalla_dibujar(int pw, int ph) {
    glViewport(0, 0, pw, ph);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, (double)pw, (double)ph, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // la escala entera mas grande que entra (en el N95 acostado, 1)
    int escala = pw / 320 < ph / gAlto ? pw / 320 : ph / gAlto;
    if (escala < 1) escala = 1;
    const int w = gAncho * escala, h = gAlto * escala;
    const float x = (float)((pw - w) / 2), y = (float)((ph - h) / 2);

    glClear(GL_COLOR_BUFFER_BIT);
    const float u = (float)gAncho / TEX_W, v = (float)gAlto / TEX_H;
    const GLfloat verts[] = { x, y, x + w, y, x, y + h, x + w, y + h };
    const GLfloat uvs[] = { 0, 0, u, 0, 0, v, u, v };
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glColor4f(1, 1, 1, 1);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
}

void pantalla_rectangulo(int pw, int ph, int x, int y, int w, int h, uint32_t rgba) {
    (void)pw;
    (void)ph;
    const GLfloat x0 = (GLfloat)x, y0 = (GLfloat)y, x1 = (GLfloat)(x + w), y1 = (GLfloat)(y + h);
    const GLfloat verts[] = { x0, y0, x1, y0, x0, y1, x1, y1 };
    glColor4f(((rgba >> 24) & 255) / 255.0f, ((rgba >> 16) & 255) / 255.0f, ((rgba >> 8) & 255) / 255.0f,
              (rgba & 255) / 255.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glColor4f(1, 1, 1, 1);
}
