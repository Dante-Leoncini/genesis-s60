#
# El icono de la aplicacion: un mando de Mega Drive (symbian\gfx\segaw3d_icono.svg) en
# \resource\apps\segaw3d_icono.mif, armado como el del editor de Whisk3D (platform\symbian\group\Icons_scalable_dc.mk)
#

ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\segaw3d_icono.mif

ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

# /V3: el mifconv del SDK de Symbian^3 pasa el SVG a NVG (solo lo entienden los Symbian^3) y en el N95 el icono sale
# roto; /V3 deja el SVGB de siempre
RESOURCE :
	mifconv $(ICONTARGETFILENAME) \
		/V3 /c32 $(ICONDIR)\segaw3d_icono.svg

FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing
