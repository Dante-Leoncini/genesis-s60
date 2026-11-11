# ---------------------------------------------------------------------------------------------------------------
#  segaw3d: la PRUEBA EN LA PC del emulador del N95 (el telefono se compila con symbian/compilar.bat).
#  El mismo codigo (src/ en C++03 y C, el nucleo de picodrive/ en C, sin el asm de ARM) en una ventana de SDL2 con
#  OpenGL 1.x, el sonido por el mezclador del Core de Whisk3D.
#
#      make                -> build/segaw3d_pc
#      ./build/segaw3d_pc [--datos carpeta/] [--tam 640x480] [--capturas 100,200] [--salir-en 300] [--teclas 60:start]
#  En Windows: desde PowerShell con C:\msys64\mingw64\bin en el PATH (SDL2 de pacman).
# ---------------------------------------------------------------------------------------------------------------
W3D     ?= $(abspath ../../Whisk3D)
CORE    := $(W3D)/libs/Whisk3DCore
PD      := picodrive
SALIDA  := build
OBJDIR  := $(SALIDA)/obj
TARGET  := $(SALIDA)/segaw3d_pc

# el nucleo de PicoDrive: Mega Drive, Master System / Game Gear, Mega CD y 32X (el nucleo no compila sin ellos),
# los 68000 / Z80 en C (FAME y CZ80), el sonido, el zip y la letra de 8x8 (para el menu)
PD_C := pico/pico.c pico/cart.c pico/memory.c pico/state.c pico/sek.c pico/z80if.c pico/videoport.c \
        pico/draw2.c pico/draw.c pico/mode4.c pico/misc.c pico/eeprom.c pico/patch.c pico/debug.c pico/media.c \
        pico/sms.c \
        pico/32x/32x.c pico/32x/memory.c pico/32x/draw.c pico/32x/sh2soc.c pico/32x/pwm.c \
        cpu/sh2/sh2.c cpu/sh2/mame/sh2pico.c \
        pico/cd/mcd.c pico/cd/memory.c pico/cd/sek.c pico/cd/cdc.c pico/cd/cdd.c pico/cd/cd_image.c \
        pico/cd/cd_parse.c pico/cd/gfx.c pico/cd/gfx_dma.c pico/cd/misc.c pico/cd/pcm.c pico/cd/megasd.c \
        pico/pico/pico.c pico/pico/memory.c pico/pico/xpcm.c \
        pico/carthw/carthw.c pico/carthw/eeprom_spi.c \
        pico/carthw/svp/svp.c pico/carthw/svp/memory.c pico/carthw/svp/ssp16.c \
        pico/sound/sound.c pico/sound/resampler.c pico/sound/sn76496.c pico/sound/ym2612.c pico/sound/ym2413.c \
        pico/sound/vgm.c pico/sound/mix.c \
        cpu/fame/famec.c cpu/cz80/cz80.c cpu/drc/cmn.c \
        zlib/gzio.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c zlib/trees.c zlib/deflate.c zlib/crc32.c \
        zlib/adler32.c zlib/zutil.c zlib/compress.c zlib/uncompr.c \
        unzip/unzip.c \
        platform/libpicofe/fonts.c
PD_DEFS := -DEMU_F68K -D_USE_CZ80 -DNDEBUG
PD_INCS := -I$(PD) -I$(PD)/zlib

FUENTES := $(wildcard src/*.cpp) src/pc/main_pc.cpp
FUENTES_C := $(wildcard src/*.c)
CORE_CPP := base/w3dlog.cpp io/w3dFilesystem.cpp audio/W3dAudio.cpp audio/W3dAudioSDL.cpp

OBJS := $(addprefix $(OBJDIR)/,$(FUENTES:.cpp=.o)) $(addprefix $(OBJDIR)/,$(FUENTES_C:.c=.o)) \
        $(addprefix $(OBJDIR)/core/,$(CORE_CPP:.cpp=.o)) $(addprefix $(OBJDIR)/pd/,$(PD_C:.c=.o))
HEADERS := $(wildcard src/*.h)

INCS := -Isrc -I$(CORE) -I$(CORE)/base -I$(CORE)/audio -I$(CORE)/io
DEFS := -DW3D_SIN_EDITOR -DW3D_ENABLE_AUDIO -Wno-cpp
ifneq ($(findstring MINGW,$(shell uname -s))$(findstring MSYS,$(shell uname -s))$(OS),)
GL_LIBS := -lopengl32
STD_CORE := gnu++17
else
GL_LIBS := -lGL -lm -lpthread
STD_CORE := gnu++98
endif
# C++03 como el RVCT 2.2 del N95 (lo que compila aca y no alla, se ve aca primero)
CXXFLAGS_J := -O2 -g -std=gnu++98 $(DEFS) $(INCS) -Wall -Wno-unused-function $(shell sdl2-config --cflags)
CFLAGS_J := -O2 -g -std=gnu99 $(INCS) $(PD_INCS) $(PD_DEFS) -Wall
CXXFLAGS_C := -O2 -g -std=$(STD_CORE) $(DEFS) $(INCS) -w $(shell sdl2-config --cflags)
CFLAGS_PD := -O2 -g -std=gnu99 $(PD_INCS) $(PD_DEFS) -w -fno-strict-aliasing
LIBS := $(shell sdl2-config --libs) $(GL_LIBS)

all: $(TARGET)

$(PD)/pico/carthw_cfg.c: $(PD)/pico/carthw.cfg herramientas/carthw_cfg.py
	python herramientas/carthw_cfg.py $< $@

$(OBJDIR)/pd/pico/cart.o: $(PD)/pico/carthw_cfg.c

$(OBJDIR)/src/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	@echo "  c++ $<"
	@$(CXX) $(CXXFLAGS_J) -c $< -o $@

$(OBJDIR)/src/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	@echo "  cc $<"
	@$(CC) $(CFLAGS_J) -c $< -o $@

$(OBJDIR)/core/%.o: $(CORE)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  c++ core/$*"
	@$(CXX) $(CXXFLAGS_C) -c $< -o $@

$(OBJDIR)/pd/%.o: $(PD)/%.c
	@mkdir -p $(dir $@)
	@echo "  cc pd/$*"
	@$(CC) $(CFLAGS_PD) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "  link $@"
	@$(CXX) -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
