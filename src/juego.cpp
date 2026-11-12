// La consola al reloj, los jugadores, el menu de las ROMs y la pantalla (juego.h).
//
// Arranca en el MENU con las ROMs de la carpeta (la ultima jugada queda elegida). Jugando, la tecla C del telefono o
// Select (o PS) de cualquier mando vuelven al menu con la partida en pausa (C / Select / circulo vuelven a ella).
//
// datos/config.txt (opcional), una opcion por linea:
//     carpeta=e:/data/sega/             donde estan las ROMs (si no, la carpeta de datos)
//     rom=e:/data/sega/juego.zip        cargar esa ROM sin pasar por el menu (las pruebas)
//     entrada=auto                      auto (el Team Player si el cartucho lo soporta; si no, dos mandos), team1 /
//                                       team2 (el Team Player en el puerto 1 o 2), ea (4 Way Play), dos
//     perfil=1                          el perfil al log cada 2 segundos
// datos/ultima.txt: la ultima ROM jugada (la escribe el emulador)
// datos/opciones.txt: las opciones del menu (Start): fps=0/1, multitap=auto/siempre/nunca, filtro=0/1, fm_alta=0/1,
//     hz=22050/32000/44100, rapido=0/1 (el dibujo rapido de PicoDrive), escala=0..3 (ESCALA_*), suave=0/1,
//     tactil=0/1/2 (auto / si / no: los controles en la pantalla)
// datos/prueba.txt (las pruebas, opcional):
//     teclas=600:start,610:nada,...     botones apretados solos desde cada cuadro de la consola (ver juego_guion)
//     salir=90                          salir a los 90 segundos
#include "juego.h"
#include "emu.h"
#include "menu.h"
#include "pantalla.h"
#include "plataforma.h"
#include "tactil.h"
#include "W3dAudio.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace w3dEngine;

namespace {
bool gCargada = false;
bool gEnMenu = false;
bool gPausa = false;
uint64_t gReloj = 0;   // cuando toca el proximo cuadro de la consola (us)
bool gRelojPuesto = false;
uint32_t gImagenSubida = 0xffffffff;
bool gRedibujar = false;   // juego_redibujar: el proximo cuadro se presenta aunque no toque
uint16_t gMenuImagen[MENU_ANCHO * MENU_ALTO];

// el volumen, de 0 a 10, y la barrita que lo muestra un rato al cambiarlo
int gVolumen = 10;
uint64_t gVolumenHasta = 0;

// los jugadores conectados y el aviso (los cuadraditos de colores arriba, unos segundos)
int gConectados = 0;   // los lugares del driver con un mando (bits)
int gJugadores = 0;    // los jugadores con mando, en orden (bits 0..n-1)
uint64_t gAvisoHasta = 0;
bool gGuia = false, gGuiaAntes = false;   // Select o PS de algun mando (vuelven al menu)
bool gBorrarAntes = false;

// el guion de las pruebas: los botones que se aprietan solos, por cuadro de la consola
char gGuion[2048] = "";
uint32_t gCuadroConsola = 0;
uint16_t gGuionPad = 0;
int gGuionJugador = 0;
uint64_t gSalirEn = 0;

// el perfil
bool gPerfil = false;
uint64_t gPerfilDesde = 0, gEmuUs = 0, gSubidaUs = 0, gDibujoUs = 0, gSwapUs = 0;
// la emulacion de los cuadros que se dibujan y de los que no: la diferencia es lo que cuesta dibujar los graficos
uint64_t gEmuConDibujoUs = 0, gEmuSinDibujoUs = 0;
int gEmuConDibujo = 0, gEmuSinDibujo = 0;
bool gSoftDerAntes = true;   // (la tecla de abajo a la derecha: cuenta al apretarla, no mientras sigue apretada)
int gPresentados = 0, gEmulados = 0, gSaltados = 0;

// --------------------------------------------------------------------------------- la configuracion y la ROM
char gRom[512] = "";
char gCarpeta[512] = "";
int gEntrada = -1;   // -1 = segun la opcion de multitap
MenuOpciones gOpc = { 0, MULTITAP_AUTO, 0, 0, 0, 0, ESCALA_AJUSTAR, 0, TACTIL_AUTO, IDIOMA_AUTO, 0 };
// la pantalla tactil: los controles en pantalla se pueden usar, y el dedo que maneja el menu
bool gTactilListo = false;
int gDedoMenu = -1, gMenuDedoX = 0, gMenuDedoY = 0;
// al pasar del juego al menu (o al reves) los dedos que siguen apoyados no cuentan hasta levantarlos todos: el que
// toco el boton del menu caeria en OPCIONES, y el que toco VOLVER, en un control
bool gEsperarSoltar = false;
// a donde va la imagen (juego_salida: la TV del N8)
int gSalida = SALIDA_TELEFONO;

// true mientras haya que esperar que se levanten los dedos
bool esperando_soltar() {
    if (!gEsperarSoltar) return false;
    Toque t[10];
    if (plat_toques(t, 10) == 0) gEsperarSoltar = false;
    return gEsperarSoltar;
}
// las frecuencias del sonido que se pueden elegir (MenuOpciones::hz)
const int HZ_SONIDO[3] = { 22050, 32000, 44100 };
// el contador de FPS: los cuadros presentados en el ultimo segundo
int gFpsCuenta = 0, gFpsMostrado = 0;
uint64_t gFpsDesde = 0;

void sin_fin_de_linea(char* linea) {
    size_t n = strlen(linea);
    while (n > 0 && (linea[n - 1] == '\n' || linea[n - 1] == '\r' || linea[n - 1] == ' ')) linea[--n] = 0;
}

void leer_config() {
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%sconfig.txt", plat_carpeta_datos());
    FILE* f = fopen(ruta, "r");
    if (!f) return;
    char linea[512];
    while (fgets(linea, sizeof(linea), f)) {
        sin_fin_de_linea(linea);
        if (!strncmp(linea, "rom=", 4)) {
            strncpy(gRom, linea + 4, sizeof(gRom) - 1);
        } else if (!strncmp(linea, "carpeta=", 8)) {
            strncpy(gCarpeta, linea + 8, sizeof(gCarpeta) - 2);
            const size_t n = strlen(gCarpeta);
            if (n && gCarpeta[n - 1] != '/' && gCarpeta[n - 1] != '\\') strcat(gCarpeta, "/");
        } else if (!strncmp(linea, "entrada=", 8)) {
            const char* v = linea + 8;
            if (!strcmp(v, "team2")) gEntrada = EMU_TEAM_PLAYER_2;
            else if (!strcmp(v, "ea")) gEntrada = EMU_EA_4WAY;
            else if (!strcmp(v, "dos")) gEntrada = EMU_DOS_MANDOS;
            else if (!strcmp(v, "team1")) gEntrada = EMU_TEAM_PLAYER_1;
            else gEntrada = -1;
        } else if (!strncmp(linea, "perfil=", 7)) {
            gPerfil = atoi(linea + 7) != 0;
        }
    }
    fclose(f);
    if (!gCarpeta[0]) strncpy(gCarpeta, plat_carpeta_datos(), sizeof(gCarpeta) - 1);
}

// la ultima ROM jugada (datos/ultima.txt)
void ultima(char* ruta, size_t largo, bool escribir) {
    char archivo[512];
    snprintf(archivo, sizeof(archivo), "%sultima.txt", plat_carpeta_datos());
    FILE* f = fopen(archivo, escribir ? "w" : "r");
    if (!f) {
        if (!escribir) ruta[0] = 0;
        return;
    }
    if (escribir) {
        fprintf(f, "%s\n", ruta);
    } else {
        if (!fgets(ruta, (int)largo, f)) ruta[0] = 0;
        sin_fin_de_linea(ruta);
    }
    fclose(f);
}

void opciones(bool escribir) {
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%sopciones.txt", plat_carpeta_datos());
    FILE* f = fopen(ruta, escribir ? "w" : "r");
    if (!f) return;
    if (escribir) {
        fprintf(f, "fps=%d\nmultitap=%s\nfiltro=%d\nfm_alta=%d\nhz=%d\nrapido=%d\nescala=%d\nsuave=%d\ntactil=%d\nidioma=%s\ninclinacion=%d\n",
                gOpc.fps,
                gOpc.multitap == MULTITAP_SIEMPRE ? "siempre" : gOpc.multitap == MULTITAP_NUNCA ? "nunca" : "auto",
                gOpc.filtro, gOpc.fmAlta, HZ_SONIDO[gOpc.hz], gOpc.rapido, gOpc.escala, gOpc.suave, gOpc.tactil,
                gOpc.idioma == IDIOMA_ES ? "es" : gOpc.idioma == IDIOMA_EN ? "en" : "auto", gOpc.inclinacion);
    } else {
        char linea[128];
        bool conHz = false;
        while (fgets(linea, sizeof(linea), f)) {
            sin_fin_de_linea(linea);
            if (!strncmp(linea, "fps=", 4)) gOpc.fps = atoi(linea + 4) != 0;
            else if (!strcmp(linea, "multitap=siempre")) gOpc.multitap = MULTITAP_SIEMPRE;
            else if (!strcmp(linea, "multitap=nunca")) gOpc.multitap = MULTITAP_NUNCA;
            else if (!strcmp(linea, "multitap=auto")) gOpc.multitap = MULTITAP_AUTO;
            else if (!strncmp(linea, "filtro=", 7)) gOpc.filtro = atoi(linea + 7) != 0;
            else if (!strncmp(linea, "fm_alta=", 8)) gOpc.fmAlta = atoi(linea + 8) != 0;
            else if (!strncmp(linea, "rapido=", 7)) gOpc.rapido = atoi(linea + 7) != 0;
            else if (!strncmp(linea, "escala=", 7)) gOpc.escala = atoi(linea + 7) % ESCALAS;
            else if (!strncmp(linea, "suave=", 6)) gOpc.suave = atoi(linea + 6) != 0;
            else if (!strncmp(linea, "tactil=", 7)) gOpc.tactil = atoi(linea + 7) % 3;
            else if (!strcmp(linea, "idioma=es")) gOpc.idioma = IDIOMA_ES;
            else if (!strcmp(linea, "idioma=en")) gOpc.idioma = IDIOMA_EN;
            else if (!strcmp(linea, "idioma=auto")) gOpc.idioma = IDIOMA_AUTO;
            else if (!strncmp(linea, "inclinacion=", 12)) gOpc.inclinacion = atoi(linea + 12) != 0;
            else if (!strncmp(linea, "hz=", 3)) {
                const int hz = atoi(linea + 3);
                gOpc.hz = hz >= 44100 ? 2 : hz >= 32000 ? 1 : 0;
                conHz = true;
            }
        }
        // (un archivo de antes de la opcion de frecuencia trae el filtro prendido, que era el defecto: apagaba los
        // agudos, "sonido de radio". Se apaga)
        if (!conHz) gOpc.filtro = 0;
    }
    fclose(f);
}

// como se enchufan los mandos en el juego cargado: config.txt (entrada=) manda; si no, la opcion de multitap
int entrada_del_juego() {
    if (gEntrada >= 0) return gEntrada;
    if (gOpc.multitap == MULTITAP_SIEMPRE) return EMU_TEAM_PLAYER_1;
    if (gOpc.multitap == MULTITAP_NUNCA) return EMU_DOS_MANDOS;
    return emu_soporta_team_player() ? EMU_TEAM_PLAYER_1 : EMU_DOS_MANDOS;
}

void leer_prueba() {
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%sprueba.txt", plat_carpeta_datos());
    FILE* f = fopen(ruta, "r");
    if (!f) return;
    char linea[2048];
    while (fgets(linea, sizeof(linea), f)) {
        sin_fin_de_linea(linea);
        if (!strncmp(linea, "teclas=", 7)) juego_guion(linea + 7);
        else if (!strncmp(linea, "salir=", 6) && atoi(linea + 6) > 0)
            gSalirEn = plat_ahora_us() + (uint64_t)atoi(linea + 6) * 1000000;
    }
    fclose(f);
    if (gGuion[0] || gSalirEn) w3dLogf("prueba: guion de %d letras, salir a los %d s", (int)strlen(gGuion),
                                       gSalirEn ? (int)((gSalirEn - plat_ahora_us()) / 1000000) : 0);
}

// lo que el guion aprieta desde el cuadro 'c' de la consola (si hay algo para ese cuadro)
void guion_cuadro(uint32_t c) {
    const char* p = gGuion;
    while (*p) {
        const uint32_t k = (uint32_t)atoi(p);
        const char* dos = strchr(p, ':');
        if (!dos) return;
        const char* fin = strchr(dos, ',');
        if (!fin) fin = dos + strlen(dos);
        if (k == c) {
            static const struct { const char* n; uint16_t b; } kNombres[] = {
                { "arriba", MD_ARRIBA }, { "abajo", MD_ABAJO }, { "izq", MD_IZQ }, { "der", MD_DER },
                { "start", MD_START }, { "a", MD_A }, { "b", MD_B }, { "c", MD_C },
                { "x", MD_X }, { "y", MD_Y }, { "z", MD_Z }, { "mode", MD_MODE } };
            gGuionPad = 0;
            for (const char* q = dos + 1; q < fin;) {
                const char* fp = q;
                while (fp < fin && *fp != '+') fp++;
                const size_t largo = (size_t)(fp - q);
                if (largo == 2 && q[0] == 'j' && q[1] >= '1' && q[1] <= '4') gGuionJugador = q[1] - '1';
                for (unsigned i = 0; i < sizeof(kNombres) / sizeof(kNombres[0]); i++)
                    if (largo == strlen(kNombres[i].n) && !strncmp(q, kNombres[i].n, largo)) gGuionPad |= kNombres[i].b;
                q = fp + 1;
            }
        }
        p = *fin ? fin + 1 : fin;
    }
}

void sonido_fuente(int* acc, int frames, void*) { emu_sonido_mezclar(acc, frames); }

// --------------------------------------------------------------------------------- los mandos
// el mando de PS3/PS4 -> el de 6 botones de Mega Drive: cuadrado/cruz/circulo = A/B/C (la fila de abajo),
// L1/triangulo/R1 = X/Y/Z (la de arriba), Start = Start, L2 = Mode; la cruceta o el stick izquierdo. Select y PS
// no van a la consola: vuelven al menu
uint16_t pad_de_mando(const MandoEstado& e) {
    uint16_t p = 0;
    if (e.botones[MandoArriba] || e.ejes[MandoLY] < -0.5f) p |= MD_ARRIBA;
    if (e.botones[MandoAbajo] || e.ejes[MandoLY] > 0.5f) p |= MD_ABAJO;
    if (e.botones[MandoIzquierda] || e.ejes[MandoLX] < -0.5f) p |= MD_IZQ;
    if (e.botones[MandoDerecha] || e.ejes[MandoLX] > 0.5f) p |= MD_DER;
    if (e.botones[MandoX]) p |= MD_A;
    if (e.botones[MandoA]) p |= MD_B;
    if (e.botones[MandoB]) p |= MD_C;
    if (e.botones[MandoL1]) p |= MD_X;
    if (e.botones[MandoY]) p |= MD_Y;
    if (e.botones[MandoR1]) p |= MD_Z;
    if (e.botones[MandoStart]) p |= MD_START;
    if (e.ejes[MandoLT] > 0.5f) p |= MD_MODE;
    return p;
}

// las teclas del telefono -> el mando 1: la cruceta; OK y el 1 = A; 2 = B; 3 = C; 4/5/6 = X/Y/Z; 7 = Mode; la
// tecla de abajo a la izquierda (o el 0) = Start. La de abajo a la derecha va al menu (y del menu vuelve)
uint16_t pad_de_teclas() {
    uint16_t p = 0;
    if (plat_tecla(TK_ARRIBA)) p |= MD_ARRIBA;
    if (plat_tecla(TK_ABAJO)) p |= MD_ABAJO;
    if (plat_tecla(TK_IZQ)) p |= MD_IZQ;
    if (plat_tecla(TK_DER)) p |= MD_DER;
    if (plat_tecla(TK_CENTRO) || plat_tecla(TK_1)) p |= MD_A;
    if (plat_tecla(TK_2)) p |= MD_B;
    if (plat_tecla(TK_3)) p |= MD_C;
    if (plat_tecla(TK_4)) p |= MD_X;
    if (plat_tecla(TK_5)) p |= MD_Y;
    if (plat_tecla(TK_6)) p |= MD_Z;
    if (plat_tecla(TK_7)) p |= MD_MODE;
    if (plat_tecla(TK_SOFT_IZQ) || plat_tecla(TK_0)) p |= MD_START;
    return p;
}

// LOS MANDOS SE LEEN DE A POCO: en el N95 cada lectura es un pedido al servidor del driver Bluetooth, y con 4 por
// cuadro (240 por segundo) el servidor no llegaba a atender a un mando que se prende (no se conectaba con el
// emulador abierto). Los conectados se leen siempre; de los que faltan, uno cada 15 cuadros (aparece en menos de 1 s)
int gRevisar = 0, gCuadrosPads = 0;

// EL WIIMOTE COMO VOLANTE (la opcion de inclinarlo): acostado, como un volante. Derecho no aprieta nada; inclinado
// aprieta izquierda o derecha una parte de los cuadros (mas inclinado, mas seguido) y bien inclinado, siempre: como
// un volante en un juego que solo tiene cruceta. La inclinacion es Y del acelerometro (a lo largo del control): con
// la cruceta a la izquierda, girar a la derecha da Y negativo (probado en el N8)
const float VOLANTE_MUERTO = 0.12f;   // (~7 grados: derecho)
const float VOLANTE_TOPE = 0.55f;     // (~33 grados: todo el tiempo)
float gVolante[4] = { 0, 0, 0, 0 };   // lo acumulado de cada mando (sigma-delta: aprieta al pasar de 1)
int gVolanteLog = 0;

uint16_t volante(int i) {
    float g[3];
    if (!plat_mando_acelerometro(i, g)) return 0;
    const float y = g[1];
    const float a = y < 0 ? -y : y;
    if (++gVolanteLog >= 120) {
        gVolanteLog = 0;
        w3dLogf("juego: wiimote %d inclinado %.2f (x %.2f z %.2f)", i + 1, y, g[0], g[2]);
    }
    if (a < VOLANTE_MUERTO) {
        gVolante[i] = 0;
        return 0;
    }
    float d = (a - VOLANTE_MUERTO) / (VOLANTE_TOPE - VOLANTE_MUERTO);
    if (d > 1.0f) d = 1.0f;
    gVolante[i] += d;
    if (gVolante[i] < 1.0f) return 0;
    gVolante[i] -= 1.0f;
    return y > 0 ? MD_IZQ : MD_DER;
}

void leer_pads(uint16_t pads[4]) {
    int conectados = 0;
    gGuia = false;
    int revisar = -1;
    if (++gCuadrosPads >= 15) {
        gCuadrosPads = 0;
        for (int k = 0; k < 4; k++) {
            gRevisar = (gRevisar + 1) % 4;
            if (!(gConectados & (1 << gRevisar))) {
                revisar = gRevisar;
                break;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        pads[i] = 0;
        if (!(gConectados & (1 << i)) && i != revisar) continue;
        MandoEstado e;
        if (plat_mando(i, e)) {
            pads[i] = pad_de_mando(e);
            // (el volante se suma a la cruceta: inclinado a la derecha + abajo = la diagonal. Si la cruceta aprieta
            // izquierda o derecha, mandan sus costados: nunca izquierda y derecha juntas)
            if (gOpc.inclinacion && !gEnMenu && e.tipo == MandoWii) {
                const uint16_t giro = volante(i);
                if (!(pads[i] & (MD_IZQ | MD_DER))) pads[i] |= giro;
            }
            conectados |= 1 << i;
            if (e.botones[MandoGuia] || e.botones[MandoSelect]) gGuia = true;
        }
    }
    // los jugadores en el orden de los mandos conectados: el primero es el 1 aunque el driver lo tenga en otro lugar
    int j = 0;
    for (int i = 0; i < 4; i++)
        if (conectados & (1 << i)) pads[j++] = pads[i];
    for (; j < 4; j++) pads[j] = 0;
    pads[0] |= pad_de_teclas();
    if (conectados != gConectados) {
        w3dLogf("juego: mandos conectados 0x%x", conectados);
        gConectados = conectados;
        int n = 0;
        for (int i = 0; i < 4; i++) n += (conectados >> i) & 1;
        gJugadores = (1 << n) - 1;
        gAvisoHasta = plat_ahora_us() + 3000000;
    }
}

// el aviso de los jugadores: un cuadradito por mando, con el color del Bomberman de cada uno
void dibujar_aviso(int pw, int ph) {
    if (plat_ahora_us() > gAvisoHasta) return;
    static const uint32_t colores[4] = { 0xffffffffu, 0x202020ffu, 0xe02020ffu, 0x2040e0ffu };
    const int lado = 10, sep = 4;
    int x = sep;
    for (int i = 0; i < 4; i++) {
        if (!(gJugadores & (1 << i))) continue;
        pantalla_rectangulo(pw, ph, x - 1, sep - 1, lado + 2, lado + 2, 0x000000ffu);
        pantalla_rectangulo(pw, ph, x, sep, lado, lado, colores[i]);
        x += lado + sep;
    }
}

// la barrita del volumen, arriba al centro: 10 rayas, las prendidas en verde
void dibujar_volumen(int pw, int ph) {
    if (plat_ahora_us() > gVolumenHasta) return;
    const int ancho = 10 * 8, x0 = (pw - ancho) / 2, y0 = 6;
    pantalla_rectangulo(pw, ph, x0 - 3, y0 - 3, ancho + 4, 16, 0x000000ffu);
    for (int i = 0; i < 10; i++)
        pantalla_rectangulo(pw, ph, x0 + i * 8, y0 + 8 - i, 6, 2 + i, i < gVolumen ? 0x40e040ffu : 0x404040ffu);
}

void perfil_cuadro(uint64_t ahora) {
    if (!gPerfil) return;
    if (!gPerfilDesde) gPerfilDesde = ahora;
    const uint64_t paso = ahora - gPerfilDesde;
    if (paso < 2000000) return;
    const float s = (float)paso * 1e-6f;
    w3dLogf("perfil: %.1f fps en pantalla, %.1f cuadros de consola/s (%d sin dibujar); emu %.2f ms/cuadro, "
            "dibujo %.2f ms (subida %.2f), swap %.2f ms; sonido en cola %d (desvio %d)",
            gPresentados / s, gEmulados / s, gSaltados, gEmulados ? (float)gEmuUs / 1000.0f / gEmulados : 0.0f,
            gPresentados ? (float)gDibujoUs / 1000.0f / gPresentados : 0.0f,
            gPresentados ? (float)gSubidaUs / 1000.0f / gPresentados : 0.0f,
            gPresentados ? (float)gSwapUs / 1000.0f / gPresentados : 0.0f, emu_sonido_en_cola(), emu_sonido_desvio());
    gPerfilDesde = ahora;
    w3dLogf("perfil: emu dibujando %.2f ms (%d), sin dibujar %.2f ms (%d)",
            gEmuConDibujo ? (float)gEmuConDibujoUs / 1000.0f / gEmuConDibujo : 0.0f, gEmuConDibujo,
            gEmuSinDibujo ? (float)gEmuSinDibujoUs / 1000.0f / gEmuSinDibujo : 0.0f, gEmuSinDibujo);
    gEmuConDibujoUs = gEmuSinDibujoUs = 0;
    gEmuConDibujo = gEmuSinDibujo = 0;
    gEmuUs = gSubidaUs = gDibujoUs = gSwapUs = 0;
    gPresentados = gEmulados = gSaltados = 0;
}

// la frecuencia del sonido de la consola (la opcion): la salida de audio queda como esta (cerrarla y abrirla de nuevo
// con el juego andando fallaba en el N95: "no pude abrir el dispositivo" y despues se caia); la cola convierte
int gHzConsola = 0;
void cambiar_hz() {
    gHzConsola = HZ_SONIDO[gOpc.hz];
    emu_sonido_hz(gHzConsola);
    w3dLogf("juego: sonido de la consola a %d Hz (la salida a %d)", gHzConsola, W3dAudioRate());
}

// los controles en la pantalla: con la opcion en SI, o en AUTO cuando no hay ningun mando conectado
bool tactil_visible() {
    if (!gTactilListo) return false;
    if (gSalida == SALIDA_TV_CONTROLES) return true;   // (el telefono es el mando)
    if (gOpc.tactil == TACTIL_NO) return false;
    return gOpc.tactil == TACTIL_SI || gConectados == 0;
}

// EL DEDO EN EL MENU: el primero que se apoya lo maneja (apoyar, mover, soltar), en pixeles de la imagen del menu
MenuAccion toques_menu() {
    if (esperando_soltar()) return MENU_NADA;
    Toque t[10];
    const int n = plat_toques(t, 10);
    int k = -1;
    for (int i = 0; i < n; i++)
        if (t[i].id == gDedoMenu) k = i;
    if (gDedoMenu >= 0 && k < 0) {   // (se levanto: soltar donde estaba)
        gDedoMenu = -1;
        return menu_toque(TOQUE_SUELTA, gMenuDedoX, gMenuDedoY);
    }
    if (gDedoMenu < 0 && n > 0) {
        int x = 0, y = 0;
        pantalla_a_imagen(t[0].x, t[0].y, &x, &y);
        gDedoMenu = t[0].id;
        gMenuDedoX = x;
        gMenuDedoY = y;
        return menu_toque(TOQUE_APOYA, x, y);
    }
    if (k >= 0) {
        int x = 0, y = 0;
        pantalla_a_imagen(t[k].x, t[k].y, &x, &y);
        if (x != gMenuDedoX || y != gMenuDedoY) {
            gMenuDedoX = x;
            gMenuDedoY = y;
            return menu_toque(TOQUE_MUEVE, x, y);
        }
    }
    return MENU_NADA;
}

// --------------------------------------------------------------------------------- las partidas guardadas
// datos/partidas/<rom>.<n>.est: la cabecera (la fecha y la captura de la pantalla a la mitad del tamano) y despues el
// estado de PicoDrive. datos/partidas/<rom>.srm: la SRAM del cartucho (los juegos con pila)
struct CabezaPartida {
    char marca[4];   // "W3DP"
    int32_t fecha[6];
    uint16_t ancho, alto;   // la captura (RGB565), que sigue a la cabecera
};
const char MARCA_PARTIDA[4] = { 'W', '3', 'D', 'P' };

void carpeta_partidas(char* s, size_t n) { snprintf(s, n, "%spartidas/", plat_carpeta_datos()); }

// el archivo de una ranura (ranura -1: el de la SRAM): el nombre de la ROM sin carpeta ni extension
void ruta_partida(char* s, size_t n, const char* rom, int ranura) {
    const char* b = rom;
    for (const char* p = rom; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    char nombre[128];
    snprintf(nombre, sizeof(nombre), "%s", b);
    char* punto = strrchr(nombre, '.');
    if (punto && punto != nombre) *punto = 0;
    char carpeta[512];
    carpeta_partidas(carpeta, sizeof(carpeta));
    if (ranura < 0) snprintf(s, n, "%s%s.srm", carpeta, nombre);
    else snprintf(s, n, "%s%s.%d.est", carpeta, nombre, ranura + 1);
}

bool leer_cabeza(FILE* f, CabezaPartida& c) {
    return fread(&c, sizeof(c), 1, f) == 1 && !memcmp(c.marca, MARCA_PARTIDA, 4) && c.ancho <= MENU_MINI_ANCHO &&
           c.alto <= MENU_MINI_ALTO;
}

int partidas_listar(const char* rom, MenuPartida* p) {
    int n = 0;
    for (int r = 0; r < MENU_RANURAS; r++) {
        p[r].usada = false;
        char ruta[512];
        ruta_partida(ruta, sizeof(ruta), rom, r);
        FILE* f = fopen(ruta, "rb");
        if (!f) continue;
        CabezaPartida c;
        if (leer_cabeza(f, c)) {
            p[r].usada = true;
            for (int k = 0; k < 6; k++) p[r].fecha[k] = c.fecha[k];
            n++;
        }
        fclose(f);
    }
    return n;
}

bool partidas_captura(const char* rom, int ranura, uint16_t* img, int* ancho, int* alto) {
    char ruta[512];
    ruta_partida(ruta, sizeof(ruta), rom, ranura);
    FILE* f = fopen(ruta, "rb");
    if (!f) return false;
    CabezaPartida c;
    bool ok = leer_cabeza(f, c) && fread(img, 2, (size_t)c.ancho * c.alto, f) == (size_t)c.ancho * c.alto;
    fclose(f);
    *ancho = ok ? c.ancho : 0;
    *alto = ok ? c.alto : 0;
    return ok && c.ancho && c.alto;
}

const MenuFuentePartidas gFuentePartidas = { partidas_listar, partidas_captura };

// la captura: la imagen de la consola a la mitad (cada pixel, el promedio de 4)
uint16_t gCaptura[MENU_MINI_ANCHO * MENU_MINI_ALTO];
void capturar(int* cw, int* ch) {
    int w = 0, h = 0;
    const uint16_t* img = emu_imagen(&w, &h);
    *cw = w / 2 > MENU_MINI_ANCHO ? MENU_MINI_ANCHO : w / 2;
    *ch = h / 2 > MENU_MINI_ALTO ? MENU_MINI_ALTO : h / 2;
    for (int y = 0; y < *ch; y++)
        for (int x = 0; x < *cw; x++) {
            const uint16_t* p = img + y * 2 * w + x * 2;
            const unsigned a = p[0], b = p[1], c = p[w], d = p[w + 1];
            const unsigned r = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11)) >> 2;
            const unsigned g = (((a >> 5) & 63) + ((b >> 5) & 63) + ((c >> 5) & 63) + ((d >> 5) & 63)) >> 2;
            const unsigned z = ((a & 31) + (b & 31) + (c & 31) + (d & 31)) >> 2;
            gCaptura[y * *cw + x] = (uint16_t)((r << 11) | (g << 5) | z);
        }
}

bool guardar_partida(int ranura) {
    if (!gCargada || ranura < 0 || ranura >= MENU_RANURAS) return false;
    char carpeta[512], ruta[512];
    carpeta_partidas(carpeta, sizeof(carpeta));
    plat_crear_carpeta(carpeta);
    ruta_partida(ruta, sizeof(ruta), gRom, ranura);
    FILE* f = fopen(ruta, "wb");
    if (!f) {
        w3dLogf("juego: no se pudo crear %s", ruta);
        return false;
    }
    CabezaPartida c;
    memcpy(c.marca, MARCA_PARTIDA, 4);
    int fecha[6];
    plat_fecha(fecha);
    for (int k = 0; k < 6; k++) c.fecha[k] = fecha[k];
    int w = 0, h = 0;
    capturar(&w, &h);
    c.ancho = (uint16_t)w;
    c.alto = (uint16_t)h;
    bool ok = fwrite(&c, sizeof(c), 1, f) == 1 && fwrite(gCaptura, 2, (size_t)w * h, f) == (size_t)w * h;
    ok = ok && emu_estado(f, 1);
    fclose(f);
    if (!ok) remove(ruta);
    w3dLogf("juego: partida %s en %s", ok ? "guardada" : "NO guardada", ruta);
    return ok;
}

bool cargar_partida(int ranura) {
    char ruta[512];
    ruta_partida(ruta, sizeof(ruta), gRom, ranura);
    FILE* f = fopen(ruta, "rb");
    if (!f) return false;
    CabezaPartida c;
    bool ok = leer_cabeza(f, c) && fseek(f, (long)c.ancho * c.alto * 2, SEEK_CUR) == 0 && emu_estado(f, 0);
    fclose(f);
    w3dLogf("juego: partida %s de %s", ok ? "cargada" : "NO cargada", ruta);
    return ok;
}

// la SRAM del juego cargado, si cambio (al ir al menu, al pasar al fondo, al cambiar de ROM y al salir)
void guardar_sram() {
    if (!gCargada) return;
    char carpeta[512], ruta[512];
    carpeta_partidas(carpeta, sizeof(carpeta));
    plat_crear_carpeta(carpeta);
    ruta_partida(ruta, sizeof(ruta), gRom, -1);
    if (emu_sram(ruta, 1)) w3dLogf("juego: SRAM guardada en %s", ruta);
}

// --------------------------------------------------------------------------------- el menu y la partida
// los primeros cuadros de cada ROM van al log (y al archivo enseguida): si el nucleo se cae, se ve donde
int gCuadrosRastro = 0;

bool cargar(const char* ruta) {
    guardar_sram();   // (la del juego que estaba)
    w3dLogf("juego: ROM %s", ruta);
    w3dLogBajar();
    gCuadrosRastro = 5;
    gCargada = emu_cargar(ruta) != 0;
    if (!gCargada) return false;
    {
        char srm[512];
        ruta_partida(srm, sizeof(srm), ruta, -1);
        if (emu_sram(srm, 0)) w3dLogf("juego: SRAM de %s", srm);
    }
    const int entrada = entrada_del_juego();
    emu_entrada(entrada);
    w3dLogf("juego: %d Hz de video, %d Hz de sonido, entrada %d (%s)", emu_hz_video(), W3dAudioRate(), entrada,
            entrada == EMU_TEAM_PLAYER_1 ? "Team Player, 4 jugadores" : entrada == EMU_DOS_MANDOS ? "dos mandos" : "otra");
    strncpy(gRom, ruta, sizeof(gRom) - 1);
    gRom[sizeof(gRom) - 1] = 0;
    return true;
}

void abrir_menu() {
    guardar_sram();
    gEsperarSoltar = true;
    gDedoMenu = -1;
    char ultimaRom[512];
    ultima(ultimaRom, sizeof(ultimaRom), false);
    menu_abrir(gCarpeta, gCargada ? gRom : ultimaRom, gCargada);
    gEnMenu = true;
    w3dLogf("juego: menu con %d ROMs de %s", menu_cantidad(), gCarpeta);
}

void a_la_partida() {
    gEsperarSoltar = true;
    gDedoMenu = -1;
    gEnMenu = false;
    gRelojPuesto = false;
    gImagenSubida = 0xffffffff;   // (la imagen de la consola de nuevo en la pantalla)
}

// el menu: los botones de las teclas y de todos los mandos (cuadrado, cruz, Start o el centro eligen; la C del
// telefono, circulo, Select o PS vuelven a la partida)
bool cuadro_menu(int pw, int ph, uint64_t ahora) {
    uint16_t pads[4];
    leer_pads(pads);
    const uint16_t todos = pads[0] | pads[1] | pads[2] | pads[3];
    // (al log, lo que llega de cada mando: si un boton no responde, se ve si llega)
    static uint16_t antes[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        if (pads[i] != antes[i]) w3dLogf("menu: mando %d botones 0x%03x", i + 1, pads[i]);
        antes[i] = pads[i];
    }
    int b = 0;
    if (todos & MD_ARRIBA) b |= MENU_ARRIBA;
    if (todos & MD_ABAJO) b |= MENU_ABAJO;
    if (todos & MD_IZQ) b |= MENU_IZQ;
    if (todos & MD_DER) b |= MENU_DER;
    if (todos & (MD_A | MD_B)) b |= MENU_ELEGIR;
    if (todos & MD_START) b |= MENU_OPCIONES;
    if ((todos & MD_C) || plat_tecla(TK_BORRAR) || plat_tecla(TK_SOFT_DER) || gGuia) b |= MENU_VOLVER;
    menu_mandos(gJugadores);
    MenuAccion accion = menu_actualizar(b, ahora);
    if (accion == MENU_NADA) accion = toques_menu();
    if (accion == MENU_JUGAR) {
        w3dLogf("juego: elegida con los botones 0x%x (teclas y mandos 0x%x 0x%x 0x%x 0x%x)", b, pads[0], pads[1],
                pads[2], pads[3]);
        char ruta[512];
        strncpy(ruta, menu_elegida(), sizeof(ruta) - 1);
        ruta[sizeof(ruta) - 1] = 0;
        const int ranura = menu_ranura();
        if (cargar(ruta)) {
            if (ranura >= 0) cargar_partida(ranura);
            ultima(ruta, 0, true);
            a_la_partida();
            gGuiaAntes = gGuia;
            gBorrarAntes = true;
            return false;
        }
    } else if (accion == MENU_CAMBIO_OPCIONES) {
        opciones(true);
        if (gCargada) emu_entrada(entrada_del_juego());
        emu_sonido_calidad(gOpc.filtro, gOpc.fmAlta);
        if (HZ_SONIDO[gOpc.hz] != gHzConsola) cambiar_hz();
        emu_dibujo_rapido(gOpc.rapido);
        w3dLogf("juego: opciones fps %d, multitap %d, escala %d, suave %d, tactil %d", gOpc.fps, gOpc.multitap,
                gOpc.escala, gOpc.suave, gOpc.tactil);
    } else if (accion == MENU_GUARDAR) {
        const int ranura = menu_ranura();
        char aviso[48];
        if (guardar_partida(ranura))
            snprintf(aviso, sizeof(aviso), menu_tr("Guardada en la ranura %d", "Saved to slot %d"), ranura + 1);
        else
            snprintf(aviso, sizeof(aviso), "%s", menu_tr("No se pudo guardar", "Could not save"));
        menu_aviso(aviso, ahora + 3000000);
    } else if (accion == MENU_SALIR) {
        guardar_sram();
        plat_salir();
        return false;
    } else if (accion == MENU_SEGUIR) {
        a_la_partida();
        gGuiaAntes = gGuia;
        gBorrarAntes = true;
        return false;
    }
    if (!menu_dibujar(gMenuImagen)) return false;
    int mw = MENU_ANCHO, mh = MENU_ALTO;
    menu_tamano(&mw, &mh);
    // (el menu, lo mas grande que entra y pixelado: en el N8, 320x180 al doble)
    pantalla_modo(ESCALA_AJUSTAR, 0);
    pantalla_subir(gMenuImagen, mw, mh);
    pantalla_dibujar(pw, ph);
    return true;
}
}

// --------------------------------------------------------------------------------------------- juego.h
// (los mensajes del nucleo son pocos: se bajan al archivo enseguida, por si lo que sigue es un crash)
void emu_mensaje(const char* texto) {
    w3dLogf("pico: %s", texto);
    w3dLogBajar();
}

void juego_iniciar(int hzSonido) {
    w3dLog("juego: segaw3d (PicoDrive)");
    leer_config();
    // (la frecuencia de la plataforma, salvo que las opciones digan otra)
    gOpc.hz = hzSonido >= 44100 ? 2 : hzSonido >= 32000 ? 1 : 0;
    opciones(false);
    menu_opciones(&gOpc);
    menu_partidas(&gFuentePartidas);
    menu_idioma_sistema(plat_espanol());
    leer_prueba();
    pantalla_iniciar();
    // la pantalla tactil: el menu de filas grandes (320x180, al doble en el N8) y los controles de Mega Drive
    if (plat_es_tactil()) {
        menu_configurar(320, 180, 22, true, plat_escala());
        gTactilListo = tactil_iniciar();
    } else {
        menu_configurar(MENU_ANCHO, MENU_ALTO, 9, false, plat_escala());
    }
    // (la salida de audio siempre a 44100: la consola genera a la frecuencia de la opcion y la cola convierte)
    W3dAudioInit(44100);
    gHzConsola = HZ_SONIDO[gOpc.hz];
    emu_iniciar(W3dAudioRate() > 0 ? W3dAudioRate() : 44100, gHzConsola);
    emu_dibujo_rapido(gOpc.rapido);
    w3dLogf("juego: sonido de la consola a %d Hz (la salida a %d); dibujo %s", gHzConsola, W3dAudioRate(),
            gOpc.rapido ? "rapido" : "preciso");
    emu_sonido_volumen(gVolumen * 256 / 10);
    emu_sonido_calidad(gOpc.filtro, gOpc.fmAlta);
    W3dAudioFuente(sonido_fuente, 0);
    // con rom= (las pruebas) directo a la partida; si no, el menu
    if (gRom[0] && cargar(gRom)) return;
    abrir_menu();
}

bool juego_cuadro() {
    int pw = 320, ph = 240;
    plat_pantalla(pw, ph);
    const uint64_t ahora = plat_ahora_us();
    // la tecla de abajo a la derecha: jugando va al menu (en el menu vuelve a la partida: ver cuadro_menu)
    const bool softDer = plat_tecla(TK_SOFT_DER);
    const bool softDerNueva = softDer && !gSoftDerAntes;
    gSoftDerAntes = softDer;
    if (gSalirEn && ahora > gSalirEn) {
        w3dLog("prueba: terminada");
        plat_salir();
    }
    if (gPausa) return false;
    if (gEnMenu) return cuadro_menu(pw, ph, ahora);

    uint16_t pads[4];
    leer_pads(pads);
    // los controles en la pantalla: el jugador 1 (junto con el teclado y su mando)
    bool menuTactil = false;
    if (tactil_visible() && !esperando_soltar()) {
        Toque t[10];
        const int n = plat_toques(t, 10);
        pads[0] |= tactil_leer(t, n, pw, ph, &menuTactil);
    }
    // al menu: la tecla de abajo a la derecha, la C del telefono o Select / PS de un mando (al apretarlos, no mientras
    // siguen apretados)
    const bool borrar = plat_tecla(TK_BORRAR);
    const bool alMenu = softDerNueva || menuTactil || (borrar && !gBorrarAntes) || (gGuia && !gGuiaAntes);
    gBorrarAntes = borrar;
    gGuiaAntes = gGuia;
    if (alMenu) {
        abrir_menu();
        return false;
    }
    // los cuadros de la consola que tocan hasta ahora (al reloj de la consola: si el telefono se atrasa, los que
    // faltan se corren sin dibujar; mas de 4 de atraso, se larga de nuevo)
    // (el reloj del sonido y el del telefono no van iguales: si en la cola sobra sonido la consola va un poco mas
    // lenta, si falta un poco mas rapida; 100 ms de diferencia = 1 %, hasta 0,5 %: no se nota en la imagen)
    int64_t periodo = 1000000 / (int64_t)emu_hz_video();
    {
        int64_t ajuste = (int64_t)emu_sonido_desvio() * periodo / gHzConsola * 10 / 100;
        const int64_t tope = periodo / 200;
        if (ajuste > tope) ajuste = tope;
        if (ajuste < -tope) ajuste = -tope;
        periodo += ajuste;
    }
    if (!gRelojPuesto || ahora > gReloj + 4 * periodo) {
        gReloj = ahora;
        gRelojPuesto = true;
    }
    // (un cuadro puede empezar hasta 3 ms antes de que toque: el ritmo sigue siendo el de la consola, pero si el
    // anterior termino un poco tarde o el sueno de 1 ms se paso, no se pierde el cuadro entero)
    const uint64_t ADELANTO = 3000;
    int n = 0;
    while (ahora + ADELANTO >= gReloj && n < 4) {
        n++;
        gReloj += periodo;
    }
    // (todavia no toca: no hay nada nuevo que mostrar, salvo que la pantalla se haya borrado)
    if (n == 0 && !gRedibujar) return false;
    gRedibujar = false;
    const uint64_t t0 = plat_ahora_us();
    for (int i = 0; i < n; i++) {
        const bool dibujar = i == n - 1;
        uint16_t p[4] = { pads[0], pads[1], pads[2], pads[3] };
        if (gGuion[0]) {
            guion_cuadro(++gCuadroConsola);
            p[gGuionJugador] |= gGuionPad;
        }
        if (gCuadrosRastro) {
            w3dLogf("juego: cuadro de consola (quedan %d con rastro)", gCuadrosRastro);
            w3dLogBajar();
        }
        const uint64_t tc = plat_ahora_us();
        emu_cuadro(p, dibujar ? 1 : 0);
        if (dibujar) {
            gEmuConDibujoUs += plat_ahora_us() - tc;
            gEmuConDibujo++;
        } else {
            gEmuSinDibujoUs += plat_ahora_us() - tc;
            gEmuSinDibujo++;
        }
        if (gCuadrosRastro) {
            gCuadrosRastro--;
            w3dLog("juego: cuadro de consola hecho");
            w3dLogBajar();
        }
        gEmulados++;
        if (!dibujar) gSaltados++;
    }
    gEmuUs += plat_ahora_us() - t0;

    const uint64_t t1 = plat_ahora_us();
    if (emu_imagen_numero() != gImagenSubida) {
        gImagenSubida = emu_imagen_numero();
        int w = 0, h = 0;
        const uint16_t* img = emu_imagen(&w, &h);
        // el contador de FPS, arriba a la derecha de la imagen (se escribe en la imagen de la consola: el cuadro
        // siguiente la vuelve a dibujar entera)
        if (gOpc.fps) {
            char t[8];
            snprintf(t, sizeof(t), "%d", gFpsMostrado);
            const int largo = (int)strlen(t) * 8, x = w - largo - 3;
            uint16_t* p = (uint16_t*)img;
            for (int yy = 1; yy < 11; yy++)
                for (int xx = x - 2; xx < w - 1; xx++) p[yy * w + xx] = 0;
            menu_texto(p, w, h, x, 2, t, gFpsMostrado >= 55 ? 0x07E0 : gFpsMostrado >= 30 ? 0xFFE0 : 0xF800);
        }
        const uint64_t ts = plat_ahora_us();
        pantalla_subir(img, w, h);
        gSubidaUs += plat_ahora_us() - ts;
    }
    pantalla_modo(gOpc.escala, gOpc.suave);
    // (con el juego en la TV, el telefono en negro: solo los controles, si van)
    if (gSalida == SALIDA_TV || gSalida == SALIDA_TV_CONTROLES) pantalla_limpiar(pw, ph);
    else pantalla_dibujar(pw, ph);
    if (tactil_visible()) tactil_dibujar(pw, ph);
    dibujar_aviso(pw, ph);
    dibujar_volumen(pw, ph);
    gDibujoUs += plat_ahora_us() - t1;
    gPresentados++;
    gFpsCuenta++;
    if (ahora - gFpsDesde >= 1000000) {
        gFpsMostrado = gFpsCuenta;
        gFpsCuenta = 0;
        gFpsDesde = ahora;
    }
    perfil_cuadro(plat_ahora_us());
    return true;
}

void juego_swap_us(uint64_t us) { gSwapUs += us; }

void juego_salida(int salida) {
    if (salida == gSalida) return;
    gSalida = salida;
    w3dLogf("juego: salida %d (0 telefono, 1 clonar, 2 solo la tele, 3 tele + controles)", salida);
    juego_redibujar();
}

void juego_dibujar_tv(int w, int h) {
    if (gEnMenu) pantalla_modo(ESCALA_AJUSTAR, 0);
    else pantalla_modo(gOpc.escala, gOpc.suave);
    pantalla_dibujar_externa(w, h);
    dibujar_volumen(w, h);
}

void juego_redibujar() {
    gRedibujar = true;
    gImagenSubida = 0xffffffff;
    menu_redibujar();
}

void juego_pausa(bool pausa) {
    if (pausa) guardar_sram();   // (al fondo: el sistema puede cerrar la app sin avisar)
    gPausa = pausa;
    gRelojPuesto = false;
    if (!pausa) juego_redibujar();
    W3dAudioPause(pausa);
}

void juego_terminar() {
    guardar_sram();
    W3dAudioFuente(0, 0);
    W3dAudioShutdown();
    emu_terminar();
    pantalla_terminar();
}

void juego_guion(const char* guion) {
    strncpy(gGuion, guion ? guion : "", sizeof(gGuion) - 1);
    gGuion[sizeof(gGuion) - 1] = 0;
}

void juego_volumen(int paso) {
    gVolumen += paso;
    if (gVolumen < 0) gVolumen = 0;
    if (gVolumen > 10) gVolumen = 10;
    emu_sonido_volumen(gVolumen * 256 / 10);
    gVolumenHasta = plat_ahora_us() + 1500000;
    w3dLogf("juego: volumen %d/10", gVolumen);
}
