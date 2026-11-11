# Los nucleos en asm ARM de PicoDrive para el N95: estan en la sintaxis de GNU as (el armasm del RVCT no los lee), asi
# que se ensamblan con el gcc de Symbian de CodeSourcery (arm-none-symbianelf, 4.4.1) y se juntan en una biblioteca
# estatica (pdasm.lib) que el .mmp linkea con el RVCT (STATICLIBRARY):
#   - Cyclone: el 68000. Cyclone.s lo ESCRIBE un programa (picodrive/cpu/cyclone, compilado con el g++ de MSYS2 y
#     ejecutado aca) con la configuracion de PicoDrive (cpu/cyclone_config.h) y las instrucciones de ARMv6
#   - m68kif_cyclone.s, cyclone/tools/idle.s: lo que une Cyclone con PicoDrive
#   - DrZ80: el Z80
#   - ym2612_arm.S: el sonido FM;  misc_arm.s (y el del Mega CD): copias y rellenos de memoria
#   - draw_arm.S / draw2_arm.S: el dibujo de los planos y los sprites;  memory_arm.S: la memoria del 68000. Piden
#     pico_int_offs.h (las posiciones de las estructuras): herramientas/offsets_symbian.py, con el RVCT
#   - mix_arm.S: la mezcla del sonido (reemplaza a pico/sound/mix.c)
#     python herramientas/asm_symbian.py
import glob
import os
import shutil
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PD = os.path.join(RAIZ, 'picodrive')
SALIDA = os.path.join(RAIZ, 'build', 'asm')
MINGW = r'C:\msys64\mingw64\bin'
CS = r'C:\Program Files (x86)\CodeSourcery\Sourcery G++ Lite\bin'
EPOC = r'C:\epoc32\release\armv5'
# las macros con que se arman los .S (las mismas que el .mmp le pasa al C)
MACROS = ['-DHAVE_ARMv6=1', '-D_ASM_YM2612_C', '-D_ASM_MISC_C', '-D_ASM_DRAW_C', '-D_ASM_MEMORY_C']
FUENTES = ['pico/m68kif_cyclone.s', 'cpu/cyclone/tools/idle.s', 'cpu/DrZ80/drz80.S', 'pico/sound/ym2612_arm.S',
           'pico/misc_arm.s', 'pico/cd/misc_arm.s', 'pico/draw_arm.S', 'pico/draw2_arm.S', 'pico/memory_arm.S',
           'pico/sound/mix_arm.S']


def correr(args, cwd=None, path=None):
    env = dict(os.environ)
    if path:
        env['PATH'] = path + os.pathsep + env['PATH']
    r = subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-2000:], r.stderr[-2000:])
        raise SystemExit('fallo: %s' % ' '.join(args[:3]))
    return r.stdout


def cyclone():
    cy = os.path.join(PD, 'cpu', 'cyclone')
    gen = os.path.join(SALIDA, 'cyclone_gen.exe')
    config = os.path.join(PD, 'cpu', 'cyclone_config.h').replace('\\', '/')
    fuentes = [os.path.join(cy, f) for f in ('Main.cpp', 'Ea.cpp', 'OpAny.cpp', 'OpArith.cpp', 'OpBranch.cpp',
                                              'OpLogic.cpp', 'OpMove.cpp', os.path.join('Disa', 'Disa.c'))]
    correr([os.path.join(MINGW, 'g++.exe'), '-O2', '-w', '-DCONFIG_FILE="%s"' % config, '-DHAVE_ARMv6=1',
            '-I' + cy] + fuentes + ['-o', gen], path=MINGW)
    correr([gen], cwd=SALIDA, path=MINGW)
    print('Cyclone.s: %d KB' % (os.path.getsize(os.path.join(SALIDA, 'Cyclone.s')) // 1024))


def idle_symbian():
    """cyclone/tools/idle.s busca sus tablas de .data desde .text con una DISTANCIA fija entre las dos secciones
    (adr r12,offset_table + .word idle_data - offset_table). En Symbian el codigo y los datos se cargan en lugares
    separados: esa distancia no vale y al cargar una ROM (SekInitIdleDet) la app se caia. La copia para Symbian guarda
    las DIRECCIONES (el sistema las reubica al cargar) y no les suma la base"""
    import re
    texto = open(os.path.join(PD, 'cpu', 'cyclone', 'tools', 'idle.s'), encoding='latin-1').read()
    cambios = [
        (r'\.word idle_data - offset_table', '.word idle_data'),
        (r'\.word patch_desc_table - offset_table', '.word patch_desc_table'),
        (r'add\s+r2, r2, r12', 'mov     r2, r2'),
        (r'str\s+r1, \[r0, r12\]', 'str     r1, [r0]'),
        (r'ldr\s+r1, \[r3, r12\]!', 'ldr     r1, [r3]'),
        (r'add\s+r1, r1, r12', 'mov     r1, r1'),
    ]
    for patron, nuevo in cambios:
        texto, n = re.subn(patron, nuevo, texto)
        if n == 0:
            raise SystemExit('idle.s cambio: no encuentro %s' % patron)
    salida = os.path.join(SALIDA, 'idle_symbian.s')
    with open(salida, 'w', encoding='latin-1', newline='\n') as f:
        f.write('@ generado por herramientas/asm_symbian.py desde picodrive/cpu/cyclone/tools/idle.s (sin distancias\n'
                '@ entre .text y .data)\n' + texto)
    return salida


def main():
    os.makedirs(SALIDA, exist_ok=True)
    for viejo in glob.glob(os.path.join(SALIDA, '*.o')):
        os.remove(viejo)
    cyclone()
    subprocess.check_call([sys.executable, os.path.join(RAIZ, 'herramientas', 'offsets_symbian.py')])
    gcc = os.path.join(CS, 'arm-none-symbianelf-gcc.exe')
    objetos = []
    fuentes = [os.path.join(SALIDA, 'Cyclone.s'), idle_symbian()] + \
        [os.path.join(PD, x) for x in FUENTES if not x.endswith('idle.s')]
    for f in fuentes:
        # (el nombre con la carpeta: hay dos misc_arm.s)
        rel = os.path.relpath(f, PD) if f.startswith(PD) else os.path.basename(f)
        o = os.path.join(SALIDA, os.path.splitext(rel.replace(os.sep, '_').replace('/', '_'))[0] + '.o')
        correr([gcc, '-c', '-mcpu=arm1136jf-s', '-marm', '-I' + SALIDA, '-I' + PD, '-I' + os.path.join(PD, 'pico'),
                '-I' + os.path.join(PD, 'cpu', 'cyclone')] + MACROS + [f, '-o', o], path=CS)
        objetos.append(o)
    lib = os.path.join(SALIDA, 'pdasm.lib')
    if os.path.exists(lib):
        os.remove(lib)
    correr([os.path.join(CS, 'arm-none-symbianelf-ar.exe'), 'rcs', lib] + objetos, path=CS)
    for v in ('urel', 'udeb'):
        os.makedirs(os.path.join(EPOC, v), exist_ok=True)
        shutil.copy(lib, os.path.join(EPOC, v, 'pdasm.lib'))
    print('pdasm.lib: %d objetos -> %s\\{urel,udeb}' % (len(objetos), EPOC))


if __name__ == '__main__':
    sys.exit(main())
