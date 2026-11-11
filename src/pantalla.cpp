// La imagen de la consola en la PC (src/pantalla.h) con OpenGL 1.x: una textura RGB565 y un rectangulo, con la escala y
// el filtro de las opciones, y los sprites de los controles tactiles (que en la PC se tocan con el mouse). En el N95 la
// imagen va por CPU (symbian/src/pantalla_symbian.cpp); en el N8, con OpenGL ES 2.0 (symbian/src/pantalla_gles2.cpp).
#include "pantalla.h"
#include <SDL2/SDL_opengl.h>

namespace {
const int TEX_W = 512, TEX_H = 256;
GLuint gTextura = 0;
int gAncho = 320, gAlto = 224;   // lo que ocupa la imagen en la textura
int gEscala = ESCALA_ENTERA, gSuave = 0;
int gImgX = 0, gImgY = 0, gImgW = 0, gImgH = 0;   // donde quedo la imagen en el ultimo dibujo

const int MAX_SPRITES = 16;
GLuint gSprites[MAX_SPRITES];
int gSpritesN = 0;

void quad(float x, float y, float w, float h, float u, float v) {
    const GLfloat verts[] = { x, y, x + w, y, x, y + h, x + w, y + h };
    const GLfloat uvs[] = { 0, 0, u, 0, 0, v, u, v };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
}

void pantalla_iniciar() {
    glGenTextures(1, &gTextura);
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_W, TEX_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glClearColor(0, 0, 0, 1);
}

void pantalla_terminar() {
    if (gTextura) glDeleteTextures(1, &gTextura);
    gTextura = 0;
    if (gSpritesN) glDeleteTextures(gSpritesN, gSprites);
    gSpritesN = 0;
}

void pantalla_modo(int escala, int suave) {
    gEscala = escala;
    gSuave = suave;
}

void pantalla_subir(const uint16_t* imagen, int ancho, int alto) {
    if (!imagen || ancho <= 0 || alto <= 0) return;
    if (ancho > TEX_W) ancho = TEX_W;
    if (alto > TEX_H) alto = TEX_H;
    gAncho = ancho;
    gAlto = alto;
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ancho, alto, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, imagen);
}

void pantalla_dibujar(int pw, int ph) {
    glViewport(0, 0, pw, ph);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, (double)pw, (double)ph, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    pantalla_geometria(gEscala, gAncho, gAlto, pw, ph, &gImgX, &gImgY, &gImgW, &gImgH);

    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gSuave ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gSuave ? GL_LINEAR : GL_NEAREST);
    glColor4f(1, 1, 1, 1);
    quad((float)gImgX, (float)gImgY, (float)gImgW, (float)gImgH, (float)gAncho / TEX_W, (float)gAlto / TEX_H);
    glDisable(GL_TEXTURE_2D);
}

void pantalla_dibujar_externa(int pw, int ph) {
    // (la PC no tiene pantalla externa: lo mismo, sin tocar donde quedo la imagen)
    const int x = gImgX, y = gImgY, w = gImgW, h = gImgH;
    pantalla_dibujar(pw, ph);
    gImgX = x;
    gImgY = y;
    gImgW = w;
    gImgH = h;
}

void pantalla_limpiar(int pw, int ph) {
    glViewport(0, 0, pw, ph);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool pantalla_a_imagen(int sx, int sy, int* ix, int* iy) {
    if (gImgW <= 0 || gImgH <= 0) return false;
    *ix = (sx - gImgX) * gAncho / gImgW;
    *iy = (sy - gImgY) * gAlto / gImgH;
    return *ix >= 0 && *iy >= 0 && *ix < gAncho && *iy < gAlto;
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

int pantalla_sprite_crear(const uint32_t* rgba, int w, int h) {
    if (gSpritesN >= MAX_SPRITES) return -1;
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    gSprites[gSpritesN] = t;
    return gSpritesN++;
}

void pantalla_sprite(int id, int x, int y, int w, int h, float alfa) {
    if (id < 0 || id >= gSpritesN) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gSprites[id]);
    glColor4f(1, 1, 1, alfa);
    quad((float)x, (float)y, (float)w, (float)h, 1.0f, 1.0f);
    glColor4f(1, 1, 1, 1);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}
