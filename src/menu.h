// ---------------------------------------------------------------------------------------------------------------
//  El menu de las ROMs: la lista de los juegos de una carpeta, para elegir cual jugar. Se dibuja por CPU en una imagen
//  RGB565 de 320x240 (la letra de 8x8 de PicoDrive, platform/libpicofe/fonts.c) que va a la pantalla como un cuadro
//  mas de la consola: igual en la PC y en el N95.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

enum { MENU_ANCHO = 320, MENU_ALTO = 240 };

// lo que pide el menu con las teclas o los mandos (bits)
enum {
    MENU_ARRIBA = 1 << 0, MENU_ABAJO = 1 << 1, MENU_IZQ = 1 << 2, MENU_DER = 1 << 3,
    MENU_ELEGIR = 1 << 4, MENU_VOLVER = 1 << 5, MENU_OPCIONES = 1 << 6
};
// lo que el menu le contesta al juego
enum MenuAccion { MENU_NADA, MENU_JUGAR, MENU_SEGUIR, MENU_SALIR, MENU_CAMBIO_OPCIONES };

// LAS OPCIONES (Start en el menu): las guarda el juego; el menu las cambia y avisa con MENU_CAMBIO_OPCIONES
enum { MULTITAP_AUTO, MULTITAP_SIEMPRE, MULTITAP_NUNCA };
struct MenuOpciones {
    int fps;        // 1 = el contador de cuadros por segundo arriba a la derecha
    int multitap;   // MULTITAP_*: el Team Player (4 jugadores)
    int filtro;     // 1 = el pasabajos del sonido
    int fmAlta;     // 1 = el FM en alta calidad (mas lento)
    int hz;         // la frecuencia del sonido: 0 = 22050, 1 = 32000, 2 = 44100 (mas alta = mejor y mas lento)
    int rapido;     // 1 = el dibujo rapido (por tiles: mas barato, sin los efectos de mitad de pantalla)
};
void menu_opciones(MenuOpciones* o);

// busca las ROMs de la carpeta (con la barra del final) y deja elegida la de 'ultima' (la ruta entera) si esta. La
// ultima fila de la lista es "Salir" (cierra el emulador)
void menu_abrir(const char* carpeta, const char* ultima, bool hayJuego);
int menu_cantidad();
// los botones apretados ahora (bits MENU_*), una vez por cuadro: mantenidos, arriba/abajo se repiten
MenuAccion menu_actualizar(int botones, uint64_t ahoraUs);
// la ruta de la ROM elegida (despues de MENU_JUGAR)
const char* menu_elegida();
// los mandos conectados (bits 0..3), para mostrarlos abajo
void menu_mandos(int conectados);
// todo de nuevo en el proximo menu_dibujar (la pantalla se borro: la app volvio al frente)
void menu_redibujar();
// un texto con la letra de 8x8 en una imagen RGB565 de 'ancho' pixeles por fila (y 'alto' filas)
void menu_texto(uint16_t* img, int ancho, int alto, int x, int y, const char* s, uint16_t color);
// dibuja el menu si cambio algo: true = hay una imagen nueva en 'imagen' (MENU_ANCHO x MENU_ALTO)
bool menu_dibujar(uint16_t* imagen);
