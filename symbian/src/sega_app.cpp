// La aplicacion, el documento y la AppUi (lo minimo del framework de S60: todo se dibuja con OpenGL ES en el
// contenedor). Siempre acostada: la Mega Drive es de 320x224 y el N95 acostado es de 320x240 (pixel por pixel).
#include "segaw3d.h"
#include <eikstart.h>
#include <avkon.hrh>
#include <eikspane.h>
#include <eikbtgpc.h>

// ------------------------------------------------------------------------------------------- la aplicacion
TUid CSegaAplicacion::AppDllUid() const { return KUidSegaW3d; }

CApaDocument* CSegaAplicacion::CreateDocumentL() { return new (ELeave) CSegaDocumento(*this); }

LOCAL_C CApaApplication* NuevaAplicacion() { return new CSegaAplicacion; }

GLDEF_C TInt E32Main() { return EikStart::RunApplication(NuevaAplicacion); }

// ------------------------------------------------------------------------------------------- el documento
CEikAppUi* CSegaDocumento::CreateAppUiL() { return new (ELeave) CSegaAppUi; }

// ------------------------------------------------------------------------------------------- la AppUi
void CSegaAppUi::ConstructL() {
    BaseConstructL(EAknEnableSkin | EAppOrientationLandscape);
    // sin la barra de estado ni la de las teclas de abajo: la ventana es la pantalla entera y las dos teclas de abajo
    // llegan como teclas (164 y 165)
    if (StatusPane()) StatusPane()->MakeVisible(EFalse);
    if (Cba()) Cba()->MakeVisible(EFalse);
    iContenedor = new (ELeave) CSegaContenedor;
    iContenedor->SetMopParent(this);
    iContenedor->ConstructL();
    AddToStackL(iContenedor);
}

CSegaAppUi::~CSegaAppUi() {
    if (iContenedor) {
        RemoveFromStack(iContenedor);
        delete iContenedor;
    }
}

void CSegaAppUi::HandleCommandL(TInt aComando) {
    switch (aComando) {
    case EEikCmdExit:
    case EAknSoftkeyExit:
    case EAknCmdExit:
        Exit();
        break;
    default:
        break;
    }
}

// (al fondo: otra aplicacion, una llamada, la tecla roja)
void CSegaAppUi::HandleForegroundEventL(TBool aAdelante) {
    CAknAppUi::HandleForegroundEventL(aAdelante);
    if (iContenedor) iContenedor->Pausar(!aAdelante);
}
