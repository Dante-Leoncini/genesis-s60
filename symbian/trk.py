"""El N95 por TRK (el agente de depuracion de Nokia, App TRK 3.1.2 para S60 3rd FP1) por el puerto serie del cable
USB. En el telefono tiene que estar App TRK abierto y conectado por USB ("Status: Connected").

    python trk.py correr programa.exe [segundos]   lo lanza y espera a que termine (o lo mata a los N segundos)

Lo que se aprendio con App TRK 3.1 en el N95:
- Instalar sin preguntar: anda con runonphone del Qt SDK (-p COM7 -s archivo.sisx; lo copia el mismo por TRK). Con el
  .sisx copiado por ConnAPI, la instalacion de TRK da "error del sistema" (0x20).
- Lanzar un programa instalado en E: da "error del sistema": el juego se arranca con FR,RI en su .pkg.
- Archivos: escribir anda (subir(), como runonphone copia el .sisx). Para leer (bajar()) hay que abrirlos para leer
  Y escribir: abiertos solo para leer, App TRK no los cierra (0x20) y quedan trabados hasta reiniciar el telefono.
  Asi los datos, la prueba y el log van por aca cuando el ConnAPI del PC Suite no contesta.

El protocolo (el de Metrowerks/Nokia que usaban Carbide y Qt Creator): cada mensaje es [comando][numero][datos]
[suma] escapado con 0x7D, entre dos 0x7E; por USB, con la cabecera 01 90 <largo>. Los enteros, en big-endian. Cada
mensaje se contesta con un ACK (0x80, el mismo numero, y un byte de error: 0x10 comando desconocido, 0x20 error del
sistema); el telefono tambien avisa cosas (un programa que carga una biblioteca, que termino) y esos avisos se
contestan con un ACK.

Hace falta pyserial (python -m pip install pyserial). El puerto se busca solo ("Nokia ... USB Serial Port")."""
import os
import struct
import sys
import time

import serial
from serial.tools import list_ports

# los comandos
PING, CONECTAR, DESCONECTAR, VERSIONES, SOPORTADO, CPU = 0x00, 0x01, 0x02, 0x04, 0x05, 0x06
CONTINUAR = 0x18
CREAR, BORRAR = 0x40, 0x41
ESCRIBIR, LEER, ABRIR, CERRAR = 0x48, 0x49, 0x4A, 0x4B
ACK, NAK = 0x80, 0xFF
# los avisos del telefono
PARADO, EXCEPCION, ERROR_INTERNO, CREADO, BORRADO = 0x90, 0x91, 0x92, 0xA0, 0xA1
AVISOS = (PARADO, EXCEPCION, ERROR_INTERNO, CREADO, BORRADO, 0xA2, 0xA6, 0xA7)



class ErrorTrk(Exception):
    pass


def _escapar(b):
    o = bytearray()
    for c in b:
        if c in (0x7E, 0x7D):
            o += bytes((0x7D, c ^ 0x20))
        else:
            o.append(c)
    return bytes(o)


def _texto(s, nulo=False):
    b = s.encode('latin-1') + (b'\0' if nulo else b'')
    return struct.pack('>H', len(b)) + b


def buscar_puerto():
    for p in list_ports.comports():
        # (solo el del N95: puede haber otro Nokia enchufado, como el N8)
        d = (p.description or '').lower()
        if 'nokia' in d and 'n95' in d and 'serial' in d:
            return p.device
    raise ErrorTrk('no encuentro el puerto serie del N95 (cable USB en modo PC Suite)')


class Trk:
    def __init__(self, puerto=None, esperar=0):
        # (el puerto a veces lo tiene otro programa un momento -Carbide lo mira de a ratos-: se reintenta 'esperar' s)
        fin = time.time() + esperar
        while True:
            try:
                self.s = serial.Serial(puerto or buscar_puerto(), 115200, timeout=0.2)
                break
            except (serial.SerialException, ErrorTrk):
                if time.time() > fin:
                    raise
                time.sleep(0.3)
        self.numero = 0
        self.resto = bytearray()
        self.avisos = []
        # el primer ping va con el numero 0; despues, desconectar (por si quedo una sesion) y la presentacion
        self._mandar(PING, b'', numero=0)
        self._esperar_ack(0, 5)
        try:
            self.pedir(DESCONECTAR)
        except ErrorTrk:
            pass   # (no habia sesion)
        self.pedir(SOPORTADO)
        self.pedir(CPU)

    def cerrar(self):
        self.s.close()

    # ----------------------------------------------------------------------------------------------- tramas
    def _mandar(self, comando, datos, numero=None):
        if numero is None:
            self.numero = self.numero % 255 + 1
            numero = self.numero
        cuerpo = bytes((comando, numero)) + datos
        cuerpo += bytes((255 - (sum(cuerpo) & 0xFF),))
        cod = _escapar(cuerpo)
        self.s.write(b'\x01\x90' + struct.pack('>H', len(cod) + 2) + b'\x7e' + cod + b'\x7e')
        return numero

    def _leer_trama(self, hasta):
        while time.time() < hasta:
            # (cabecera 01 90 <largo>, despues 7E ... 7E)
            i = self.resto.find(b'\x01\x90')
            if i >= 0 and len(self.resto) >= i + 4:
                n = struct.unpack('>H', bytes(self.resto[i + 2:i + 4]))[0]
                if len(self.resto) >= i + 4 + n:
                    t = bytes(self.resto[i + 4:i + 4 + n])
                    del self.resto[:i + 4 + n]
                    t = t.strip(b'\x7e')
                    o, esc = bytearray(), False
                    for c in t:
                        if esc:
                            o.append(c ^ 0x20)
                            esc = False
                        elif c == 0x7D:
                            esc = True
                        else:
                            o.append(c)
                    if len(o) >= 3:
                        return o[0], o[1], bytes(o[2:-1])
                    continue
            self.resto += self.s.read(4096)
        return None

    def _atender(self, comando, numero, datos):
        """Un aviso del telefono: se contesta y se guarda."""
        self._mandar(ACK, b'\x00', numero=numero)
        self.avisos.append((comando, datos))

    def _esperar_ack(self, numero, segundos):
        hasta = time.time() + segundos
        while True:
            t = self._leer_trama(hasta)
            if t is None:
                raise ErrorTrk('el telefono no contesta (App TRK abierto y conectado por USB?)')
            c, n, d = t
            if c in AVISOS:
                self._atender(c, n, d)
            elif c in (ACK, NAK) and n == numero:
                if c == NAK:
                    raise ErrorTrk('NAK')
                return d

    def pedir(self, comando, datos=b'', segundos=10):
        """Manda un comando y devuelve los datos de su ACK (sin el byte de error); error != 0 es una excepcion."""
        d = self._esperar_ack(self._mandar(comando, datos), segundos)
        if d and d[0] != 0:
            raise ErrorTrk('comando 0x%02X: error %d' % (comando, d[0]))
        return d[1:]

    # ---------------------------------------------------------------------------------------------- archivos
    def subir(self, local, remoto, al_avanzar=None):
        """Copia un archivo al telefono (lo crea o lo pisa), como runonphone el .sisx. Abrir (escribir | binario)
        contesta [?][mango 4][fecha 4]; se escribe de a 2 KB ([mango][largo 2][datos]) y se cierra con el mango y una
        fecha de 4 bytes (la de la apertura)."""
        with open(local, 'rb') as f:
            datos = f.read()
        d = self.pedir(ABRIR, bytes((0x0A,)) + _texto(remoto), segundos=20)
        mango, fecha = d[1:5], d[5:9]
        try:
            for i in range(0, len(datos), 2048):
                parte = datos[i:i + 2048]
                self.pedir(ESCRIBIR, mango + struct.pack('>H', len(parte)) + parte, segundos=20)
                if al_avanzar:
                    al_avanzar(i + len(parte), len(datos))
        finally:
            self.pedir(CERRAR, mango + (fecha if len(fecha) == 4 else bytes(4)), segundos=20)

    def bajar(self, remoto):
        """Lee un archivo del telefono. Se abre para LEER Y ESCRIBIR (0x0B): abierto solo para leer, App TRK no lo
        puede cerrar (error 0x20) y queda trabado hasta reiniciar."""
        d = self.pedir(ABRIR, bytes((0x0B,)) + _texto(remoto), segundos=20)
        mango, fecha = d[1:5], d[5:9]
        datos = bytearray()
        try:
            while True:
                r = self.pedir(LEER, mango + struct.pack('>H', 2048), segundos=20)
                # (la respuesta: [?][largo 2][datos])
                n = struct.unpack('>H', r[1:3])[0]
                datos += r[3:3 + n]
                if n < 2048:
                    break
        finally:
            self.pedir(CERRAR, mango + (fecha if len(fecha) == 4 else bytes(4)), segundos=20)
        return bytes(datos)

    # ---------------------------------------------------------------------------------------------- programas
    def correr(self, programa, argumentos='', segundos=0):
        """Lo lanza y espera a que termine (segundos > 0: lo mata al pasar ese tiempo). Devuelve el aviso final."""
        linea = programa.encode('latin-1') + b'\0' + argumentos.encode('latin-1')
        d = self.pedir(CREAR, struct.pack('>HB', 0, 0) + struct.pack('>H', len(linea) + 1) + linea + b'\0')
        pid, tid = struct.unpack('>II', d[:8])
        self.avisos = []
        self.pedir(CONTINUAR, struct.pack('>II', pid, tid))
        fin = time.time() + segundos if segundos > 0 else None
        while True:
            t = self._leer_trama(time.time() + 1)
            if t:
                c, n, dd = t
                if c in AVISOS:
                    self._atender(c, n, dd)
                    if c == CREADO:
                        # (una biblioteca que cargo: el programa queda parado hasta que se le dice que siga)
                        self._mandar(CONTINUAR, struct.pack('>II', pid, tid))
                    elif c == BORRADO:
                        return 'termino'
                    elif c in (PARADO, EXCEPCION):
                        return 'se paro: %s' % dd.hex()
            if fin and time.time() > fin:
                self.pedir(BORRAR, struct.pack('>HI', 0, pid))
                return 'lo mate a los %d s' % segundos


def main(a):
    if not a:
        print(__doc__)
        return 1
    t = Trk()
    try:
        if a[0] == 'correr':
            print(t.correr(a[1], segundos=int(a[2]) if len(a) > 2 else 0))
        else:
            print(__doc__)
            return 1
    finally:
        t.cerrar()
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except ErrorTrk as e:
        print('TRK:', e)
        sys.exit(2)
