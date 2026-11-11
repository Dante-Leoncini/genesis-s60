"""El N95 por el cable USB, desde la PC: la API de conectividad de Nokia (ConnAPI, la que usa el PC Suite). Hace
falta el Nokia PC Suite (o el PC Connectivity Solution) instalado y el telefono enchufado en modo "PC Suite".

    python n95.py info                       el telefono, sus unidades y la memoria libre
    python n95.py ls E:\\                    lo que hay en una carpeta del telefono
    python n95.py bajar E:\\crashw3d\\crashw3d.log [carpeta_de_la_pc]
    python n95.py subir archivo E:\\carpeta
    python n95.py borrar E:\\carpeta\\archivo
    python n95.py instalar archivo.sisx      lo copia y abre el instalador del telefono
    python n95.py log [--desde N]            E:\\crashw3d\\crashw3d.log (el log del juego: uno nuevo en cada arranque)

Las rutas del telefono van como en el telefono (E:\\carpeta\\archivo); ConnAPI las quiere como \\\\\\\\E:\\carpeta.

Los prototipos de ConnAPI salen de la DLL (el SDK de PC Connectivity ya no se consigue): ver los comentarios de cada
funcion. Se usa ConnAPI64.dll con el Python de 64 bits y ConnAPI.dll con el de 32."""
import ctypes as C
import os
import struct
import sys
import tempfile
import time
from ctypes import wintypes as W

CARPETA_API = r"C:\Program Files (x86)\PC Connectivity Solution"
VERSION_API = 34

# CONACopyFile: el sentido de la copia
TELEFONO_A_PC = 0x2
PC_A_TELEFONO = 0x4
# CONAInstallApplication: el tipo de archivo y la opcion que anda con el N95 (el archivo se busca en la carpeta de
# origen; sin ella devuelve "parametro invalido")
APLICACION_SIS = 0x1
INSTALAR_DESDE_CARPETA = 0x10000

ERRORES = {
    0x80100004: "ConnAPI sin inicializar",
    0x80100012: "puntero invalido",
    0x80100013: "parametro invalido",
    0x80100014: "handle invalido",
    0x80200006: "el telefono no esta conectado",
}


class _Conexion(C.Structure):
    _fields_ = [("dwDeviceID", W.DWORD), ("dwMedia", W.DWORD), ("pstrDeviceName", C.c_wchar_p),
                ("pstrAddress", C.c_wchar_p), ("dwState", W.DWORD)]


class _Dispositivo(C.Structure):
    _fields_ = [("pstrSerialNumber", C.c_wchar_p), ("pstrFriendlyName", C.c_wchar_p), ("pstrModel", C.c_wchar_p),
                ("pstrManufacturer", C.c_wchar_p), ("dwNumberOfItems", W.DWORD),
                ("pItems", C.POINTER(_Conexion))]


class _Carpeta(C.Structure):
    _fields_ = [("pstrName", C.c_wchar_p), ("dwAttributes", W.DWORD), ("tFolderTime", W.FILETIME),
                ("pstrLabel", C.c_wchar_p), ("pstrMemoryType", C.c_wchar_p)]


class _Archivo(C.Structure):
    _fields_ = [("pstrName", C.c_wchar_p), ("dwAttributes", W.DWORD), ("tFileTime", W.FILETIME),
                ("dwFileSize", W.DWORD), ("pstrMIMEType", C.c_wchar_p)]


class _Sis(C.Structure):
    # el nombre del .sisx y su carpeta en la PC
    _fields_ = [("pstrSisFile", C.c_wchar_p), ("pstrFilePath", C.c_wchar_p)]


class ErrorN95(Exception):
    pass


def _ruta(ruta):
    """E:\\carpeta -> \\\\\\\\E:\\carpeta (como la quiere ConnAPI)."""
    ruta = ruta.replace("/", "\\").rstrip("\\")
    return "\\\\\\\\" + ruta


def _partir(ruta):
    ruta = ruta.replace("/", "\\")
    i = ruta.rfind("\\")
    return ruta[:i], ruta[i + 1:]


class N95:
    def __init__(self, modelo=None):
        """modelo: un pedazo del nombre del telefono ("N8", "N95"); sin modelo, el primero que haya"""
        es64 = struct.calcsize("P") == 8
        os.add_dll_directory(CARPETA_API)
        self.api = C.WinDLL(os.path.join(CARPETA_API, "ConnAPI64.dll" if es64 else "ConnAPI.dll"))
        self._ok(self.api.DMAPI_Initialize(VERSION_API, None), "DMAPI_Initialize")
        self._ok(self.api.FSAPI_Initialize(VERSION_API, None), "FSAPI_Initialize")
        self.dm = C.c_void_p()
        self._ok(self.api.CONAOpenDM(C.byref(self.dm)), "CONAOpenDM")
        n = W.DWORD()
        self._ok(self.api.CONAGetDeviceCount(self.dm, C.byref(n)), "CONAGetDeviceCount")
        if n.value == 0:
            raise ErrorN95("no hay ningun telefono conectado (enchufarlo en modo PC Suite)")
        self._dispositivos = (_Dispositivo * n.value)()
        self._ok(self.api.CONAGetDevices(self.dm, C.byref(n), self._dispositivos), "CONAGetDevices")
        d = self._dispositivos[0]
        if modelo:
            elegidos = [x for x in self._dispositivos if modelo.lower() in ((x.pstrFriendlyName or "") + " " +
                                                                               (x.pstrModel or "")).lower()]
            if not elegidos:
                raise ErrorN95("no hay un telefono %s conectado (hay: %s)" % (modelo, ", ".join(
                    (x.pstrFriendlyName or "?") for x in self._dispositivos)))
            d = elegidos[0]
        self.serie, self.nombre, self.modelo = d.pstrSerialNumber, d.pstrFriendlyName, d.pstrModel
        medio = W.DWORD(d.pItems[0].dwMedia if d.dwNumberOfItems else 0)
        ident = W.DWORD(d.pItems[0].dwDeviceID if d.dwNumberOfItems else 0)
        self.fs = C.c_void_p()
        self._ok(self.api.CONAOpenFS(C.c_wchar_p(self.serie), C.byref(medio), C.byref(self.fs), C.byref(ident)),
                 "CONAOpenFS")

    @staticmethod
    def _ok(r, que):
        r &= 0xFFFFFFFF
        if r:
            raise ErrorN95("%s: 0x%08X %s" % (que, r, ERRORES.get(r, "")))

    def cerrar(self):
        self.api.CONACloseFS(self.fs)
        self.api.CONACloseDM(self.dm)

    # ----------------------------------------------------------------------------------------------- archivos
    def listar(self, carpeta):
        """[(nombre, es_carpeta, bytes)]; carpeta "" = las unidades."""
        h = C.c_void_p()
        self._ok(self.api.CONAFindBegin(self.fs, 0, C.byref(h), C.c_wchar_p(_ruta(carpeta) if carpeta else "\\\\")),
                 "CONAFindBegin %s" % carpeta)
        cosas = []
        try:
            while True:
                c = _Carpeta()
                if self.api.CONAFindNextFolder(h, C.byref(c)) != 0:
                    break
                cosas.append((c.pstrName, True, 0))
            while True:
                a = _Archivo()
                if self.api.CONAFindNextFile(h, C.byref(a)) != 0:
                    break
                cosas.append((a.pstrName, False, a.dwFileSize))
        finally:
            self.api.CONAFindEnd(h)
        return cosas

    def existe(self, ruta):
        carpeta, nombre = _partir(ruta)
        try:
            return any(n.lower() == nombre.lower() for n, _, _ in self.listar(carpeta))
        except ErrorN95:
            return False

    def bajar(self, ruta, carpeta_pc):
        """Copia un archivo del telefono a la PC (lo pisa) y devuelve su ruta en la PC."""
        carpeta, nombre = _partir(ruta)
        carpeta_pc = os.path.abspath(carpeta_pc)
        os.makedirs(carpeta_pc, exist_ok=True)
        destino = os.path.join(carpeta_pc, nombre)
        if os.path.exists(destino):
            os.remove(destino)
        # CONACopyFile(fs, sentido, nombre, carpeta de origen, carpeta de destino)
        self._ok(self.api.CONACopyFile(self.fs, TELEFONO_A_PC, C.c_wchar_p(nombre), C.c_wchar_p(_ruta(carpeta)),
                                       C.c_wchar_p(carpeta_pc)), "bajar %s" % ruta)
        return destino

    def borrar(self, ruta):
        carpeta, nombre = _partir(ruta)
        # CONADeleteFile(fs, nombre, carpeta)
        self._ok(self.api.CONADeleteFile(self.fs, C.c_wchar_p(nombre), C.c_wchar_p(_ruta(carpeta))),
                 "borrar %s" % ruta)

    def crear_carpeta(self, ruta):
        """Crea la carpeta (y las de arriba que falten), como "E:/Data/sega"."""
        partes = ruta.replace("/", "\\").rstrip("\\").split("\\")
        actual = partes[0]
        for p in partes[1:]:
            if not self.existe(actual + "\\" + p):
                # CONACreateFolder(fs, nombre, carpeta de arriba)
                self._ok(self.api.CONACreateFolder(self.fs, C.c_wchar_p(p), C.c_wchar_p(_ruta(actual))),
                         "crear %s/%s" % (actual, p))
            actual = actual + "\\" + p

    def subir(self, archivo, carpeta):
        """Copia un archivo de la PC a una carpeta del telefono (pisa el que haya)."""
        archivo = os.path.abspath(archivo)
        nombre = os.path.basename(archivo)
        if self.existe(carpeta.rstrip("\\") + "\\" + nombre):
            self.borrar(carpeta.rstrip("\\") + "\\" + nombre)
        self._ok(self.api.CONACopyFile(self.fs, PC_A_TELEFONO, C.c_wchar_p(nombre),
                                       C.c_wchar_p(os.path.dirname(archivo)), C.c_wchar_p(_ruta(carpeta))),
                 "subir %s" % archivo)

    # ---------------------------------------------------------------------------------------------- instalar
    def instalar(self, sisx, opciones=INSTALAR_DESDE_CARPETA):
        """Manda el .sisx al telefono y arranca su instalador, que sigue solo en el telefono (puede pedir confirmar).
        Vuelve cuando termino de copiarlo, no cuando se instalo: ver esperar_archivo."""
        sisx = os.path.abspath(sisx)
        info = _Sis(os.path.basename(sisx), os.path.dirname(sisx))
        # CONAInstallApplication(fs, tipo, estructura del tipo, opciones, carpeta de origen, carpeta de destino)
        self._ok(self.api.CONAInstallApplication(self.fs, APLICACION_SIS, C.byref(info), opciones,
                                                 C.c_wchar_p(os.path.dirname(sisx)), None),
                 "instalar %s" % sisx)


def esperar_archivo(t, ruta, segundos, antes=None):
    """Espera a que 'ruta' aparezca en el telefono (o cambie de tamano, si se da 'antes'). True si aparecio."""
    carpeta, nombre = _partir(ruta)
    fin = time.time() + segundos
    while time.time() < fin:
        try:
            for n, _, b in t.listar(carpeta):
                if n.lower() == nombre.lower() and b != antes:
                    return True
        except ErrorN95:
            pass
        time.sleep(2)
    return False


LOG_JUEGO = "E:\\crashw3d\\crashw3d.log"


def leer_log(t, desde=0, ruta=LOG_JUEGO):
    """Las lineas del log del juego (empieza de cero en cada arranque); [] si todavia no existe."""
    if not t.existe(ruta):
        return []
    with tempfile.TemporaryDirectory() as tmp:
        p = t.bajar(ruta, tmp)
        with open(p, "rb") as f:
            lineas = f.read().decode("utf-8", "replace").splitlines()
    return lineas[desde:]


def main(args):
    if not args:
        print(__doc__)
        return 1
    t = N95()
    try:
        cmd = args[0]
        if cmd == "info":
            print("%s (%s), serie %s" % (t.nombre, t.modelo, t.serie))
            for n, _, _ in t.listar(""):
                print("  unidad", n)
        elif cmd == "ls":
            for n, carpeta, b in t.listar(args[1]):
                print(("  [%s]" % n) if carpeta else ("  %-40s %10d" % (n, b)))
        elif cmd == "bajar":
            print(t.bajar(args[1], args[2] if len(args) > 2 else "."))
        elif cmd == "subir":
            t.subir(args[1], args[2])
        elif cmd == "borrar":
            t.borrar(args[1])
        elif cmd == "instalar":
            t0 = time.time()
            t.instalar(args[1])
            print("instalado en %.1f s" % (time.time() - t0))
        elif cmd == "log":
            desde = int(args[args.index("--desde") + 1]) if "--desde" in args else 0
            for l in leer_log(t, desde):
                print(l)
        else:
            print(__doc__)
            return 1
    finally:
        t.cerrar()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ErrorN95 as e:
        print("N95:", e)
        sys.exit(2)
