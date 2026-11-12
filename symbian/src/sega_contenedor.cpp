// El contenedor: la ventana de pantalla completa con el Direct Screen Access, el bucle de cuadros y las teclas.
//
// El bucle es un CIdle: corre cada vez que el planificador no tiene otra cosa que hacer (las teclas y el servidor de
// ventanas pasan antes), sin el reloj de CPeriodic, que redondea a su tic (1/64 s) y deja los cuadros en escalones.
// El ritmo lo pone el juego con el reloj de la consola: cuando todavia no toca un cuadro, no se presenta y se espera
// un milisegundo.
//
// La imagen la dibuja pantalla_symbian.cpp con el contexto del DSA, directo en la pantalla; presentar es pedirle a la
// pantalla que tome lo dibujado (Update). Si otra ventana tapa (un aviso, el menu de tareas) el sistema avisa
// (AbortNow) y no se dibuja hasta que vuelve (Restart).
#include "segaw3d.h"
#include "plataforma.h"
#include "plataforma_symbian.h"
#include "pantalla_symbian.h"
#include "juego.h"
#include "w3dlog.h"
#include <eikenv.h>
#include <akndef.h>
#include <e32base.h>

void CSegaContenedor::ConstructL() {
    CreateWindowL();
    // la pantalla en 16 bits si se puede: la imagen de la consola es RGB565 (sin convertir) y se escriben la mitad de
    // bytes en la memoria de la pantalla, que es lenta (lo que queda, al log: "pantalla: panel ...")
    const TInt pedido = Window().SetRequiredDisplayMode(EColor64K);
    w3dLogf("contenedor: modo de la ventana pedido 64K, queda %d", (int)pedido);
    SetExtentToWholeScreen();
    ActivateL();
    plat_symbian_iniciar();
    plat_symbian_pantalla(Size().iWidth, Size().iHeight);
    iDsa = CDirectScreenAccess::NewL(iCoeEnv->WsSession(), *iCoeEnv->ScreenDevice(), Window(), *this);
    iCoeEnv->WsSession().Flush();
    EmpezarDsa();
    juego_iniciar(22050);
    iJuegoIniciado = ETrue;
    iBucle = CIdle::NewL(CActive::EPriorityIdle);
    iBucle->Start(TCallBack(CSegaContenedor::Cuadro, this));
}

void CSegaContenedor::EmpezarDsa() {
    TRAPD(error, iDsa->StartL());
    if (error != KErrNone) {
        w3dLogf("contenedor: el DSA no arranco (%d)", (int)error);
        iDsaActivo = EFalse;
        pantalla_symbian_dsa(0);
        return;
    }
    iDsa->Gc()->SetClippingRegion(iDsa->DrawingRegion());
    iDsaActivo = ETrue;
    pantalla_symbian_dsa(iDsa);
    const TSize t = iDsa->ScreenDevice()->SizeInPixels();
    w3dLogf("contenedor: DSA en %dx%d (ventana %dx%d)", (int)t.iWidth, (int)t.iHeight, (int)Size().iWidth,
            (int)Size().iHeight);
}

void CSegaContenedor::AbortNow(RDirectScreenAccess::TTerminationReasons) {
    iDsa->Cancel();
    iDsaActivo = EFalse;
    pantalla_symbian_dsa(0);
}

void CSegaContenedor::Restart(RDirectScreenAccess::TTerminationReasons) {
    Window().SetRequiredDisplayMode(EColor64K);
    EmpezarDsa();
    juego_redibujar();
}

CSegaContenedor::~CSegaContenedor() {
    delete iBucle;
    if (iJuegoIniciado) juego_terminar();
    pantalla_symbian_dsa(0);
    delete iDsa;
    plat_symbian_terminar();
}

TInt CSegaContenedor::Cuadro(TAny* aContenedor) {
    static_cast<CSegaContenedor*>(aContenedor)->UnCuadro();
    return 1;   // (el CIdle lo vuelve a llamar)
}

void CSegaContenedor::UnCuadro() {
    plat_symbian_cuadro();
    if (juego_cuadro()) {
        const uint64_t antes = plat_ahora_us();
        if (iDsaActivo) pantalla_symbian_presentar();
        juego_swap_us(plat_ahora_us() - antes);
        // que no se apague la luz de la pantalla
        if (++iCuadros % 100 == 0) User::ResetInactivityTime();
    } else {
        User::AfterHighRes(1000);
    }
    if (plat_symbian_salir_pedido()) {
        iBucle->Cancel();
        static_cast<CSegaAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleCommandL(EEikCmdExit);
    }
}

void CSegaContenedor::Pausar(TBool aPausar) {
    w3dLogf("contenedor: %s", aPausar ? "al fondo" : "al frente");
    if (!iBucle || aPausar == iPausado) return;
    iPausado = aPausar;
    plat_symbian_soltar_teclas();
    if (iJuegoIniciado) juego_pausa(aPausar != EFalse);
    if (aPausar) {
        iBucle->Cancel();
    } else {
        // (de vuelta al frente: la pantalla en 16 bits otra vez -el sistema la deja como la tenia la otra app-; si el
        // sistema no devolvio el DSA solo, se pide de nuevo; y todo se dibuja otra vez)
        Window().SetRequiredDisplayMode(EColor64K);
        if (!iDsaActivo) EmpezarDsa();
        juego_redibujar();
        if (!iBucle->IsActive()) iBucle->Start(TCallBack(CSegaContenedor::Cuadro, this));
    }
}

// las teclas del juego: apretada / soltada (las repeticiones de EEventKey no hacen falta). Las demas (la roja, la de
// menu) siguen al sistema
TKeyResponse CSegaContenedor::OfferKeyEventL(const TKeyEvent& aEvento, TEventCode aTipo) {
    if (aTipo == EEventKeyDown) return plat_symbian_tecla(aEvento.iScanCode, true) ? EKeyWasConsumed : EKeyWasNotConsumed;
    if (aTipo == EEventKeyUp) return plat_symbian_tecla(aEvento.iScanCode, false) ? EKeyWasConsumed : EKeyWasNotConsumed;
    return plat_symbian_tecla(aEvento.iScanCode, true) ? EKeyWasConsumed : EKeyWasNotConsumed;
}

void CSegaContenedor::SizeChanged() {
    plat_symbian_pantalla(Size().iWidth, Size().iHeight);
}

void CSegaContenedor::HandleResourceChange(TInt aTipo) {
    CCoeControl::HandleResourceChange(aTipo);
    if (aTipo == KEikDynamicLayoutVariantSwitch) SetExtentToWholeScreen();
}

// (el sistema repinta la ventana: en negro, y el bucle vuelve a dibujar todo encima en el cuadro siguiente)
void CSegaContenedor::Draw(const TRect& aRect) const {
    CWindowGc& gc = SystemGc();
    gc.SetBrushColor(KRgbBlack);
    gc.Clear(aRect);
    juego_redibujar();
}

// (el N95: la salida de TV la maneja el sistema; nunca se pregunta)
void plat_tv_elegida(int) {}
