#---------------------------------------------------------------------------------
# JheraldBootSound - Makefile de sysmodule (devkitA64 + libnx)
#
# Produce:  JheraldBootSound.nsp   (= exefs.nsp: PFS0 con { main, main.npdm })
#
# Uso:
#   export DEVKITPRO=/opt/devkitpro         (Linux/macOS)
#   set    DEVKITPRO=C:/devkitPro           (Windows, en MSYS2 de devkitPro)
#   make            -> compila
#   make dist       -> regenera por completo ./dist/ (reproducible y limpio)
#   make clean      -> borra artefactos de build y ./dist/
#
# Este Makefile es de un solo directorio de fuentes (source/). Sin libs externas.
#
# NOTA sobre 'make dist' y 'clean':
#   Ambos hacen 'rm -rf' UNICAMENTE sobre $(CURDIR)/dist (ruta del proyecto,
#   absoluta y verificada con un guard). Nunca tocan rutas externas ni la SD.
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Define DEVKITPRO en el entorno. Ej: export DEVKITPRO=/opt/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Config del proyecto
#---------------------------------------------------------------------------------
TARGET   := JheraldBootSound
BUILD    := build
SOURCES  := source
INCLUDES := include

# Program ID (debe coincidir con JheraldBootSound.json). Usado por 'make dist'.
PROGRAM_ID := 42000000004A4253

VERSION  := 0.4.0-fase4.0

#---------------------------------------------------------------------------------
# Flags de compilacion
#---------------------------------------------------------------------------------
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

DEFINES := -D__SWITCH__ -DVERSION=\"$(VERSION)\"

CFLAGS  := -g -Wall -Wextra -O2 -ffunction-sections -fdata-sections \
           $(ARCH) $(DEFINES) $(INCLUDE)

CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -std=gnu++20

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) \
           -Wl,--gc-sections -Wl,-Map,$(notdir $*.map)

LIBS    := -lnx

#---------------------------------------------------------------------------------
LIBDIRS := $(PORTLIBS) $(LIBNX)
#---------------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# switch_rules genera main.npdm a partir de este JSON y lo mete en el PFS0.
export APP_JSON := $(TOPDIR)/$(TARGET).json

.PHONY: all dist clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

#---------------------------------------------------------------------------------
# make dist: regenera ./dist/ DESDE CERO. Contenido exacto y reproducible:
#
#   dist/
#   |-- atmosphere/contents/$(PROGRAM_ID)/
#   |   |-- exefs.nsp                 (copia de $(TARGET).nsp recien compilado)
#   |   `-- flags/boot2.flag          (archivo vacio, 0 bytes)
#   `-- config/JheraldBootSound/
#       |-- config.ini.example        (copia de package/config.ini.example)
#       `-- README_AUDIO.txt          (copia de package/README_AUDIO.txt)
#
# NO incluye .gitkeep ni ningun placeholder: al recrearse desde cero no existen.
# NO incluye startup.wav ni debug.flag: los aporta/activa el usuario en la SD.
#---------------------------------------------------------------------------------
DIST_ROOT    := $(CURDIR)/dist
DIST_CONTENT := $(DIST_ROOT)/atmosphere/contents/$(PROGRAM_ID)
DIST_CONFIG  := $(DIST_ROOT)/config/JheraldBootSound
PKG_SRC      := $(CURDIR)/package

dist: all
	@# --- guard: solo se permite borrar exactamente $(CURDIR)/dist ---
	@test -n "$(CURDIR)" || { echo "CURDIR vacio, abortando"; exit 1; }
	@case "$(DIST_ROOT)" in \
	  "$(CURDIR)/dist") : ;; \
	  *) echo "ruta dist inesperada ($(DIST_ROOT)); abortando"; exit 1 ;; \
	esac
	@rm -rf "$(DIST_ROOT)"
	@mkdir -p "$(DIST_CONTENT)/flags" "$(DIST_CONFIG)"
	@: > "$(DIST_CONTENT)/flags/boot2.flag"
	@cp "$(TARGET).nsp" "$(DIST_CONTENT)/exefs.nsp"
	@cp "$(PKG_SRC)/config.ini.example" "$(DIST_CONFIG)/config.ini.example"
	@cp "$(PKG_SRC)/README_AUDIO.txt"   "$(DIST_CONFIG)/README_AUDIO.txt"
	@echo "dist/ regenerado:"
	@find "$(DIST_ROOT)" -type f -printf '  %P  (%s bytes)\n' | sort

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nsp $(TARGET).nso $(TARGET).npdm $(TARGET).elf $(TARGET).map
	@test -n "$(CURDIR)" || { echo "CURDIR vacio, abortando"; exit 1; }
	@case "$(CURDIR)/dist" in \
	  "$(CURDIR)/dist") rm -rf "$(CURDIR)/dist" ;; \
	esac

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nsp

$(OUTPUT).nsp : $(OUTPUT).nso $(OUTPUT).npdm
$(OUTPUT).nso : $(OUTPUT).elf
$(OUTPUT).elf : $(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
