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
// LA PANTALLA EXTERNA (la TV del N8: RCA o HDMI): a donde va la imagen. Sin TV, el telefono; clonar = el juego en
// las dos; solo la tele = el telefono en negro; tele + controles = el telefono muestra solo los controles tactiles
// (es el mando). En el menu, el telefono lo muestra igual (para tocarlo)
enum { SALIDA_TELEFONO, SALIDA_CLONAR, SALIDA_TV, SALIDA_TV_CONTROLES };
void juego_salida(int salida);
// LA TELE: se enchufo una pantalla externa de ancho x alto: el juego pregunta que mostrar en su menu (se contesta con
// el mando, las teclas o el dedo; si se estaba jugando, despues sigue la partida) y contesta con plat_tv_elegida
void juego_preguntar_tv(int ancho, int alto);
// despues de juego_cuadro (con la superficie de la TV puesta): la misma imagen en la TV, de w x h
void juego_dibujar_tv(int w, int h);
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
