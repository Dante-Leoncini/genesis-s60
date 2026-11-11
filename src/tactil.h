// ---------------------------------------------------------------------------------------------------------------
//  Los controles de Mega Drive en la pantalla tactil: la cruceta abajo a la izquierda, A B C abajo a la derecha (como
//  en el mando), Start abajo al medio y el boton del menu arriba a la derecha. Varios dedos a la vez: un dedo que
//  empezo en la cruceta la sigue manejando aunque se salga. Los graficos se arman por codigo (sprites RGBA de
//  pantalla.h), semitransparentes encima de la imagen.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

// un dedo apoyado (las coordenadas son pixeles de la pantalla; 'id' sigue al mismo dedo mientras esta apoyado)
struct Toque {
    int id;
    int x, y;
};

// arma los graficos (despues de pantalla_iniciar). false = la pantalla no tiene sprites (el N95)
bool tactil_iniciar();
// los dedos de este cuadro -> los botones de Mega Drive (bits MD_* de emu.h); 'menu' = se toco el boton del menu
uint16_t tactil_leer(const Toque* toques, int n, int pw, int ph, bool* menu);
// los controles encima de la imagen (lo apretado, mas fuerte)
void tactil_dibujar(int pw, int ph);
