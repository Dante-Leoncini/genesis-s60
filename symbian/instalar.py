"""Instala un .sisx por App TRK (runonphone) esperando a que TRK conteste, con tiempo limite para runonphone (si el
instalador del telefono se queda con un cartel, runonphone no vuelve nunca).

    python symbian/instalar.py build/segaw3d_prueba.sisx [--esperar 600] [--limite 120]"""
import os
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import trk  # noqa: E402

RUNONPHONE = r'C:\QtSDK\Symbian\tools\runonphone.exe'


def esperar_trk(segundos):
    fin = time.time() + segundos
    while True:
        try:
            puerto = trk.buscar_puerto()
            trk.Trk(puerto).cerrar()
            return puerto
        except Exception as e:
            if time.time() > fin:
                raise SystemExit('sin TRK (%s): abrir App TRK en el telefono' % e)
            time.sleep(1)


def instalar(sisx, esperar=600, limite=120):
    puerto = esperar_trk(esperar)
    print('TRK contesta en %s; instalando %s' % (puerto, sisx), flush=True)
    try:
        r = subprocess.run([RUNONPHONE, '-p', puerto, '-s', sisx], capture_output=True, text=True, errors='replace',
                           timeout=limite)
    except subprocess.TimeoutExpired as e:
        salida = (e.stdout or b'')
        if isinstance(salida, bytes):
            salida = salida.decode('latin-1', 'replace')
        print(salida[-600:])
        raise SystemExit('runonphone no termino en %d s (un cartel en el telefono, o se cayo App TRK)' % limite)
    print(r.stdout[-600:])
    ok = 'Cannot install' not in r.stdout and ('Installing finished' in r.stdout or r.stdout.rstrip().endswith('Done.'))
    print('INSTALADO' if ok else 'NO SE INSTALO')
    return ok


if __name__ == '__main__':
    a = sys.argv[1:]
    esperar = int(a[a.index('--esperar') + 1]) if '--esperar' in a else 600
    limite = int(a[a.index('--limite') + 1]) if '--limite' in a else 120
    instalar(os.path.abspath(a[0]), esperar, limite)
