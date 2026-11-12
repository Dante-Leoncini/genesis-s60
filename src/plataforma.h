// ---------------------------------------------------------------------------------------------------------------
//  Lo que pone cada plataforma (symbian/: el N95, con Carbide; pc/: la prueba en la PC con SDL): el reloj, donde
//  estan los datos, la pantalla, las teclas, los mandos y la salida. La plataforma crea la ventana con OpenGL ES 1.1
//  (en la PC, OpenGL 1.x), llama a juego.h una vez por cuadro y presenta el cuadro.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include "W3dMandos.h"

// el reloj, en microsegundos (solo cuenta la diferencia)
uint64_t plat_ahora_us();
// la carpeta de datos (la ROM, el registro), con la barra del final
const char* plat_carpeta_datos();
// la pantalla en pixeles (en el N95 acostado: 320 x 240)
void plat_pantalla(int& ancho, int& alto);

// las teclas del N95 (en la PC, las del teclado que hacen de ellas: ver pc/main_pc.cpp)
enum Tecla {
    TK_ARRIBA, TK_ABAJO, TK_IZQ, TK_DER, TK_CENTRO,
    TK_0, TK_1, TK_2, TK_3, TK_4, TK_5, TK_6, TK_7, TK_8, TK_9, TK_ASTERISCO, TK_NUMERAL,
    TK_SOFT_IZQ, TK_SOFT_DER, TK_BORRAR, TK_VERDE, TK_LAPIZ,
    TECLAS
};
bool plat_tecla(int t);
// LOS JOYSTICKS, hasta 4 (en el N95, los de PS3 y PS4 por Bluetooth con el driver bthid-s60; en la PC, los de SDL):
// el estado del mando i en el orden del Core (base/W3dMandos.h). false = no hay mando con ese numero
bool plat_mando(int i, w3dEngine::MandoEstado& e);
// el acelerometro del mando i si es un Wiimote (en g: X a lo ancho, Y a lo largo hacia la punta, Z de la cara de los
// botones). false = no es un Wiimote
bool plat_mando_acelerometro(int i, float g[3]);
// el juego pide salir (la tecla roja la maneja el sistema)
void plat_salir();

// LA PANTALLA TACTIL (el N8; en la PC, el mouse con --tactil): si hay, y los dedos apoyados ahora, en pixeles de la
// pantalla del telefono (hasta 'max'; devuelve cuantos)
struct Toque;
bool plat_es_tactil();
int plat_toques(Toque* toques, int max);
// la imagen se puede escalar y filtrar (hay GPU: la PC y el N8; el N95 dibuja 1:1 por CPU)
bool plat_escala();
// el idioma del sistema es el castellano (el menu sale en castellano; si no, en ingles)
bool plat_espanol();
// la fecha y la hora del telefono (la local): anio, mes (1..12), dia (1..31), hora, minutos, segundos
void plat_fecha(int f[6]);
// crea la carpeta 'ruta' si no esta (con la barra del final o sin ella)
void plat_crear_carpeta(const char* ruta);
