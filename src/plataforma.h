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
// el juego pide salir (la tecla roja la maneja el sistema)
void plat_salir();
