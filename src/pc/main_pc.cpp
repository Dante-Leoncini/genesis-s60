// ---------------------------------------------------------------------------------------------------------------
//  La prueba en la PC: el mismo emulador que en el N95, en una ventana de SDL con OpenGL 1.x (el pipeline fijo: el
//  mismo camino que el GLES 1.1 del telefono) y el teclado haciendo de las teclas del telefono.
//
//      build/segaw3d_pc [--datos carpeta/] [--tam 640x480] [--capturas c1,c2,...] [--salir-en cuadro]
//                       [--teclas cuadro:tecla+tecla,...] [--tactil] [--toques ms:x:y:dur,...]
//
//  Teclas: flechas = la cruceta; Z X C = A B C; Enter = Start; A S D = X Y Z; Tab = Mode; Borrar = la C del
//  telefono (el menu); Re Pag / Av Pag (o + / - del teclado numerico) = el volumen; Escape sale. Los
//  joysticks: los que SDL reconoce como mando, el primero es el jugador 1 (junto con el teclado). --capturas guarda
//  esos cuadros en build/captura_<cuadro>.ppm; --teclas: los botones que se aprietan solos desde cada cuadro de la
//  consola (ver juego_guion en juego.h: "600:start,610:nada,3350:abajo+j3").
//  --tactil: como una pantalla tactil (el N8): el mouse es un dedo (el boton izquierdo apoyado), el menu de filas
//  grandes y los controles en la pantalla. --toques: dedos solos para las pruebas, a los ms desde que arranca,
//  en (x, y) de la ventana, durante dur ms (varios a la vez valen: son dedos distintos).
// ---------------------------------------------------------------------------------------------------------------
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "../plataforma.h"
#include "../juego.h"
#include "../tactil.h"
#include "../emu.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {
SDL_Window* gVentana = 0;
bool gTeclas[TECLAS];
bool gSalir = false;
char gDatos[512] = "datos/";
Uint64 gFrecuencia = 1;
SDL_GameController* gMandos[w3dEngine::MandosMax];
bool gTactil = false;
const char* gToques = "";
uint64_t gInicioUs = 0;

int tecla_de(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP: return TK_ARRIBA;
    case SDLK_DOWN: return TK_ABAJO;
    case SDLK_LEFT: return TK_IZQ;
    case SDLK_RIGHT: return TK_DER;
    case SDLK_z: return TK_1;
    case SDLK_x: return TK_2;
    case SDLK_c: return TK_3;
    case SDLK_RETURN: case SDLK_KP_ENTER: return TK_0;
    case SDLK_a: return TK_4;
    case SDLK_s: return TK_5;
    case SDLK_d: return TK_6;
    case SDLK_TAB: return TK_ASTERISCO;
    case SDLK_BACKSPACE: return TK_BORRAR;
    default: return -1;
    }
}

void capturar(int cuadro) {
    int w, h;
    SDL_GL_GetDrawableSize(gVentana, &w, &h);
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    char ruta[128];
    snprintf(ruta, sizeof(ruta), "build/captura_%d.ppm", cuadro);
    FILE* f = fopen(ruta, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) fwrite(px + (size_t)y * w * 3, 1, (size_t)w * 3, f);
        fclose(f);
    }
    free(px);
}
}

uint64_t plat_ahora_us() { return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e6 / (double)gFrecuencia); }
const char* plat_carpeta_datos() { return gDatos; }
void plat_pantalla(int& ancho, int& alto) { SDL_GL_GetDrawableSize(gVentana, &ancho, &alto); }
bool plat_tecla(int t) { return t >= 0 && t < TECLAS && gTeclas[t]; }
void plat_salir() { gSalir = true; }

bool plat_es_tactil() { return gTactil; }
bool plat_escala() { return true; }

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned short __stdcall GetUserDefaultUILanguage(void);
#endif
bool plat_espanol() {
    const char* lang = getenv("LANG");
    if (lang && lang[0]) return !strncmp(lang, "es", 2);
#ifdef _WIN32
    return (GetUserDefaultUILanguage() & 0x3FF) == 0x0A;   // (LANG_SPANISH)
#else
    return false;
#endif
}

// el mouse (el boton izquierdo) es el dedo 0; los de --toques, los dedos 100 en adelante
int plat_toques(Toque* t, int max) {
    int n = 0;
    if (!gTactil) return 0;
    int x = 0, y = 0;
    if ((SDL_GetMouseState(&x, &y) & SDL_BUTTON(SDL_BUTTON_LEFT)) && n < max) {
        t[n].id = 0;
        t[n].x = x;
        t[n].y = y;
        n++;
    }
    const int ms = (int)((plat_ahora_us() - gInicioUs) / 1000);
    const char* p = gToques;
    for (int k = 0; *p && n < max; k++) {
        int desde = 0, tx = 0, ty = 0, dur = 0;
        if (sscanf(p, "%d:%d:%d:%d", &desde, &tx, &ty, &dur) == 4 && ms >= desde && ms < desde + dur) {
            t[n].id = 100 + k;
            t[n].x = tx;
            t[n].y = ty;
            n++;
        }
        const char* coma = strchr(p, ',');
        if (!coma) break;
        p = coma + 1;
    }
    return n;
}

bool plat_mando_acelerometro(int, float*) { return false; }   // (en la PC no hay Wiimote)

void plat_fecha(int f[6]) {
    const time_t t = time(0);
    const struct tm* l = localtime(&t);
    f[0] = l->tm_year + 1900;
    f[1] = l->tm_mon + 1;
    f[2] = l->tm_mday;
    f[3] = l->tm_hour;
    f[4] = l->tm_min;
    f[5] = l->tm_sec;
}

void plat_crear_carpeta(const char* ruta) {
    char r[512];
    snprintf(r, sizeof(r), "%s", ruta);
    size_t n = strlen(r);
    while (n > 1 && (r[n - 1] == '/' || r[n - 1] == '\\')) r[--n] = 0;   // (mkdir de Windows no quiere la barra)
#ifdef _WIN32
    _mkdir(r);
#else
    mkdir(r, 0755);
#endif
}

bool plat_mando(int i, w3dEngine::MandoEstado& e) {
    using namespace w3dEngine;
    if (i < 0 || i >= MandosMax) return false;
    if (gMandos[i] && !SDL_GameControllerGetAttached(gMandos[i])) {
        SDL_GameControllerClose(gMandos[i]);
        gMandos[i] = 0;
    }
    if (!gMandos[i] && i < SDL_NumJoysticks() && SDL_IsGameController(i)) gMandos[i] = SDL_GameControllerOpen(i);
    if (!gMandos[i]) return false;
    e.tipo = MandoGenerico;
    for (int k = 0; k < MandoEjes; k++)
        e.ejes[k] = SDL_GameControllerGetAxis(gMandos[i], (SDL_GameControllerAxis)k) / 32767.0f;
    for (int k = 0; k < MandoBotones; k++)
        e.botones[k] = SDL_GameControllerGetButton(gMandos[i], (SDL_GameControllerButton)k) != 0;
    return true;
}

int main(int argc, char** argv) {
    int w = 640, h = 480, salir_en = 0;
    const char* capturas = "";
    const char* guion = "";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--datos") && i + 1 < argc) snprintf(gDatos, sizeof(gDatos), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--tam") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--capturas") && i + 1 < argc) capturas = argv[++i];
        else if (!strcmp(argv[i], "--salir-en") && i + 1 < argc) salir_en = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--teclas") && i + 1 < argc) guion = argv[++i];
        else if (!strcmp(argv[i], "--tactil")) gTactil = true;
        else if (!strcmp(argv[i], "--pcm") && i + 1 < argc) emu_sonido_grabar(argv[++i]);
        else if (!strcmp(argv[i], "--toques") && i + 1 < argc) {
            gToques = argv[++i];
            gTactil = true;
        }
    }
    w3dLogRuta("build/segaw3d.log");
    w3dLogReset();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    gFrecuencia = SDL_GetPerformanceFrequency();
    gInicioUs = plat_ahora_us();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    gVentana = SDL_CreateWindow("segaw3d (prueba en la PC)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!gVentana || !SDL_GL_CreateContext(gVentana)) {
        fprintf(stderr, "GL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    juego_iniciar(44100);
    if (*guion) juego_guion(guion);
    int cuadro = 0;
    while (!gSalir) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) gSalir = true;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) gSalir = true;
            else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_KP_PLUS || e.key.keysym.sym == SDLK_PAGEUP)) juego_volumen(1);
            else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_KP_MINUS || e.key.keysym.sym == SDLK_PAGEDOWN)) juego_volumen(-1);
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                const int t = tecla_de(e.key.keysym.sym);
                if (t >= 0) gTeclas[t] = e.type == SDL_KEYDOWN;
            }
        }
        if (!juego_cuadro()) {
            SDL_Delay(1);
            continue;
        }
        cuadro++;
        char clave[16];
        snprintf(clave, sizeof(clave), "%d", cuadro);
        const char* p = capturas;
        while ((p = strstr(p, clave)) != 0) {
            const char fin = p[strlen(clave)];
            if ((p == capturas || p[-1] == ',') && (fin == 0 || fin == ',')) {
                capturar(cuadro);
                break;
            }
            p += strlen(clave);
        }
        const uint64_t antesSwap = plat_ahora_us();
        SDL_GL_SwapWindow(gVentana);
        juego_swap_us(plat_ahora_us() - antesSwap);
        if (salir_en > 0 && cuadro >= salir_en) gSalir = true;
    }
    juego_terminar();
    SDL_Quit();
    return 0;
}
