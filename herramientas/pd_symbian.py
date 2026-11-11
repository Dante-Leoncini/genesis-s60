# El nucleo de PicoDrive para el proyecto de Symbian (abld + RVCT 2.2), sin tocar picodrive/:
#   - un envoltorio por cada .c de PD_C del Makefile en symbian/pd/ (pd_pico_cd_memory.c = #include
#     <pico/cd/memory.c>): abld deja todos los objetos en una sola carpeta y PicoDrive tiene cinco memory.c, dos
#     pico.c, ... con el mismo nombre
#   - lo que el RVCT necesita en algunos (el Z80 sin la tabla de saltos de gcc, off_t, M_PI)
#   - symbian/group/pd_fuentes.mmh con la lista para el .mmp
# El #include va dentro de "#ifdef __CC_ARM" (solo el armcc lo ve): el cpp viejo (C89) con el que makmake busca las
# dependencias no entiende las macros con "..." de PicoDrive y corta la generacion del makefile. Por eso abld no
# sabe de que depende cada envoltorio: despues de actualizar picodrive/, compilar.bat limpio.
#     python herramientas/pd_symbian.py
import os
import re

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# lo que en el N95 va en asm ARM (herramientas/asm_symbian.py -> pdasm.lib): el 68000 en C (FAME), el Z80 en C (CZ80) y
# la mezcla del sonido en C no entran
SOLO_PC = ('cpu/fame/famec.c', 'cpu/cz80/cz80.c', 'pico/sound/mix.c')

# lo que va antes del #include en algunos archivos
ANTES = {
    "cpu/cz80/cz80.c": "#include <cpu/cz80/cz80.h>\n#undef CZ80_USE_JUMPTABLE\n"
                       "/* (el RVCT no tiene los goto *etiqueta de gcc: el switch) */\n#define CZ80_USE_JUMPTABLE 0\n",
    "pico/cd/cdd.c": "#include <sys/types.h>\n",
    "pico/pico/xpcm.c": "#include <math.h>\n#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n"
                        "#ifndef M_SQRT2\n#define M_SQRT2 1.41421356237309504880\n#endif\n",
}


def fuentes():
    mk = open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read()
    m = re.search(r"PD_C :=(.*?)\nPD_DEFS", mk, re.S)
    return m.group(1).replace("\\", " ").split()


def main():
    carpeta = os.path.join(RAIZ, "symbian", "pd")
    os.makedirs(carpeta, exist_ok=True)
    for viejo in os.listdir(carpeta):
        if viejo.startswith("pd_") and viejo.endswith(".c"):
            os.remove(os.path.join(carpeta, viejo))
    nombres = []
    for f in fuentes():
        if f in SOLO_PC:
            continue
        nombre = "pd_" + f[:-2].replace("/", "_") + ".c"
        texto = "/* generado por herramientas/pd_symbian.py: el %s de picodrive/ */\n" % f
        texto += "#ifdef __CC_ARM\n"
        # (con --gnu el RVCT 2.2 dice ser gcc pero no tiene __builtin_expect: likely()/unlikely() de PicoDrive)
        texto += "#define __builtin_expect(x, esperado) (x)\n"
        texto += ANTES.get(f, "")
        texto += "#include <%s>\n" % f
        texto += "#endif\n"
        with open(os.path.join(carpeta, nombre), "w", encoding="ascii", newline="\n") as s:
            s.write(texto)
        nombres.append(nombre)
    lineas = ["// generado por herramientas/pd_symbian.py: el nucleo de PicoDrive (un envoltorio por archivo)",
              "SOURCEPATH      ..\\pd"]
    for i in range(0, len(nombres), 4):
        lineas.append("SOURCE          " + " ".join(nombres[i:i + 4]))
    with open(os.path.join(RAIZ, "symbian", "group", "pd_fuentes.mmh"), "w", encoding="ascii", newline="\r\n") as s:
        s.write("\n".join(lineas) + "\n")
    print(len(nombres), "archivos")


if __name__ == "__main__":
    main()
