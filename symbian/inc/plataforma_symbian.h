// ---------------------------------------------------------------------------------------------------------------
//  Lo que el contenedor (sega_contenedor.cpp) le pasa a la plataforma del juego (plataforma_symbian.cpp, que
//  implementa src/plataforma.h): el tamano de la ventana, las teclas apretadas y si el juego pidio salir.
// ---------------------------------------------------------------------------------------------------------------
#ifndef PLATAFORMA_SYMBIAN_H
#define PLATAFORMA_SYMBIAN_H

// al arrancar (antes de juego_iniciar): la carpeta de datos, el log, el reloj y las teclas de volumen
void plat_symbian_iniciar();
void plat_symbian_terminar();
// una vez por cuadro, antes del juego (las teclas de volumen mantenidas)
void plat_symbian_cuadro();
void plat_symbian_pantalla(int ancho, int alto);
// una tecla del telefono (el codigo de exploracion: TKeyEvent::iScanCode) se apreto o se solto; false: no es del juego
bool plat_symbian_tecla(int codigo, bool abajo);
void plat_symbian_soltar_teclas();
bool plat_symbian_salir_pedido();

#endif
