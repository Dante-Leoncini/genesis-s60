// El Direct Screen Access del contenedor para la pantalla del N95 (pantalla_symbian.cpp): el contenedor lo pasa al
// arrancar y cada vez que vuelve (NULL mientras la pantalla esta tapada por otra ventana).
#ifndef PANTALLA_SYMBIAN_H
#define PANTALLA_SYMBIAN_H

class CDirectScreenAccess;
void pantalla_symbian_dsa(CDirectScreenAccess* dsa);
// lo dibujado, a la pantalla (el swap): el panel del N95 solo muestra lo que se le dice que cambio. Lo dibujado con
// el contexto del DSA lo anota el sistema; lo escrito directo en la memoria hay que decirlo (el rectangulo)
void pantalla_symbian_presentar();

#endif
