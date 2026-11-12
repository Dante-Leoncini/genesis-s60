// ---------------------------------------------------------------------------------------------------------------
//  LOS CONTROLES de cada jugador (1 a 4): que tecla del telefono y que boton del mando aprieta cada boton del mando de
//  Mega Drive. Cada boton tiene hasta dos teclas del telefono y un boton del mando (el stick izquierdo es siempre la
//  cruceta). Se cambian en el menu (Controles): se elige el boton y se aprieta la tecla o el boton del mando.
//
//  Lo que no se asigna: la tecla de abajo a la derecha del telefono (va al menu y pausa) y PS / Home del mando (el
//  menu). Select tambien va al menu mientras no tenga un boton asignado.
//
//  El menu se maneja siempre con los controles de fabrica (controles_*(..., true)): una asignacion rara no deja a
//  nadie trabado.
//
//  datos/controles.txt: una linea por boton, "jugador boton tecla1 tecla2 mando" (-1 = nada).
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include "W3dMandos.h"

enum { CTRL_JUGADORES = 4, CTRL_BOTONES = 12, CTRL_NADA = -1 };
// los botones del mando del telefono que no son botones: los gatillos (despues de los de W3dMandos.h)
enum { CTRL_LT = w3dEngine::MandoBotones, CTRL_RT, CTRL_FUENTES_MANDO };

// todo como viene de fabrica (jugador -1: todos)
void controles_defecto(int jugador);
// lee (o escribe) datos/controles.txt
void controles_archivo(const char* ruta, bool escribir);

// los botones de Mega Drive (bits MD_* de emu.h) que aprietan las teclas del telefono para el jugador
uint16_t controles_teclas(int jugador, bool deFabrica);
// los que aprieta su mando (el stick izquierdo es la cruceta)
uint16_t controles_mando(int jugador, const w3dEngine::MandoEstado& e, bool deFabrica);
// Select del mando del jugador no tiene un boton asignado (entonces va al menu)
bool controles_select_libre(int jugador);
// el tipo de mando del jugador (MandoTipo): para los nombres de los botones
void controles_tipo(int jugador, int tipo);

// para el menu: el nombre del boton de Mega Drive (0..CTRL_BOTONES-1) y lo que tiene asignado
const char* controles_nombre(int boton);
void controles_texto(int jugador, int boton, bool conTeclas, char* s, int n);   // (conTeclas: las del telefono)
// ESPERAR UNA TECLA para el boton: llamar empezar y despues una vez por cuadro. 0 = todavia nada, 1 = asignada,
// -1 = cancelado (la tecla de abajo a la derecha o PS / Home). Espera que se suelte todo antes de tomar la tecla
void controles_esperar_empezar();
int controles_esperar(int jugador, int boton);
