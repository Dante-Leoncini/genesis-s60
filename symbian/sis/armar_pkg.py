"""Arma el instalador del N95 (segaw3d.pkg -> segaw3d.sisx, firmado con el certificado propio de Whisk3D).

El ejecutable va en sys\\bin y la configuracion en segaw3d\\datos\\ (config.txt), en la unidad que elija el usuario (el
emulador busca los datos en la del ejecutable). Las ROMs NO van en el instalador: config.txt dice en que carpeta del
telefono estan (carpeta=...) y el emulador arranca con el menu de esa carpeta.

    python symbian/sis/armar_pkg.py                     el instalador para usar (unidad a eleccion)
    python symbian/sis/armar_pkg.py --carpeta e:/data/sega/

Desde probar.py: en C:, con una prueba.txt (el guion de botones y la salida) y rom= (directo a Mega Bomberman, sin el
menu). El .sisx queda en build/. Antes hay que compilar (symbian/compilar.bat).

La firma: un certificado propio SHA-1 (segaw3d_firma.cer/.key, al lado de este script): el N95 (S60 3rd FP1) no
conoce SHA-256 y dice "archivo danado"."""
import os
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.normpath(os.path.join(AQUI, '..', '..'))
SALIDA = os.path.join(RAIZ, 'build')
CERTIFICADO = os.path.join(AQUI, 'segaw3d_firma.cer')
CLAVE = os.path.join(AQUI, 'segaw3d_firma.key')
HERRAMIENTAS = r'C:\epoc32\tools'
UID = '0xE5E6A001'
EXE = r'\epoc32\release\armv5\urel\segaw3d.exe'
CARPETA_N95 = 'e:/data/sega/'
ROM_N95 = CARPETA_N95 + 'Mega Bomberman.zip'

CABEZA = r''';
; segaw3d para el Nokia N95: el emulador de Mega Drive (PicoDrive) con hasta 4 mandos. Lo arma armar_pkg.py.
;
&EN

#{"Mega Drive W3D"},(%(uid)s),1,0,0,TYPE=SA

%%{"Dante Leoncini"}
:"Dante Leoncini"

; las plataformas: S60 3rd (el N95: FP1; FP2), S60 5th y Symbian^3. Hace falta Open C / Open C++ (en S60 3rd hay que
; instalarlo; en Symbian^3 viene de fabrica)
[0x101F7961],0,0,0,{"Series60ProductID"}
[0x102032BE],0,0,0,{"Series60ProductID"}
[0x102752AE],0,0,0,{"Series60ProductID"}
[0x1028315F],0,0,0,{"Series60ProductID"}
[0x20022E6D],0,0,0,{"Series60ProductID"}

"\epoc32\data\z\resource\apps\segaw3d.rsc"                     -"%(u)s:\resource\apps\segaw3d.rsc"
"\epoc32\data\z\private\10003a3f\import\apps\segaw3d_reg.rsc"  -"%(u)s:\private\10003a3f\import\apps\segaw3d_reg.rsc"
"\epoc32\data\z\resource\apps\segaw3d_icono.mif"               -"%(u)s:\resource\apps\segaw3d_icono.mif"
'''


def certificado():
    """el certificado propio para firmar (la primera vez, con openssl; no se versiona). SHA-1: el N95 no conoce
    SHA-256"""
    if os.path.isfile(CERTIFICADO) and os.path.isfile(CLAVE):
        return
    # (openssl 1.1+ deja la clave en PKCS#8; signsis quiere la RSA de siempre: el segundo paso la pasa)
    pkcs8 = CLAVE + '.pkcs8'
    subprocess.check_call(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-sha1', '-days', '7300', '-nodes',
                           '-keyout', pkcs8, '-out', CERTIFICADO, '-subj', '/CN=genesis-s60'])
    subprocess.check_call(['openssl', 'rsa', '-in', pkcs8, '-out', CLAVE])
    os.remove(pkcs8)
    print('certificado nuevo:', CERTIFICADO)


def armar(prueba=None, carpeta=CARPETA_N95, perfil=False, correr=True, unidad=None):
    """prueba: el texto de prueba.txt (None = el instalador para usar). Devuelve la ruta del .sisx"""
    certificado()
    exe = os.path.join('C:' + os.sep, EXE.lstrip('\\'))
    if not os.path.isfile(exe):
        raise SystemExit('falta %s: compilar antes (symbian/compilar.bat)' % exe)
    de_prueba = prueba is not None
    u = unidad or ('E' if de_prueba else '!')
    lineas = [CABEZA % {'uid': UID, 'u': u}]
    os.makedirs(SALIDA, exist_ok=True)
    tmp = os.path.join(SALIDA, 'pkg_datos')
    os.makedirs(tmp, exist_ok=True)
    config = os.path.join(tmp, 'config.txt')
    with open(config, 'w', newline='\r\n') as f:
        # (sin entrada=: el emulador pone el Team Player solo en los juegos que lo soportan)
        f.write('carpeta=%s\n' % carpeta)
        if de_prueba:
            f.write('rom=%s\n' % ROM_N95)
        if perfil or de_prueba:
            f.write('perfil=1\n')
    lineas.append('\n"%s" -"%s:\\segaw3d\\datos\\config.txt"\n' % (config, u))
    # (la prueba va siempre, vacia en el de usar: pisa la de una prueba anterior)
    p = os.path.join(tmp, 'prueba.txt')
    with open(p, 'w', newline='\r\n') as f:
        f.write(prueba or '')
    lineas.append('"%s" -"%s:\\segaw3d\\datos\\prueba.txt"\n' % (p, u))
    # el ejecutable va AL FINAL: con FR,RI el instalador lo lanza apenas lo copia
    lineas.append('\n"%s" -"%s:\\sys\\bin\\segaw3d.exe"%s\n' % (EXE, u, ',FR,RI' if de_prueba and correr else ''))
    nombre = 'segaw3d_prueba' if de_prueba else 'segaw3d'
    pkg = os.path.join(SALIDA, nombre + '.pkg')
    with open(pkg, 'w', newline='\r\n') as f:
        f.writelines(lineas)
    sis = os.path.join(SALIDA, nombre + '.sis')
    sisx = os.path.join(SALIDA, nombre + '.sisx')
    for v in (sis, sisx):
        if os.path.exists(v):
            os.remove(v)
    # (makesis resuelve las rutas \epoc32\... contra la unidad actual: se corre desde C:)
    subprocess.check_call([os.path.join(HERRAMIENTAS, 'makesis.exe'), pkg, sis], cwd='C:\\')
    subprocess.check_call([os.path.join(HERRAMIENTAS, 'signsis.exe'), sis, sisx, CERTIFICADO, CLAVE])
    os.remove(sis)
    print('%s: %d KB' % (sisx, os.path.getsize(sisx) // 1024))
    return sisx


if __name__ == '__main__':
    a = sys.argv[1:]
    armar(carpeta=a[a.index('--carpeta') + 1] if '--carpeta' in a else CARPETA_N95, perfil='--perfil' in a)
