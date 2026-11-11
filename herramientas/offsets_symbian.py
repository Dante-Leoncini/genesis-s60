# pico_int_offs.h para el N95: las posiciones de los campos de las estructuras de PicoDrive que usan draw_arm.S y
# memory_arm.S. Lo mismo que tools/mkoffsets.sh de PicoDrive (de ahi sale la lista), pero calculado con EL MISMO
# compilador que arma el emulador (el RVCT 2.2, con las macros del .mmp): un arreglo de C con todas las posiciones,
# compilado a asm (armcc -S), y los numeros se leen del listado.
#     python herramientas/offsets_symbian.py     -> build/asm/pico_int_offs.h
import os
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PD = os.path.join(RAIZ, 'picodrive')
SALIDA = os.path.join(RAIZ, 'build', 'asm')
RVCT = r'C:\ARM\RVCT\Programs\2.2\349\win_32-pentium'
# las del .mmp (las que cambian las estructuras)
MACROS = ['-DEMU_C68K', '-D_USE_DRZ80', '-D_ASM_YM2612_C', '-D_ASM_MISC_C', '-D_ASM_DRAW_C', '-D_ASM_MEMORY_C',
          '-DNDEBUG', '-D__SYMBIAN32__', '-D__CC_ARM']


def lista():
    """las de tools/mkoffsets.sh: (nombre de la macro, estructura, campo con puntos)"""
    salida = []
    for linea in open(os.path.join(PD, 'tools', 'mkoffsets.sh'), encoding='latin-1'):
        m = re.match(r'\s*get_define\s+(\S+)\s+(\S+)\s+([^;]+);', linea)
        if not m:
            continue
        prefijo, estructura, campos = m.group(1), m.group(2), m.group(3).split()
        # (lo del 32X y del SH2 no lo usan draw_arm ni memory_arm)
        if estructura in ('SH2_', 'Pico32xMem'):
            continue
        salida.append((prefijo + '_'.join(campos), estructura, '.'.join(campos)))
    return salida


def main():
    os.makedirs(SALIDA, exist_ok=True)
    campos = lista()
    c = os.path.join(SALIDA, 'offs_symbian.c')
    with open(c, 'w', encoding='ascii', newline='\n') as f:
        f.write('#include <stddef.h>\n#include <pico/pico_int.h>\n')
        f.write('#define __builtin_expect(x, esperado) (x)\n')
        f.write('const int pd_offs[] = {\n')
        for nombre, estructura, campo in campos:
            f.write('    (int)offsetof(struct %s, %s),\n' % (estructura, campo))
        f.write('    0x12345678\n};\n')
    s = os.path.join(SALIDA, 'offs_symbian.s')
    args = [os.path.join(RVCT, 'armcc.exe'), '--gnu', '-S', '-O0', '--cpu', '6', '--arm',
            '--preinclude', r'C:\epoc32\include\rvct2_2\rvct2_2.h', '-I' + PD, '-I' + os.path.join(PD, 'zlib'),
            r'-IC:\epoc32\include\stdapis', r'-IC:\epoc32\include'] + MACROS + [c, '-o', s]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        print((r.stdout + r.stderr)[-3000:])
        raise SystemExit('fallo armcc')
    texto = open(s, encoding='latin-1').read()
    i = texto.find('pd_offs')
    valores = [int(v, 0) for v in re.findall(r'DCD\s+(0x[0-9a-fA-F]+|\d+)', texto[i:])]
    if len(valores) < len(campos) + 1 or valores[len(campos)] != 0x12345678:
        raise SystemExit('no encuentro el arreglo en %s (%d valores)' % (s, len(valores)))
    h = os.path.join(SALIDA, 'pico_int_offs.h')
    with open(h, 'w', encoding='ascii', newline='\n') as f:
        f.write('/* generado por herramientas/offsets_symbian.py con el RVCT 2.2 (las posiciones del N95) */\n')
        for (nombre, estructura, campo), v in zip(campos, valores):
            f.write('#define %-30s 0x%04x\n' % (nombre, v))
    print('%s: %d posiciones' % (h, len(campos)))


if __name__ == '__main__':
    sys.exit(main())
