// ---------------------------------------------------------------------------------------------------------------
//  La aplicacion de S60 3rd (Nokia N95) alrededor del emulador: lo minimo del framework (la aplicacion, el documento,
//  la AppUi) y el contenedor, que tiene la ventana de pantalla completa con el Direct Screen Access (el emulador
//  dibuja directo en la pantalla, por CPU), el bucle de cuadros (un CIdle que llama a juego_cuadro y presenta) y las
//  teclas. El emulador esta en ../../src: no sabe nada de Symbian (ver src/plataforma.h).
// ---------------------------------------------------------------------------------------------------------------
#ifndef SEGAW3D_H
#define SEGAW3D_H

#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <w32std.h>
#include <stdint.h>

// el UID3 (del rango de pruebas, sin proteger: 0xE...): el mismo del .mmp, de los .rss y del .pkg
const TUid KUidSegaW3d = { 0xE5E6A001 };

class CSegaAplicacion : public CAknApplication {
private:
    CApaDocument* CreateDocumentL();
    TUid AppDllUid() const;
};

class CSegaDocumento : public CAknDocument {
public:
    CSegaDocumento(CEikApplication& aApp) : CAknDocument(aApp) {}
private:
    CEikAppUi* CreateAppUiL();
};

class CIdle;

class CSegaContenedor : public CCoeControl, public MDirectScreenAccess {
public:
    void ConstructL();
    ~CSegaContenedor();
    TKeyResponse OfferKeyEventL(const TKeyEvent& aEvento, TEventCode aTipo);
    // la aplicacion paso al fondo (o volvio): el bucle se para, las teclas se sueltan y el sonido calla
    void Pausar(TBool aPausar);
private:
    void SizeChanged();
    void HandleResourceChange(TInt aTipo);
    void Draw(const TRect& aRect) const;
    static TInt Cuadro(TAny* aContenedor);
    void UnCuadro();
    // MDirectScreenAccess: otra ventana tapa la pantalla (un aviso, el menu) y cuando se va
    void AbortNow(RDirectScreenAccess::TTerminationReasons aRazon);
    void Restart(RDirectScreenAccess::TTerminationReasons aRazon);
    void EmpezarDsa();

    CDirectScreenAccess* iDsa;
    TBool iDsaActivo;
    CIdle* iBucle;
    TBool iJuegoIniciado;
    TBool iPausado;
    TInt iCuadros;
};

class CSegaAppUi : public CAknAppUi {
public:
    void ConstructL();
    ~CSegaAppUi();
    void HandleCommandL(TInt aComando);
private:
    void HandleForegroundEventL(TBool aAdelante);
    CSegaContenedor* iContenedor;
};

#endif
