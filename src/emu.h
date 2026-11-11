// ---------------------------------------------------------------------------------------------------------------
//  El emulador de Mega Drive / Genesis (y Master System / Game Gear): el nucleo de PicoDrive (picodrive/pico) visto
//  desde el juego. Cargar una ROM, correr un cuadro con los 4 mandos, la imagen de ese cuadro (RGB565) y el sonido.
//  En C (emu.c incluye los encabezados de PicoDrive, que son de C).
//
//  Los mandos: dos de 6 botones, o el TEAM PLAYER de Sega (hasta 4 jugadores) en los juegos que lo soportan (Mega
//  Bomberman, Gauntlet IV, ...: lo dice su cabecera). Un juego que no lo soporta no entiende el adaptador y no
//  responde a ningun boton.
// ---------------------------------------------------------------------------------------------------------------
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// los botones del mando de Mega Drive (los bits de PicoIn.pad: MXYZ SACB RLDU)
enum {
    MD_ARRIBA = 1 << 0, MD_ABAJO = 1 << 1, MD_IZQ = 1 << 2, MD_DER = 1 << 3,
    MD_B = 1 << 4, MD_C = 1 << 5, MD_A = 1 << 6, MD_START = 1 << 7,
    MD_Z = 1 << 8, MD_Y = 1 << 9, MD_X = 1 << 10, MD_MODE = 1 << 11
};

// como se enchufan los mandos: dos de 6 botones, el Team Player de Sega en el puerto 1 o en el 2 (4 jugadores) o el
// 4 Way Play de EA (4 jugadores, los juegos de EA)
enum EmuEntrada { EMU_DOS_MANDOS, EMU_TEAM_PLAYER_1, EMU_TEAM_PLAYER_2, EMU_EA_4WAY };

// arranca el nucleo: 'hzSalida' = la frecuencia de la salida de audio (fija), 'hz' = la del sonido de la consola
void emu_iniciar(int hzSalida, int hz);
void emu_terminar(void);
// la ROM (.md .bin .gen .smd .sms .gg o un .zip con una adentro). 0 = no se pudo
int emu_cargar(const char* ruta);
void emu_entrada(int entrada);   // EmuEntrada
// el cartucho cargado soporta el Team Player (un '4' entre los perifericos de su cabecera, en 0x190)
int emu_soporta_team_player(void);
void emu_reiniciar(void);
// un cuadro de la consola (1/60 s; 1/50 en las PAL). 'dibujar' 0: corre igual pero no arma la imagen (para alcanzar
// al reloj cuando el telefono se atrasa)
void emu_cuadro(const uint16_t pads[4], int dibujar);
// los cuadros por segundo de la consola (60 NTSC, 50 PAL)
int emu_hz_video(void);

// la imagen del ultimo cuadro dibujado: ancho x alto pixeles RGB565, filas seguidas (paso = ancho)
const uint16_t* emu_imagen(int* ancho, int* alto);
// el numero de la ultima imagen (cambia cuando hay una nueva)
uint32_t emu_imagen_numero(void);

// EL SONIDO (en el hilo de audio): suma 'frames' muestras estereo al acumulador de la mezcla (W3dAudioFuente),
// estirando o apretando un poco para que la cola no se vacie ni se llene
void emu_sonido_mezclar(int* acumulador, int frames);
// cuanto sonido hay esperando (muestras estereo)
int emu_sonido_en_cola(void);
// cuanto sobra (+) o falta (-) en la cola, promediado (muestras de la consola): el juego acomoda la velocidad de la
// consola para que la cola no se vacie ni se llene (el sonido se lee a paso fijo)
int emu_sonido_desvio(void);
// el volumen del sonido de la consola: 0..256 (el volumen general del Core no le llega a W3dAudioFuente)
void emu_sonido_volumen(int v);
// (las pruebas en la PC) guarda el sonido de la consola en un archivo PCM crudo: estereo, 16 bits
void emu_sonido_grabar(const char* ruta);
// la calidad del sonido: 'filtro' = un pasabajos (suaviza lo agudo; casi no cuesta); 'fmAlta' = el FM a su frecuencia
// original (~53 kHz) y bajado con un filtro FIR (sin el aliasing de los agudos, como los anillos de Sonic; ~2,4 veces
// el trabajo del FM). Vale en el momento, con o sin juego cargado
void emu_sonido_calidad(int filtro, int fmAlta);
// otra frecuencia para el sonido de la consola (22050 / 32000 / 44100: mas alta suena mejor y cuesta mas). La salida
// de audio no cambia: la cola convierte
void emu_sonido_hz(int hz);
// el dibujo rapido de PicoDrive (por tiles, el cuadro entero de una vez): mucho mas barato, pero sin los efectos de
// mitad de pantalla (paletas o scroll por linea). Vale en el momento
void emu_dibujo_rapido(int si);

// los mensajes del nucleo (lprintf): los pone el juego (al log)
void emu_mensaje(const char* texto);

#ifdef __cplusplus
}
#endif
