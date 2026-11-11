"""Sube las ROMs de una carpeta de la PC a un telefono, por la API de conectividad de Nokia (la del PC Suite), a la
carpeta donde las busca el emulador. Saltea las que ya estan con el mismo tamano y reintenta las que fallan (en el N8
ConnAPI a veces contesta 0x80100022).

    python symbian/subir_roms.py N8 datos/roms E:/Data/sega"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import telefono  # noqa: E402


def main(modelo, origen, destino):
    t = telefono.N95(modelo)
    print('telefono: %s (%s)' % (t.nombre, t.modelo), flush=True)
    destino = destino.replace('/', '\\')
    t.crear_carpeta(destino)
    hay = {n.lower(): b for n, carpeta, b in t.listar(destino) if not carpeta}
    archivos = sorted(f for f in os.listdir(origen) if os.path.isfile(os.path.join(origen, f)))
    total = len(archivos)
    for i, f in enumerate(archivos):
        ruta = os.path.join(origen, f)
        if hay.get(f.lower()) == os.path.getsize(ruta):
            print('[%d/%d] ya esta: %s' % (i + 1, total, f), flush=True)
            continue
        for intento in range(5):
            try:
                t0 = time.time()
                t.subir(ruta, destino)
                print('[%d/%d] %s (%d KB, %.1f s)' % (i + 1, total, f, os.path.getsize(ruta) // 1024, time.time() - t0),
                      flush=True)
                break
            except telefono.ErrorN95 as e:
                print('[%d/%d] %s: %s (reintento %d)' % (i + 1, total, f, e, intento + 1), flush=True)
                time.sleep(2)
    t.cerrar()
    print('LISTO', flush=True)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
