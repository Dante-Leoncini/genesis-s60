// El contenedor de Symbian^3 / Anna / Belle (N8): la ventana de pantalla completa con OpenGL ES 2.0, el bucle de
// cuadros, las teclas, LOS DEDOS (varios a la vez) y LA PANTALLA EXTERNA.
//
// El bucle es un CIdle: corre cada vez que el planificador no tiene otra cosa que hacer. El ritmo lo pone el juego con
// el reloj de la consola: cuando todavia no toca un cuadro, no se presenta y se espera un milisegundo. El swap espera el
// refresco de la pantalla (intervalo 1: en Symbian^3 sin esperar, el compositor agarra cuadros a medio dibujar).
//
// LA TV (la salida RCA de 480 lineas o la HDMI de 720p): wserv la muestra como la pantalla 1. Una vez por segundo se
// revisa si aparecio (con tamano y con un cable de video, que confirma el AccMonitor: el N8 puede tener la pantalla 1
// configurada aunque no haya nada enchufado) y se pregunta que mostrar, como en Half-Life y en Whisk3D:
//   - clonar: el sistema copia la pantalla del telefono en la TV (no hace falta nada mas)
//   - solo la tele: el juego en la TV, a su resolucion; el telefono en negro
//   - tele + controles: el juego en la TV y en el telefono los controles tactiles (el telefono es el mando)
// Para las dos ultimas, una ventana en la pantalla 1 con su superficie EGL, que comparte el contexto (las texturas ya
// subidas sirven para las dos). La ventana de la TV TIENE que ser un control de CONE: CONE trata el handle de cada
// ventana y window group de la sesion como un CCoeControl* (con un RWindow pelado, KERN-EXEC 3 en el N8).
#include "segaw3d.h"
#include "plataforma.h"
#include "plataforma_symbian.h"
#include "juego.h"
#include "menu.h"
#include "tactil.h"
#include "w3dlog.h"
#include <eikenv.h>
#include <akndef.h>
#include <e32base.h>
#include <hal.h>
#include <accmonitor.h>
#include <akngloballistquery.h>
#include <badesca.h>
#include <GLES2/gl2.h>

namespace {
// ------------------------------------------------------------------------------------------------- los dedos
const TInt KMaxDedos = 10;
Toque gDedos[KMaxDedos];
TInt gDedosN = 0;
TInt gTactil = -1;   // (-1 = todavia no se pregunto al HAL)

// ------------------------------------------------------------------------------------------------- la TV
class CVentanaTv : public CCoeControl {
public:
    void ConstruirL(RWindowGroup& aGrupo, const TRect& aRect) {
        CreateWindowL(aGrupo);
        SetRect(aRect);
        Window().SetBackgroundColor(KRgbBlack);
        ActivateL();
    }
    RWindow& Ventana() { return Window(); }
private:
    void Draw(const TRect&) const {}
};

struct Tv {
    TBool enchufada;        // ya se pregunto que mostrar
    TInt pantW, pantH;      // lo que mide la pantalla de la TV
    TInt w, h;              // la superficie (puede ser mas chica si la memoria grafica no alcanza)
    CWsScreenDevice* dev;
    RWindowGroup* grupo;
    CVentanaTv* ventana;
    EGLSurface sup;
};
Tv gTv = { EFalse, 0, 0, 0, 0, 0, 0, 0, EGL_NO_SURFACE };
CAccMonitor* gAcc = 0;
TUint gUltimaRevision = 0;
TInt gUltimasPantallas = -1, gUltimoCable = -2;

// hay un cable de video (HDMI o el de video compuesto)? -1 = no se puede saber
TInt CableDeVideo() {
    if (!gAcc) return -1;
    RConnectedAccessories lista;
    TInt hay = 0;
    TRAPD(error, gAcc->GetConnectedAccessoriesL(lista));
    if (error != KErrNone) {
        lista.ResetAndDestroy();
        return -1;
    }
    for (TInt i = 0; i < lista.Count(); i++)
        if (lista[i]->Exists(KAccMonHDMI) || lista[i]->Exists(KAccMonVideoOut)) hay = 1;
    lista.ResetAndDestroy();
    return hay;
}

void SoltarSuperficieTv(EGLDisplay aDpy, EGLSurface aTelefono, EGLContext aCtx) {
    if (gTv.sup != EGL_NO_SURFACE) {
        eglMakeCurrent(aDpy, aTelefono, aTelefono, aCtx);
        eglDestroySurface(aDpy, gTv.sup);
        gTv.sup = EGL_NO_SURFACE;
    }
    delete gTv.ventana;
    gTv.ventana = 0;
    if (gTv.grupo) {
        gTv.grupo->Close();
        delete gTv.grupo;
        gTv.grupo = 0;
    }
}
}

// ------------------------------------------------------------------------------------- la pregunta de la TV
// una lista global de Avkon (no necesita recursos), asincronica: la respuesta llega en RunL
class CPreguntaTv : public CActive {
public:
    CPreguntaTv(CSegaContenedor& aDuenio) : CActive(EPriorityStandard), iDuenio(aDuenio), iLista(0) {
        CActiveScheduler::Add(this);
    }
    ~CPreguntaTv() {
        Cancel();
        delete iLista;
    }
    void PreguntarL(TInt aW, TInt aH) {
        if (IsActive()) return;
        if (!iLista) iLista = CAknGlobalListQuery::NewL();
        CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(3);
        CleanupStack::PushL(items);
        // (textos cortos: con los largos la lista global no se muestra, KErrOverflow)
        TBuf<40> solo;
        const TBool en = menu_en_ingles();
        if (en) solo.Format(_L("TV only (%dx%d)"), aW, aH);
        else solo.Format(_L("Solo la tele (%dx%d)"), aW, aH);
        if (en) {
            items->AppendL(_L("Mirror the screen"));
            items->AppendL(solo);
            items->AppendL(_L("TV + controls"));
            iLista->SetHeadingL(_L("External display"));
        } else {
            items->AppendL(_L("Clonar la pantalla"));
            items->AppendL(solo);
            items->AppendL(_L("Tele + controles"));
            iLista->SetHeadingL(_L("Pantalla externa"));
        }
        iLista->ShowListQueryL(items, iStatus, 0);
        CleanupStack::PopAndDestroy(items);
        SetActive();
    }
private:
    void RunL() {
        const TInt elegido = iStatus.Int();
        w3dLogf("tv: se eligio %d", (int)elegido);
        iDuenio.ElegirSalida(elegido == 1 ? SALIDA_TV : elegido == 2 ? SALIDA_TV_CONTROLES : SALIDA_CLONAR);
    }
    void DoCancel() {
        if (iLista) iLista->CancelListQuery();
    }
    CSegaContenedor& iDuenio;
    CAknGlobalListQuery* iLista;
};

// --------------------------------------------------------------------------------------------- plataforma.h
bool plat_es_tactil() {
    if (gTactil < 0) {
        TInt lapiz = 0;
        gTactil = HAL::Get(HALData::EPen, lapiz) == KErrNone && lapiz ? 1 : 0;
    }
    return gTactil != 0;
}

int plat_toques(Toque* t, int max) {
    int n = gDedosN < max ? gDedosN : max;
    for (int i = 0; i < n; i++) t[i] = gDedos[i];
    return n;
}

bool plat_escala() { return true; }

// --------------------------------------------------------------------------------------------- el contenedor
void CSegaContenedor::ConstructL() {
    CreateWindowL();
    SetExtentToWholeScreen();
    // los dedos: arrastrar (sin esto solo llegan el apoyar y el soltar) y un evento por dedo (multitactil). Se pide
    // ANTES de activar la ventana
    EnableDragEvents();
    Window().EnableAdvancedPointers();
    ActivateL();
    IniciarEgl();
    plat_symbian_iniciar();
    plat_symbian_pantalla(Size().iWidth, Size().iHeight);
    TRAPD(errorAcc, gAcc = CAccMonitor::NewL());
    if (errorAcc != KErrNone) {
        gAcc = 0;
        w3dLogf("tv: sin AccMonitor (%d)", (int)errorAcc);
    }
    iPregunta = new (ELeave) CPreguntaTv(*this);
    juego_iniciar(22050);
    iJuegoIniciado = ETrue;
    iBucle = CIdle::NewL(CActive::EPriorityIdle);
    iBucle->Start(TCallBack(CSegaContenedor::Cuadro, this));
}

// la ventana con el contexto de GLES 2.0: la config con los bits de color de la ventana (si no coinciden, en Symbian^3
// la superficie da EGL_BAD_MATCH y la pantalla queda negra), sin profundidad (es una imagen plana)
void CSegaContenedor::IniciarEgl() {
    iEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (iEglDisplay == EGL_NO_DISPLAY) User::Panic(_L("eglGetDisplay"), 0);
    if (eglInitialize(iEglDisplay, NULL, NULL) == EGL_FALSE) User::Panic(_L("eglInitialize"), 0);
    eglBindAPI(EGL_OPENGL_ES_API);
    TInt bits = 32;
    switch (Window().DisplayMode()) {
    case EColor4K: bits = 12; break;
    case EColor64K: bits = 16; break;
    case EColor16M: bits = 24; break;
    default: bits = 32; break;
    }
    const EGLint atributos[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BUFFER_SIZE, bits,
                                 EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLint n = 0;
    if (eglChooseConfig(iEglDisplay, atributos, &iEglConfig, 1, &n) == EGL_FALSE || n == 0)
        User::Panic(_L("eglChooseConfig"), 0);
    iEglSurface = eglCreateWindowSurface(iEglDisplay, iEglConfig, &Window(), NULL);
    // (sin memoria grafica, EGL_BAD_ALLOC: otra app abierta la tiene; el sistema la libera cerrando las de fondo)
    for (TInt intento = 0; iEglSurface == EGL_NO_SURFACE && intento < 10; intento++) {
        w3dLogf("egl: eglCreateWindowSurface fallo (0x%x), reintento", (unsigned)eglGetError());
        User::After(500000);
        iEglSurface = eglCreateWindowSurface(iEglDisplay, iEglConfig, &Window(), NULL);
    }
    if (iEglSurface == EGL_NO_SURFACE) User::Panic(_L("eglCreateWindowSurface"), 0);
    const EGLint contexto[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    iEglContext = eglCreateContext(iEglDisplay, iEglConfig, EGL_NO_CONTEXT, contexto);
    if (iEglContext == EGL_NO_CONTEXT) User::Panic(_L("eglCreateContext"), 0);
    if (eglMakeCurrent(iEglDisplay, iEglSurface, iEglSurface, iEglContext) == EGL_FALSE)
        User::Panic(_L("eglMakeCurrent"), 0);
    eglSwapInterval(iEglDisplay, 1);
    w3dLogf("egl: ventana de %dx%d, %d bits", (int)Size().iWidth, (int)Size().iHeight, (int)bits);
}

CSegaContenedor::~CSegaContenedor() {
    delete iBucle;
    delete iPregunta;
    SoltarSuperficieTv(iEglDisplay, iEglSurface, iEglContext);
    delete gTv.dev;
    gTv.dev = 0;
    delete gAcc;
    gAcc = 0;
    // (las texturas se sueltan con el contexto todavia puesto)
    if (iJuegoIniciado) juego_terminar();
    plat_symbian_terminar();
    eglMakeCurrent(iEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(iEglDisplay, iEglSurface);
    eglDestroyContext(iEglDisplay, iEglContext);
    eglTerminate(iEglDisplay);
}

TInt CSegaContenedor::Cuadro(TAny* aContenedor) {
    static_cast<CSegaContenedor*>(aContenedor)->UnCuadro();
    return 1;   // (el CIdle lo vuelve a llamar)
}

void CSegaContenedor::UnCuadro() {
    plat_symbian_cuadro();
    RevisarTv();
    if (juego_cuadro()) {
        const uint64_t antes = plat_ahora_us();
        // la TV: la misma imagen en su superficie (el contexto es el mismo) y de vuelta al telefono
        if (gTv.sup != EGL_NO_SURFACE &&
            eglMakeCurrent(iEglDisplay, gTv.sup, gTv.sup, iEglContext) == EGL_TRUE) {
            juego_dibujar_tv(gTv.w, gTv.h);
            eglSwapBuffers(iEglDisplay, gTv.sup);
            eglMakeCurrent(iEglDisplay, iEglSurface, iEglSurface, iEglContext);
            glFinish();   // (el telefono va sin esperar el refresco: que el compositor no agarre medio cuadro)
        }
        eglSwapBuffers(iEglDisplay, iEglSurface);
        juego_swap_us(plat_ahora_us() - antes);
        if (++iCuadros % 100 == 0) User::ResetInactivityTime();
    } else {
        User::AfterHighRes(1000);
    }
    if (plat_symbian_salir_pedido()) {
        iBucle->Cancel();
        static_cast<CSegaAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleCommandL(EEikCmdExit);
    }
}

// LA TV: una vez por segundo, si se enchufo (se pregunta que mostrar) o se saco (todo vuelve al telefono)
void CSegaContenedor::RevisarTv() {
    const TUint ahora = User::NTickCount();
    if (gUltimaRevision != 0 && ahora - gUltimaRevision < 1000) return;
    gUltimaRevision = ahora;
    RWsSession& ws = iCoeEnv->WsSession();
    const TInt pantallas = ws.NumberOfScreens(), cable = CableDeVideo();
    if (pantallas != gUltimasPantallas || cable != gUltimoCable) {
        w3dLogf("tv: %d pantalla(s), cable de video: %s", (int)pantallas, cable < 0 ? "no se sabe" : cable ? "SI" : "no");
        gUltimasPantallas = pantallas;
        gUltimoCable = cable;
    }
    TSize tam(0, 0);
    if (pantallas > 1) {
        if (!gTv.dev) {
            gTv.dev = new CWsScreenDevice(ws);
            if (gTv.dev && gTv.dev->Construct(1) != KErrNone) {
                delete gTv.dev;
                gTv.dev = 0;
            }
        }
        if (gTv.dev) tam = gTv.dev->SizeInPixels();
    }
    const TBool hay = tam.iWidth > 0 && tam.iHeight > 0 && cable != 0;
    if (hay && !gTv.enchufada) {
        gTv.enchufada = ETrue;
        gTv.pantW = tam.iWidth;
        gTv.pantH = tam.iHeight;
        w3dLogf("tv: enchufada, %dx%d: se pregunta que mostrar", (int)tam.iWidth, (int)tam.iHeight);
        TRAPD(error, iPregunta->PreguntarL(tam.iWidth, tam.iHeight));
        if (error != KErrNone) w3dLogf("tv: no se pudo preguntar (%d)", (int)error);
    } else if (!hay && gTv.enchufada) {
        w3dLog("tv: desenchufada: todo al telefono");
        gTv.enchufada = EFalse;
        iPregunta->Cancel();
        ElegirSalida(SALIDA_TELEFONO);
        delete gTv.dev;
        gTv.dev = 0;
    }
}

// lo elegido en la pregunta: las salidas a la TV crean la ventana y la superficie en la pantalla 1
void CSegaContenedor::ElegirSalida(TInt aSalida) {
    if (aSalida != SALIDA_TV && aSalida != SALIDA_TV_CONTROLES) {
        SoltarSuperficieTv(iEglDisplay, iEglSurface, iEglContext);
        eglSwapInterval(iEglDisplay, 1);
        juego_salida(aSalida);
        return;
    }
    if (gTv.sup == EGL_NO_SURFACE) {
        RWsSession& ws = iCoeEnv->WsSession();
        if (!gTv.dev) return;
        gTv.ventana = new CVentanaTv();   // (primero el control: su puntero es el handle del grupo)
        gTv.grupo = new RWindowGroup(ws);
        if (!gTv.ventana || !gTv.grupo || gTv.grupo->Construct((TUint32)gTv.ventana, EFalse, gTv.dev) != KErrNone) {
            w3dLog("tv: no se pudo crear el window group en la pantalla 1");
            SoltarSuperficieTv(iEglDisplay, iEglSurface, iEglContext);
            juego_salida(SALIDA_CLONAR);
            return;
        }
        gTv.grupo->SetOrdinalPosition(0);
        // a la resolucion de la TV; si la memoria grafica no alcanza (EGL_BAD_ALLOC), a 3/4 y a la mitad, centrada
        static const TInt kNum[3] = { 1, 3, 1 }, kDen[3] = { 1, 4, 2 };
        for (TInt k = 0; k < 3 && gTv.sup == EGL_NO_SURFACE; k++) {
            gTv.w = gTv.pantW * kNum[k] / kDen[k];
            gTv.h = gTv.pantH * kNum[k] / kDen[k];
            const TRect r(TPoint((gTv.pantW - gTv.w) / 2, (gTv.pantH - gTv.h) / 2), TSize(gTv.w, gTv.h));
            if (k == 0) {
                TRAPD(error, gTv.ventana->ConstruirL(*gTv.grupo, r));
                if (error != KErrNone) {
                    w3dLogf("tv: no se pudo crear la ventana (%d)", (int)error);
                    break;
                }
            } else {
                gTv.ventana->SetRect(r);
            }
            ws.Flush();
            gTv.sup = eglCreateWindowSurface(iEglDisplay, iEglConfig, (EGLNativeWindowType)&gTv.ventana->Ventana(), NULL);
            if (gTv.sup == EGL_NO_SURFACE) {
                const EGLint error = eglGetError();
                w3dLogf("tv: la superficie de %dx%d no entro (0x%x)", (int)gTv.w, (int)gTv.h, (unsigned)error);
                if (error != EGL_BAD_ALLOC) break;
            }
        }
        if (gTv.sup == EGL_NO_SURFACE) {
            SoltarSuperficieTv(iEglDisplay, iEglSurface, iEglContext);
            juego_salida(SALIDA_CLONAR);
            return;
        }
        // la TV espera su refresco; el telefono ya no (dos esperas por cuadro serian 30 cuadros por segundo)
        eglMakeCurrent(iEglDisplay, gTv.sup, gTv.sup, iEglContext);
        eglSwapInterval(iEglDisplay, 1);
        eglMakeCurrent(iEglDisplay, iEglSurface, iEglSurface, iEglContext);
        eglSwapInterval(iEglDisplay, 0);
        w3dLogf("tv: superficie de %dx%d en la pantalla 1 (%dx%d)", (int)gTv.w, (int)gTv.h, (int)gTv.pantW,
                (int)gTv.pantH);
    }
    juego_salida(aSalida);
}

void CSegaContenedor::Pausar(TBool aPausar) {
    w3dLogf("contenedor: %s", aPausar ? "al fondo" : "al frente");
    if (!iBucle || aPausar == iPausado) return;
    iPausado = aPausar;
    plat_symbian_soltar_teclas();
    gDedosN = 0;
    if (iJuegoIniciado) juego_pausa(aPausar != EFalse);
    if (aPausar) {
        iBucle->Cancel();
    } else {
        juego_redibujar();
        if (!iBucle->IsActive()) iBucle->Start(TCallBack(CSegaContenedor::Cuadro, this));
    }
}

// LOS DEDOS: cada uno con su numero (multitactil); apoyado o arrastrado = esta, soltado = se va
void CSegaContenedor::HandlePointerEventL(const TPointerEvent& aEvento) {
    TInt id = 0;
    if (aEvento.IsAdvancedPointerEvent()) id = aEvento.AdvancedPointerEvent()->PointerNumber();
    TInt k = 0;
    while (k < gDedosN && gDedos[k].id != id) k++;
    switch (aEvento.iType) {
    case TPointerEvent::EButton1Down:
    case TPointerEvent::EDrag:
        if (k == gDedosN) {
            if (gDedosN >= KMaxDedos) return;
            gDedosN++;
        }
        gDedos[k].id = id;
        gDedos[k].x = aEvento.iPosition.iX;
        gDedos[k].y = aEvento.iPosition.iY;
        break;
    case TPointerEvent::EButton1Up:
        if (k < gDedosN) {
            gDedos[k] = gDedos[gDedosN - 1];
            gDedosN--;
        }
        break;
    default:
        break;
    }
}

// las teclas del juego: apretada / soltada. Las demas (la roja, la de menu) siguen al sistema
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

void CSegaContenedor::Draw(const TRect&) const { juego_redibujar(); }
