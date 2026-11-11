// La plataforma del emulador en el N95 (src/plataforma.h): el reloj, la carpeta de datos, la pantalla, las teclas,
// las de volumen y los mandos.
//
// Los datos van en \segaw3d\datos\ en la unidad del ejecutable (E:, la tarjeta): una carpeta publica, para que se pueda
// cambiar config.txt (o poner una ROM) por el cable sin reinstalar.
#include "plataforma.h"
#include "plataforma_symbian.h"
#include "juego.h"
#include "w3dlog.h"
#include <e32std.h>
#include <e32keys.h>
#include <hal.h>
#include <remconcoreapitarget.h>
#include <remconcoreapitargetobserver.h>
#include <remconinterfaceselector.h>
#include <stdio.h>

namespace {
bool gTeclas[TECLAS];
int gAncho = 320, gAlto = 240;
char gDatos[64] = "e:/segaw3d/datos/";
bool gSalir = false;
// el reloj: el contador rapido del sistema (de 32 bits: se acumula la diferencia de cada lectura)
TInt gFrecuencia = 0;
TBool gSube = ETrue;
TUint32 gContador = 0;
uint64_t gTics = 0;

// LAS TECLAS DE VOLUMEN (las del costado): en S60 no llegan como teclas sino por el control remoto del sistema
// (RemCon). Un toque cambia un paso; mantenida, repite
class CTeclasVolumen : public CBase, public MRemConCoreApiTargetObserver {
public:
    static CTeclasVolumen* NewL() {
        CTeclasVolumen* t = new (ELeave) CTeclasVolumen;
        CleanupStack::PushL(t);
        t->iSelector = CRemConInterfaceSelector::NewL();
        t->iObjetivo = CRemConCoreApiTarget::NewL(*t->iSelector, *t);
        t->iSelector->OpenTargetL();
        CleanupStack::Pop(t);
        return t;
    }
    ~CTeclasVolumen() { delete iSelector; }   // (el selector es el duenio del objetivo)
    void MrccatoCommand(TRemConCoreApiOperationId aOperacion, TRemConCoreApiButtonAction aAccion) {
        int paso = 0;
        TRequestStatus estado;
        if (aOperacion == ERemConCoreApiVolumeUp) {
            paso = 1;
            iObjetivo->VolumeUpResponse(estado, KErrNone);
            User::WaitForRequest(estado);
        } else if (aOperacion == ERemConCoreApiVolumeDown) {
            paso = -1;
            iObjetivo->VolumeDownResponse(estado, KErrNone);
            User::WaitForRequest(estado);
        }
        if (!paso) return;
        if (aAccion == ERemConCoreApiButtonRelease) { iMantenida = 0; return; }
        juego_volumen(paso);
        iMantenida = aAccion == ERemConCoreApiButtonPress ? paso : 0;
        iProxima = plat_ahora_us() + 500000;
    }
    // (cada cuadro) mantenida: un paso cada 0,15 s despues del primer medio segundo
    void Repetir() {
        if (!iMantenida || plat_ahora_us() < iProxima) return;
        juego_volumen(iMantenida);
        iProxima = plat_ahora_us() + 150000;
    }
private:
    CTeclasVolumen() : iSelector(0), iObjetivo(0), iMantenida(0), iProxima(0) {}
    CRemConInterfaceSelector* iSelector;
    CRemConCoreApiTarget* iObjetivo;
    int iMantenida;
    uint64_t iProxima;
};
CTeclasVolumen* gVolumen = 0;
}

void plat_symbian_iniciar() {
    // la carpeta de datos, en la unidad del ejecutable (Open C entiende "e:/segaw3d/..."), y el log al lado (uno
    // nuevo cada vez)
    const TFileName exe = RProcess().FileName();
    const char unidad = exe.Length() > 0 ? (char)(exe[0] | 0x20) : 'e';
    snprintf(gDatos, sizeof(gDatos), "%c:/segaw3d/datos/", unidad);
    char log[32];
    snprintf(log, sizeof(log), "%c:\\segaw3d\\segaw3d.log", unidad);
    w3dLogRuta(log);
    w3dLogReset();
    HAL::Get(HALData::EFastCounterFrequency, gFrecuencia);
    TInt sube = 1;
    HAL::Get(HALData::EFastCounterCountsUp, sube);
    gSube = sube != 0;
    gContador = User::FastCounter();
    TRAPD(errorVolumen, gVolumen = CTeclasVolumen::NewL());
    if (errorVolumen != KErrNone) {
        gVolumen = 0;
        w3dLogf("plataforma: sin las teclas de volumen (RemCon: %d)", (int)errorVolumen);
    }
    w3dLogf("plataforma: datos en %s; reloj de %d Hz", gDatos, (int)gFrecuencia);
}

void plat_symbian_terminar() {
    delete gVolumen;
    gVolumen = 0;
    w3dEngine::MandosCerrar();
    w3dLogBajar();
}

void plat_symbian_cuadro() {
    if (gVolumen) gVolumen->Repetir();
}

void plat_symbian_pantalla(int ancho, int alto) {
    gAncho = ancho;
    gAlto = alto;
}

// el codigo de exploracion de cada tecla del N95 (iScanCode) -> la del juego
bool plat_symbian_tecla(int codigo, bool abajo) {
    int t = -1;
    if (codigo >= '0' && codigo <= '9') {
        t = TK_0 + (codigo - '0');
    } else {
        switch (codigo) {
        case EStdKeyUpArrow: t = TK_ARRIBA; break;
        case EStdKeyDownArrow: t = TK_ABAJO; break;
        case EStdKeyLeftArrow: t = TK_IZQ; break;
        case EStdKeyRightArrow: t = TK_DER; break;
        case EStdKeyDevice3:          // (el centro de la cruceta)
        case EStdKeyEnter: t = TK_CENTRO; break;
        case '*':
        case EStdKeyNkpAsterisk: t = TK_ASTERISCO; break;
        case EStdKeyHash: t = TK_NUMERAL; break;
        case EStdKeyDevice0: t = TK_SOFT_IZQ; break;   // (164: la de abajo a la izquierda de la pantalla)
        case EStdKeyDevice1: t = TK_SOFT_DER; break;   // (165: la de la derecha)
        case EStdKeyBackspace: t = TK_BORRAR; break;   // (la C)
        case EStdKeyYes: t = TK_VERDE; break;          // (la de llamar)
        case EStdKeyLeftShift:
        case EStdKeyRightShift: t = TK_LAPIZ; break;   // (la del lapiz)
        default: break;
        }
    }
    if (t < 0) return false;
    gTeclas[t] = abajo;
    return true;
}

void plat_symbian_soltar_teclas() {
    for (int t = 0; t < TECLAS; t++) gTeclas[t] = false;
}

bool plat_symbian_salir_pedido() { return gSalir; }

// ------------------------------------------------------------------------------------------- plataforma.h
uint64_t plat_ahora_us() {
    if (gFrecuencia <= 0) return (uint64_t)User::NTickCount() * 1000;
    const TUint32 ahora = User::FastCounter();
    gTics += gSube ? (TUint32)(ahora - gContador) : (TUint32)(gContador - ahora);
    gContador = ahora;
    return gTics * (uint64_t)1000000 / (uint64_t)gFrecuencia;
}

const char* plat_carpeta_datos() { return gDatos; }

void plat_pantalla(int& ancho, int& alto) {
    ancho = gAncho;
    alto = gAlto;
}

bool plat_tecla(int t) { return t >= 0 && t < TECLAS && gTeclas[t]; }

bool plat_mando(int i, w3dEngine::MandoEstado& e) { return w3dEngine::MandoLeer(i, e); }

void plat_salir() { gSalir = true; }

bool plat_espanol() {
    const TLanguage l = User::Language();
    return l == ELangSpanish || l == ELangLatinAmericanSpanish;
}

#ifndef SEGA_BELLE
// (el N95 no tiene pantalla tactil ni GPU para escalar: la imagen va 1:1 por CPU; en el N8 los pone el contenedor)
bool plat_es_tactil() { return false; }
int plat_toques(Toque*, int) { return 0; }
bool plat_escala() { return false; }
#endif
