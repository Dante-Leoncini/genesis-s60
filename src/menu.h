// ---------------------------------------------------------------------------------------------------------------
//  El menu de las ROMs: la lista de los juegos de una carpeta, para elegir cual jugar, y la de las opciones. Se dibuja
//  por CPU en una imagen RGB565 (la letra de 8x8 de PicoDrive, platform/libpicofe/fonts.c) que va a la pantalla como
//  un cuadro mas de la consola: igual en la PC, en el N95 y en el N8.
//
//  El tamano lo elige la plataforma (menu_configurar): en el N95, 320x240 con filas chicas (se maneja con teclas); en
//  una pantalla tactil, filas del alto de un dedo, botones arriba y abajo, arrastrar desplaza y tocar elige.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

// lo mas grande que puede ser la imagen del menu
enum { MENU_ANCHO = 320, MENU_ALTO = 240 };

// lo que pide el menu con las teclas o los mandos (bits)
enum {
    MENU_ARRIBA = 1 << 0, MENU_ABAJO = 1 << 1, MENU_IZQ = 1 << 2, MENU_DER = 1 << 3,
    MENU_ELEGIR = 1 << 4, MENU_VOLVER = 1 << 5, MENU_OPCIONES = 1 << 6
};
// lo que el menu le contesta al juego (MENU_JUGAR y MENU_GUARDAR: la ranura en menu_ranura; MENU_CAMBIO_CONTROLES:
// se cambio un control, guardarlos)
enum MenuAccion { MENU_NADA, MENU_JUGAR, MENU_SEGUIR, MENU_SALIR, MENU_CAMBIO_OPCIONES, MENU_GUARDAR,
                  MENU_CAMBIO_CONTROLES, MENU_TV };

// CON UN JUEGO CARGADO el menu abre en la PAUSA: guardar la partida, cargar una guardada, cargar otra ROM, los
// controles, las opciones, volver al juego y cerrar. Sin juego, la lista de las ROMs (Start: las opciones)
//
// LAS PARTIDAS GUARDADAS, de a ranuras, con la fecha y una captura de la pantalla. Al elegir una ROM que tiene
// partidas, se elige entre una partida nueva o una de las guardadas. Los archivos los maneja el juego: el menu le pide
// la lista y las capturas
enum { MENU_RANURAS = 8, MENU_MINI_ANCHO = 160, MENU_MINI_ALTO = 120 };
struct MenuPartida {
    bool usada;
    int fecha[6];   // anio, mes (1..12), dia, hora, minutos, segundos
};
struct MenuFuentePartidas {
    // las MENU_RANURAS partidas de la ROM 'ruta'; devuelve cuantas hay
    int (*listar)(const char* ruta, MenuPartida* partidas);
    // la captura de una ranura (hasta MENU_MINI_ANCHO x MENU_MINI_ALTO); false = no hay
    bool (*captura)(const char* ruta, int ranura, uint16_t* img, int* ancho, int* alto);
};
void menu_partidas(const MenuFuentePartidas* fuente);
// despues de MENU_JUGAR: -1 = partida nueva, 0.. = cargar esa ranura; despues de MENU_GUARDAR: donde guardar
int menu_ranura();
// LA PREGUNTA DE LA TELE (el N8, al enchufar el cable): una pantalla del menu, encima de todo, que se contesta con el
// mando, las teclas o el dedo (sin levantarse a tocar el telefono). Despues de MENU_TV, menu_tv_salida() dice lo
// elegido: SALIDA_CLONAR, SALIDA_TV o SALIDA_TV_CONTROLES (juego.h); cancelar es clonar
void menu_preguntar_tv(int ancho, int alto);
int menu_tv_salida();
// un aviso en el pie del menu ("Partida guardada") hasta 'hastaUs' (el reloj de menu_actualizar)
void menu_aviso(const char* texto, uint64_t hastaUs);

// LAS OPCIONES (Start en el menu): las guarda el juego; el menu las cambia y avisa con MENU_CAMBIO_OPCIONES
enum { MULTITAP_AUTO, MULTITAP_SIEMPRE, MULTITAP_NUNCA };
enum { TACTIL_AUTO, TACTIL_SI, TACTIL_NO };
enum { IDIOMA_AUTO, IDIOMA_ES, IDIOMA_EN };
struct MenuOpciones {
    int fps;        // 1 = el contador de cuadros por segundo arriba a la derecha
    int multitap;   // MULTITAP_*: el Team Player (4 jugadores)
    int filtro;     // 1 = el pasabajos del sonido
    int fmAlta;     // 1 = el FM en alta calidad (mas lento)
    int hz;         // la frecuencia del sonido: 0 = 22050, 1 = 32000, 2 = 44100 (mas alta = mejor y mas lento)
    int rapido;     // 1 = el dibujo rapido (por tiles: mas barato, sin los efectos de mitad de pantalla)
    int escala;     // ESCALA_* de pantalla.h (donde se puede elegir)
    int suave;      // 1 = la imagen filtrada (borrosa); 0 = pixel perfect
    int tactil;     // TACTIL_*: los controles en la pantalla (auto = si no hay mandos conectados)
    int idioma;     // IDIOMA_*: auto = el del sistema (castellano si es castellano; si no, ingles)
    int inclinacion; // 1 = el Wiimote como volante: inclinarlo aprieta izquierda / derecha (mas inclinado, mas tiempo)
    int volumen;    // 0..10 (tambien las teclas de volumen del costado)
};
void menu_opciones(MenuOpciones* o);
// el idioma del sistema (para IDIOMA_AUTO) y el que se esta usando
void menu_idioma_sistema(bool espanol);
bool menu_en_ingles();
// un texto en el idioma elegido
inline const char* menu_tr(const char* es, const char* en) { return menu_en_ingles() ? en : es; }

// el tamano de la imagen del menu (hasta MENU_ANCHO x MENU_ALTO), el alto de cada fila y si se maneja con el dedo;
// 'pantalla' = se puede elegir la escala, el filtro y los controles tactiles (no en el N95)
void menu_configurar(int ancho, int alto, int fila, bool tactil, bool pantalla);
void menu_tamano(int* ancho, int* alto);

// busca las ROMs de la carpeta (con la barra del final) y deja elegida la de 'ultima' (la ruta entera) si esta. Con
// teclas la ultima fila de la lista es "Salir" (cierra el emulador); con el dedo, un boton abajo a la derecha
void menu_abrir(const char* carpeta, const char* ultima, bool hayJuego);
int menu_cantidad();
// los botones apretados ahora (bits MENU_*), una vez por cuadro: mantenidos, arriba/abajo se repiten
MenuAccion menu_actualizar(int botones, uint64_t ahoraUs);
// EL DEDO, en pixeles de la imagen del menu: apoyar, mover y soltar (arrastrar desplaza; tocar sin mover elige)
enum { TOQUE_APOYA, TOQUE_MUEVE, TOQUE_SUELTA };
MenuAccion menu_toque(int tipo, int x, int y);
// la ruta de la ROM a jugar (despues de MENU_JUGAR)
const char* menu_elegida();
// los mandos conectados (bits 0..3), para mostrarlos abajo
void menu_mandos(int conectados);
// todo de nuevo en el proximo menu_dibujar (la pantalla se borro: la app volvio al frente)
void menu_redibujar();
// un texto con la letra de 8x8 en una imagen RGB565 de 'ancho' pixeles por fila (y 'alto' filas)
void menu_texto(uint16_t* img, int ancho, int alto, int x, int y, const char* s, uint16_t color);
// dibuja el menu si cambio algo: true = hay una imagen nueva en 'imagen' (del tamano de menu_tamano)
bool menu_dibujar(uint16_t* imagen);
