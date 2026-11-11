// La imagen de la consola en Symbian^3 / Anna / Belle (N8) (src/pantalla.h) con OpenGL ES 2.0: la imagen va a una
// textura RGB565 y se dibuja con la escala y el filtro de las opciones; los sprites son los controles tactiles. Un solo
// shader: la textura por un color (los rectangulos usan una textura blanca de 1x1).
//
// La pantalla del telefono y la de la TV (pantalla_dibujar_externa) comparten el contexto: la imagen se sube una vez y
// se dibuja en las dos.
#include "pantalla.h"
#include "w3dlog.h"
#include <GLES2/gl2.h>
#include <string.h>

namespace {
const int TEX_W = 512, TEX_H = 256;
GLuint gPrograma = 0, gTextura = 0, gBlanca = 0;
GLint gAPos = -1, gAUv = -1, gUEscala = -1, gUColor = -1, gUTex = -1;
int gAncho = 320, gAlto = 224;   // lo que ocupa la imagen en la textura
int gEscala = ESCALA_AJUSTAR, gSuave = 0, gSuavePuesto = -1;
int gImgX = 0, gImgY = 0, gImgW = 0, gImgH = 0;   // donde quedo la imagen en el telefono (para los dedos)
int gPw = 0, gPh = 0;                             // la pantalla donde se dibuja ahora

const int MAX_SPRITES = 16;
GLuint gSprites[MAX_SPRITES];
int gSpritesN = 0;

const char* const kVertices =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUv;\n"
    "uniform vec4 uEscala;\n"   // (x, y) -> clip: x * e.x + e.z, y * e.y + e.w
    "varying vec2 vUv;\n"
    "void main() {\n"
    "  vUv = aUv;\n"
    "  gl_Position = vec4(aPos.x * uEscala.x + uEscala.z, aPos.y * uEscala.y + uEscala.w, 0.0, 1.0);\n"
    "}\n";
const char* const kFragmentos =
    "precision mediump float;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec4 uColor;\n"
    "varying vec2 vUv;\n"
    "void main() { gl_FragColor = texture2D(uTex, vUv) * uColor; }\n";

GLuint shader(GLenum tipo, const char* fuente) {
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &fuente, 0);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256] = "";
        glGetShaderInfoLog(s, sizeof(log), 0, log);
        w3dLogf("pantalla: el shader no compila: %s", log);
    }
    return s;
}

// la pantalla de pw x ph: el viewport y la cuenta de pixeles a clip (y para abajo)
void preparar(int pw, int ph) {
    gPw = pw;
    gPh = ph;
    glViewport(0, 0, pw, ph);
    glUseProgram(gPrograma);
    glUniform4f(gUEscala, 2.0f / pw, -2.0f / ph, -1.0f, 1.0f);
    glUniform1i(gUTex, 0);
    glActiveTexture(GL_TEXTURE0);
}

void quad(GLuint tex, float x, float y, float w, float h, float u, float v, float r, float g, float b, float a) {
    const GLfloat pos[] = { x, y, x + w, y, x, y + h, x + w, y + h };
    const GLfloat uv[] = { 0, 0, u, 0, 0, v, u, v };
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform4f(gUColor, r, g, b, a);
    glVertexAttribPointer(gAPos, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer(gAUv, 2, GL_FLOAT, GL_FALSE, 0, uv);
    glEnableVertexAttribArray(gAPos);
    glEnableVertexAttribArray(gAUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// la imagen de la consola centrada con la escala y el filtro de las opciones
void dibujar_imagen(int pw, int ph, bool guardar) {
    preparar(pw, ph);
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    int x, y, w, h;
    pantalla_geometria(gEscala, gAncho, gAlto, pw, ph, &x, &y, &w, &h);
    if (guardar) {
        gImgX = x;
        gImgY = y;
        gImgW = w;
        gImgH = h;
    }
    glBindTexture(GL_TEXTURE_2D, gTextura);
    if (gSuave != gSuavePuesto) {
        gSuavePuesto = gSuave;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gSuave ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gSuave ? GL_LINEAR : GL_NEAREST);
    }
    quad(gTextura, (float)x, (float)y, (float)w, (float)h, (float)gAncho / TEX_W, (float)gAlto / TEX_H, 1, 1, 1, 1);
}
}

void pantalla_iniciar() {
    gPrograma = glCreateProgram();
    glAttachShader(gPrograma, shader(GL_VERTEX_SHADER, kVertices));
    glAttachShader(gPrograma, shader(GL_FRAGMENT_SHADER, kFragmentos));
    glLinkProgram(gPrograma);
    GLint ok = 0;
    glGetProgramiv(gPrograma, GL_LINK_STATUS, &ok);
    if (!ok) w3dLog("pantalla: el programa de shaders no linkea");
    gAPos = glGetAttribLocation(gPrograma, "aPos");
    gAUv = glGetAttribLocation(gPrograma, "aUv");
    gUEscala = glGetUniformLocation(gPrograma, "uEscala");
    gUColor = glGetUniformLocation(gPrograma, "uColor");
    gUTex = glGetUniformLocation(gPrograma, "uTex");

    glGenTextures(1, &gTextura);
    glBindTexture(GL_TEXTURE_2D, gTextura);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gSuavePuesto = 0;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_W, TEX_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);

    // (la textura blanca de 1x1: los rectangulos de color)
    const uint8_t blanco[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &gBlanca);
    glBindTexture(GL_TEXTURE_2D, gBlanca);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blanco);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    w3dLogf("pantalla: OpenGL ES 2.0 (%s, %s)", (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
}

void pantalla_terminar() {
    if (gTextura) glDeleteTextures(1, &gTextura);
    if (gBlanca) glDeleteTextures(1, &gBlanca);
    if (gSpritesN) glDeleteTextures(gSpritesN, gSprites);
    if (gPrograma) glDeleteProgram(gPrograma);
    gTextura = gBlanca = gPrograma = 0;
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

void pantalla_dibujar(int pw, int ph) { dibujar_imagen(pw, ph, true); }

void pantalla_dibujar_externa(int pw, int ph) { dibujar_imagen(pw, ph, false); }

void pantalla_limpiar(int pw, int ph) {
    preparar(pw, ph);
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
    if (pw != gPw || ph != gPh) preparar(pw, ph);
    const float a = (rgba & 255) / 255.0f;
    if (a < 1) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    quad(gBlanca, (float)x, (float)y, (float)w, (float)h, 1, 1, ((rgba >> 24) & 255) / 255.0f,
         ((rgba >> 16) & 255) / 255.0f, ((rgba >> 8) & 255) / 255.0f, a);
    glDisable(GL_BLEND);
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
    quad(gSprites[id], (float)x, (float)y, (float)w, (float)h, 1, 1, 1, 1, 1, alfa);
    glDisable(GL_BLEND);
}
