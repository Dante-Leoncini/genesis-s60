// ---------------------------------------------------------------------------------------------------------------
//  El juego: la consola con la ROM, al reloj de la consola (60 cuadros por segundo; si el telefono se atrasa, corre
//  cuadros sin dibujarlos para no perder velocidad ni sonido), los mandos y las teclas, y la imagen en la pantalla.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

// la frecuencia del sonido de la plataforma (22050 en el N95, 44100 en la PC)
void juego_iniciar(int hzSonido);
// true: hay un cuadro nuevo para presentar; false: todavia no toca (no presentar: la plataforma espera un poco)
bool juego_cuadro();
// la pantalla se borro (la app volvio al frente, otra ventana la tapo): el proximo cuadro se dibuja entero aunque no
// haya nada nuevo
void juego_redibujar();
// lo que tardo la plataforma en presentar (para el perfil)
void juego_swap_us(uint64_t us);
void juego_pausa(bool pausa);
void juego_terminar();
// las teclas de volumen del costado: +1 / -1
void juego_volumen(int paso);
// LAS PRUEBAS: botones que se aprietan solos, por cuadro de la consola: "600:start,610:nada,1550:abajo,..." (cada
// entrada deja apretados esos botones hasta la proxima; arriba abajo izq der start a b c x y z mode; "nada" suelta
// todo; "j3" pasa al jugador 3)
void juego_guion(const char* guion);
