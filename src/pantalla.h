// ---------------------------------------------------------------------------------------------------------------
//  La imagen de la consola en la pantalla, PIXEL PERFECT: a escala entera (en el N95 acostado, 1:1), sin filtro, sin
//  estirar y centrada. En la PC con OpenGL (pantalla.cpp); en el N95 por CPU, directo en la pantalla
//  (symbian/src/pantalla_symbian.cpp).
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

void pantalla_iniciar();
void pantalla_terminar();
// sube la imagen nueva (ancho x alto, filas seguidas) si cambio
void pantalla_subir(const uint16_t* imagen, int ancho, int alto);
// dibuja la ultima imagen en una pantalla de pw x ph
void pantalla_dibujar(int pw, int ph);
// un rectangulo de color encima (los avisos de los jugadores): x, y, w, h en pixeles de la pantalla
void pantalla_rectangulo(int pw, int ph, int x, int y, int w, int h, uint32_t rgba);
