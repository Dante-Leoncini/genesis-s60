"""Una prueba en el N95, desde la PC y por el cable: compila, arma el instalador con una prueba.txt (el guion de
botones: entra a una batalla de 4 en Mega Bomberman y se cierra a los N segundos), lo instala por App TRK
(runonphone, del Qt SDK) y espera el log. Hay que ABRIR EL EMULADOR A MANO en el telefono: arrancado por el
instalador se cae App TRK, y TRK no puede lanzar programas.

En el telefono tiene que estar App TRK abierto y conectado por USB. Nunca se usa el instalador del PC Suite: deja un
dialogo esperando en el telefono y traba las instalaciones siguientes.

    python symbian/probar.py                  compilar + instalar + 90 s (la batalla empieza a los ~55 s)
    python symbian/probar.py --segundos 120
    python symbian/probar.py --sin-compilar
    python symbian/probar.py --instalar       solo instalar el de usar (sin prueba ni perfil)
    python symbian/probar.py --log            solo bajar y mostrar el log"""
import os
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
sys.path.insert(0, os.path.join(AQUI, 'sis'))
import trk  # noqa: E402
import armar_pkg  # noqa: E402

RUNONPHONE = r'C:\QtSDK\Symbian\tools\runonphone.exe'
LOG = r'C:\segaw3d\segaw3d.log'

# el turno del telefono entre las sesiones que lo usan
OCUPADO = os.path.normpath(os.path.join(AQUI, '..', '..', '..', 'conversiones', 'n95_ocupado.txt'))
# Mega Bomberman: la intro, el titulo, BATTLE GAME, 4 combatientes, y adelante hasta que arranca la batalla (~3200
# cuadros); despues el jugador 1 camina y pone bombas
GUION_BOMBERMAN = ('600:start,610:nada,800:start,810:nada,1000:abajo,1010:nada,1050:start,1060:nada,1200:start,'
                   '1210:nada,1550:abajo,1560:nada,1600:start,1610:nada,1900:abajo,1910:nada,2000:abajo,2010:nada,'
                   '2100:start,2110:nada,2300:start,2310:nada,2500:start,2510:nada,2700:start,2710:nada,2850:start,'
                   '2860:nada,3000:start,3010:nada,3150:start,3160:nada,3400:der,3440:b,3450:abajo,3500:nada,'
                   '3700:izq,3740:b,3750:arriba,3800:nada')


def compilar():
    # (sin lo de MSYS / git-bash en el entorno: las herramientas de Symbian se pierden con su PATH)
    env = dict((k, v) for k, v in os.environ.items() if not k.upper().startswith(('MSYS', 'MINGW', 'SHELL', 'TERM')))
    env['PATH'] = os.pathsep.join(p for p in env.get('PATH', '').split(os.pathsep)
                                  if 'msys' not in p.lower() and r'\git' not in p.lower() and not p.startswith('/'))
    r = subprocess.run(['cmd', '/c', os.path.join(AQUI, 'compilar.bat')], capture_output=True, text=True,
                       errors='replace', env=env)
    if r.returncode != 0:
        print(r.stdout[-4000:])
        print(r.stderr[-2000:])
        raise SystemExit('fallo la compilacion')
    print('compilado')


def instalar(sisx):
    import instalar as inst
    if not inst.instalar(sisx, esperar=600, limite=150):
        raise SystemExit('no se instalo')


def bajar_log():
    t = trk.Trk(esperar=30)
    try:
        datos = t.bajar(LOG)
    finally:
        t.cerrar()
    return datos.decode('latin-1', 'replace').replace('\r', '').split('\n')


def primera_linea():
    """la primera linea del log de la corrida anterior (para reconocer cuando hay uno nuevo)"""
    try:
        lineas = bajar_log()
        return lineas[0] if lineas else ''
    except trk.ErrorTrk:
        return ''


class Turno:
    """El N95 lo comparten varias sesiones: OCUPADO dice quien lo esta usando (se espera a que lo suelte)."""
    def __enter__(self):
        while os.path.exists(OCUPADO):
            with open(OCUPADO, errors='replace') as f:
                quien = f.read().strip()
            if quien.startswith('segaw3d'):
                break
            print('el N95 esta ocupado: %s (espero)' % quien)
            time.sleep(30)
        with open(OCUPADO, 'w') as f:
            f.write('segaw3d (probar.py) desde %s\n' % time.strftime('%Y-%m-%d %H:%M:%S'))

    def __exit__(self, *a):
        if os.path.exists(OCUPADO):
            with open(OCUPADO, errors='replace') as f:
                propio = f.read().startswith('segaw3d')
            if propio:
                os.remove(OCUPADO)


def mostrar(lineas):
    inicio = max([i for i, l in enumerate(lineas) if 'inicio de sesion' in l] or [0])
    for l in lineas[inicio:]:
        if l.strip():
            print('  ' + l.strip()[:220])


def main(a):
    segundos = int(a[a.index('--segundos') + 1]) if '--segundos' in a else 90
    with Turno():
        if '--log' in a:
            mostrar(bajar_log())
            return
        if '--sin-compilar' not in a:
            compilar()
        if '--instalar' in a:
            instalar(armar_pkg.armar())
            return
        # En C: y SIN arrancar solo: lanzado por el instalador (FR,RI) el emulador se lleva puesto a App TRK, y TRK
        # no puede lanzar programas (CREAR da error 32). Se abre a mano desde el menu; se espera el log de esa corrida
        antes = primera_linea()
        instalar(armar_pkg.armar(prueba='teclas=%s\nsalir=%d\n' % (GUION_BOMBERMAN, segundos), correr=False,
                                 unidad='C'))
        print('ABRIR "Mega Drive W3D" EN EL TELEFONO: corre %d s y se cierra solo' % segundos, flush=True)
        lineas = []
        fin = time.time() + segundos + 600
        while time.time() < fin:
            time.sleep(15)
            try:
                lineas = bajar_log()
            except trk.ErrorTrk as e:
                print('log: %s' % e)
                continue
            if lineas and lineas[0] != antes and any('prueba: terminada' in l for l in lineas):
                break
        mostrar(lineas)
        with open(os.path.join(armar_pkg.SALIDA, 'segaw3d_n95.log'), 'w') as f:
            f.write('\n'.join(lineas))


if __name__ == '__main__':
    main(sys.argv[1:])
