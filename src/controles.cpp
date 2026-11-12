#include "controles.h"
#include "emu.h"
#include "menu.h"
#include "plataforma.h"
#include <stdio.h>
#include <string.h>

using namespace w3dEngine;

namespace {
// lo que aprieta cada boton de Mega Drive: hasta dos teclas del telefono (Tecla de plataforma.h) y un boton del mando
// (MandoBoton, CTRL_LT o CTRL_RT)
struct Asignacion {
    int tecla[2];
    int mando;
};
Asignacion gCtrl[CTRL_JUGADORES][CTRL_BOTONES];
int gTipo[CTRL_JUGADORES] = { MandoPS3, MandoPS3, MandoPS3, MandoPS3 };
bool gEsperaSoltar = true;

// los botones de Mega Drive, en el orden del menu
const uint16_t BIT[CTRL_BOTONES] = { MD_ARRIBA, MD_ABAJO, MD_IZQ, MD_DER, MD_A, MD_B, MD_C,
                                     MD_X, MD_Y, MD_Z, MD_START, MD_MODE };

// DE FABRICA: el telefono (solo el jugador 1) y el mando: OK y 1 = A, 2 = B, 3 = C, 4 5 6 = X Y Z, la tecla de abajo
// a la izquierda y el 0 = Start, 7 = Mode; en el mando cuadrado / cruz / circulo = A B C, L1 / triangulo / R1 = X Y Z,
// L2 = Mode
const Asignacion FABRICA_1[CTRL_BOTONES] = {
    { { TK_ARRIBA, CTRL_NADA }, MandoArriba },   { { TK_ABAJO, CTRL_NADA }, MandoAbajo },
    { { TK_IZQ, CTRL_NADA }, MandoIzquierda },   { { TK_DER, CTRL_NADA }, MandoDerecha },
    { { TK_CENTRO, TK_1 }, MandoX },             { { TK_2, CTRL_NADA }, MandoA },
    { { TK_3, CTRL_NADA }, MandoB },             { { TK_4, CTRL_NADA }, MandoL1 },
    { { TK_5, CTRL_NADA }, MandoY },             { { TK_6, CTRL_NADA }, MandoR1 },
    { { TK_SOFT_IZQ, TK_0 }, MandoStart },       { { TK_7, CTRL_NADA }, CTRL_LT },
};

const Asignacion& de(int jugador, int boton, bool deFabrica) {
    static Asignacion fabrica;
    if (!deFabrica) return gCtrl[jugador][boton];
    fabrica = FABRICA_1[boton];
    if (jugador != 0) fabrica.tecla[0] = fabrica.tecla[1] = CTRL_NADA;   // (el telefono es del jugador 1)
    return fabrica;
}

bool fuente_apretada(const MandoEstado& e, int s) {
    if (s >= 0 && s < MandoBotones) return e.botones[s];
    if (s == CTRL_LT) return e.ejes[MandoLT] > 0.5f;
    if (s == CTRL_RT) return e.ejes[MandoRT] > 0.5f;
    return false;
}

const char* tr(const char* es, const char* en) { return menu_tr(es, en); }

const char* nombre_tecla(int t) {
    switch (t) {
    case TK_ARRIBA: return tr("Arriba", "Up");
    case TK_ABAJO: return tr("Abajo", "Down");
    case TK_IZQ: return tr("Izq", "Left");
    case TK_DER: return tr("Der", "Right");
    case TK_CENTRO: return "OK";
    case TK_0: return "0";
    case TK_1: return "1";
    case TK_2: return "2";
    case TK_3: return "3";
    case TK_4: return "4";
    case TK_5: return "5";
    case TK_6: return "6";
    case TK_7: return "7";
    case TK_8: return "8";
    case TK_9: return "9";
    case TK_ASTERISCO: return "*";
    case TK_NUMERAL: return "#";
    case TK_SOFT_IZQ: return tr("Soft izq", "L soft");
    case TK_BORRAR: return "C";
    case TK_VERDE: return tr("Verde", "Green");
    case TK_LAPIZ: return tr("Lapiz", "Pencil");
    default: return "?";
    }
}

const char* nombre_mando(int s, int tipo) {
    if (s == MandoArriba) return tr("Arriba", "Up");
    if (s == MandoAbajo) return tr("Abajo", "Down");
    if (s == MandoIzquierda) return tr("Izq", "Left");
    if (s == MandoDerecha) return tr("Der", "Right");
    if (tipo == MandoWii) {
        // (el driver: A = cruz, B = R2, 1 = cuadrado, 2 = circulo, - = select, + = start, Home = PS)
        static const char* const wii[CTRL_FUENTES_MANDO] = { "A", "2", "1", "Y", "-", "Home", "+", "L3", "R3",
                                                             "L1", "R1", "", "", "", "", "LT", "B" };
        return s >= 0 && s < CTRL_FUENTES_MANDO ? wii[s] : "?";
    }
    if (tipo == MandoGenerico) {
        static const char* const xbox[CTRL_FUENTES_MANDO] = { "A", "B", "X", "Y", "Back", "Guia", "Start", "L3", "R3",
                                                              "LB", "RB", "", "", "", "", "LT", "RT" };
        return s >= 0 && s < CTRL_FUENTES_MANDO ? xbox[s] : "?";
    }
    static const char* const ps[CTRL_FUENTES_MANDO] = { "Cruz", "Circulo", "Cuadrado", "Triangulo", "Select", "PS",
                                                        "Start", "L3", "R3", "L1", "R1", "", "", "", "", "L2", "R2" };
    static const char* const psEn[CTRL_FUENTES_MANDO] = { "Cross", "Circle", "Square", "Triangle", "Select", "PS",
                                                          "Start", "L3", "R3", "L1", "R1", "", "", "", "", "L2", "R2" };
    if (s < 0 || s >= CTRL_FUENTES_MANDO) return "?";
    return menu_en_ingles() ? psEn[s] : ps[s];
}

// que se aprieta ahora (para esperar una tecla): la tecla del telefono o el boton de algun mando; -1 = nada. 'cancela'
// = la tecla de abajo a la derecha o PS / Home
void leer_todo(int* tecla, int* mando, int* tipo, bool* cancela) {
    *tecla = *mando = -1;
    *tipo = MandoPS3;
    *cancela = plat_tecla(TK_SOFT_DER);
    for (int t = 0; t < TECLAS; t++)
        if (t != TK_SOFT_DER && plat_tecla(t)) {
            *tecla = t;
            break;
        }
    for (int i = 0; i < MandosMax; i++) {
        MandoEstado e;
        if (!plat_mando(i, e)) continue;
        if (e.botones[MandoGuia]) *cancela = true;
        for (int s = 0; s < CTRL_FUENTES_MANDO && *mando < 0; s++)
            if (s != MandoGuia && fuente_apretada(e, s)) {
                *mando = s;
                *tipo = e.tipo;
            }
    }
}
}   // namespace

void controles_defecto(int jugador) {
    for (int j = 0; j < CTRL_JUGADORES; j++)
        if (jugador < 0 || jugador == j)
            for (int b = 0; b < CTRL_BOTONES; b++) gCtrl[j][b] = de(j, b, true);
}

void controles_archivo(const char* ruta, bool escribir) {
    FILE* f = fopen(ruta, escribir ? "w" : "r");
    if (!f) return;
    if (escribir) {
        for (int j = 0; j < CTRL_JUGADORES; j++)
            for (int b = 0; b < CTRL_BOTONES; b++)
                fprintf(f, "%d %d %d %d %d\n", j + 1, b, gCtrl[j][b].tecla[0], gCtrl[j][b].tecla[1], gCtrl[j][b].mando);
    } else {
        int j, b, t0, t1, m;
        while (fscanf(f, "%d %d %d %d %d", &j, &b, &t0, &t1, &m) == 5) {
            if (j < 1 || j > CTRL_JUGADORES || b < 0 || b >= CTRL_BOTONES) continue;
            Asignacion& a = gCtrl[j - 1][b];
            a.tecla[0] = t0 >= 0 && t0 < TECLAS && t0 != TK_SOFT_DER ? t0 : CTRL_NADA;
            a.tecla[1] = t1 >= 0 && t1 < TECLAS && t1 != TK_SOFT_DER ? t1 : CTRL_NADA;
            a.mando = m >= 0 && m < CTRL_FUENTES_MANDO && m != MandoGuia ? m : CTRL_NADA;
        }
    }
    fclose(f);
}

uint16_t controles_teclas(int jugador, bool deFabrica) {
    uint16_t p = 0;
    if (jugador < 0 || jugador >= CTRL_JUGADORES) return 0;
    for (int b = 0; b < CTRL_BOTONES; b++) {
        const Asignacion& a = de(jugador, b, deFabrica);
        if ((a.tecla[0] >= 0 && plat_tecla(a.tecla[0])) || (a.tecla[1] >= 0 && plat_tecla(a.tecla[1]))) p |= BIT[b];
    }
    return p;
}

uint16_t controles_mando(int jugador, const MandoEstado& e, bool deFabrica) {
    uint16_t p = 0;
    if (jugador < 0 || jugador >= CTRL_JUGADORES) return 0;
    for (int b = 0; b < CTRL_BOTONES; b++)
        if (fuente_apretada(e, de(jugador, b, deFabrica).mando)) p |= BIT[b];
    // (el stick izquierdo: siempre la cruceta)
    if (e.ejes[MandoLY] < -0.5f) p |= MD_ARRIBA;
    if (e.ejes[MandoLY] > 0.5f) p |= MD_ABAJO;
    if (e.ejes[MandoLX] < -0.5f) p |= MD_IZQ;
    if (e.ejes[MandoLX] > 0.5f) p |= MD_DER;
    return p;
}

bool controles_select_libre(int jugador) {
    if (jugador < 0 || jugador >= CTRL_JUGADORES) return true;
    for (int b = 0; b < CTRL_BOTONES; b++)
        if (gCtrl[jugador][b].mando == MandoSelect) return false;
    return true;
}

void controles_tipo(int jugador, int tipo) {
    if (jugador >= 0 && jugador < CTRL_JUGADORES) gTipo[jugador] = tipo;
}

const char* controles_nombre(int boton) {
    static const char* const es[CTRL_BOTONES] = { "Arriba", "Abajo", "Izquierda", "Derecha", "A", "B", "C",
                                                  "X", "Y", "Z", "Start", "Mode" };
    static const char* const en[CTRL_BOTONES] = { "Up", "Down", "Left", "Right", "A", "B", "C",
                                                  "X", "Y", "Z", "Start", "Mode" };
    if (boton < 0 || boton >= CTRL_BOTONES) return "?";
    return menu_en_ingles() ? en[boton] : es[boton];
}

// "OK,1 / Cuadrado": las teclas del telefono, y despues de la barra el boton del mando
void controles_texto(int jugador, int boton, bool conTeclas, char* s, int n) {
    s[0] = 0;
    if (jugador < 0 || jugador >= CTRL_JUGADORES || boton < 0 || boton >= CTRL_BOTONES) return;
    const Asignacion& a = gCtrl[jugador][boton];
    char teclas[24] = "";
    if (!conTeclas) {
        snprintf(s, n, "%s", a.mando >= 0 ? nombre_mando(a.mando, gTipo[jugador]) : "-");
        return;
    }
    if (a.tecla[0] >= 0) snprintf(teclas, sizeof(teclas), "%s", nombre_tecla(a.tecla[0]));
    if (a.tecla[1] >= 0)
        snprintf(teclas + strlen(teclas), sizeof(teclas) - strlen(teclas), "%s%s", teclas[0] ? "," : "",
                 nombre_tecla(a.tecla[1]));
    const char* mando = a.mando >= 0 ? nombre_mando(a.mando, gTipo[jugador]) : "";
    if (!teclas[0] && !mando[0]) snprintf(s, n, "-");
    else if (!mando[0]) snprintf(s, n, "%s", teclas);
    else if (!teclas[0]) snprintf(s, n, "%s", mando);
    else snprintf(s, n, "%s / %s", teclas, mando);
}

void controles_esperar_empezar() { gEsperaSoltar = true; }

int controles_esperar(int jugador, int boton) {
    if (jugador < 0 || jugador >= CTRL_JUGADORES || boton < 0 || boton >= CTRL_BOTONES) return -1;
    int tecla, mando, tipo;
    bool cancela;
    leer_todo(&tecla, &mando, &tipo, &cancela);
    if (cancela) return -1;
    // (primero que se suelte todo: el OK que eligio el boton no es la tecla nueva)
    if (gEsperaSoltar) {
        if (tecla < 0 && mando < 0) gEsperaSoltar = false;
        return 0;
    }
    Asignacion* fila = gCtrl[jugador];
    if (tecla >= 0) {
        // (la tecla deja de hacer lo que hacia en este jugador: una tecla, un boton)
        for (int b = 0; b < CTRL_BOTONES; b++)
            for (int k = 0; k < 2; k++)
                if (fila[b].tecla[k] == tecla) fila[b].tecla[k] = CTRL_NADA;
        fila[boton].tecla[0] = tecla;
        fila[boton].tecla[1] = CTRL_NADA;
        return 1;
    }
    if (mando >= 0) {
        for (int b = 0; b < CTRL_BOTONES; b++)
            if (fila[b].mando == mando) fila[b].mando = CTRL_NADA;
        fila[boton].mando = mando;
        gTipo[jugador] = tipo;
        return 1;
    }
    return 0;
}
