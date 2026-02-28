DEBUG = 0
DEBUG_ASAN = 0
DEBUG_UBSAN = 0
HAVE_GRIFFIN = 0
# fastcall only works on x86_32
FASTCALL = 0
# fastmath should improve performance with every arch, and there is no known issue with fbneo
FASTMATH = 1
USE_SPEEDHACKS = 1
EXTERNAL_ZLIB = 0
INCLUDE_7Z_SUPPORT = 1
# enable x86_64 dynarec used in killer instinct
USE_X64_DRC = 0
AUTOGEN_DATS = 0
HAVE_NEON = 0
USE_EXPERIMENTAL_FLAGS = 0
USE_CYCLONE = 0
USE_C68K = 1
USE_CZ80 = 1
SUPPORT_LARGE_FILES = 1
# Use a temporary file to hold the list of objects, as it can exceed windows shell command limits
SPLIT_UP_LINK ?= 0
REGEN_HEADERS ?= 0

SPACE :=
SPACE := $(SPACE) $(SPACE)
BACKSLASH :=
BACKSLASH := \$(BACKSLASH)
filter_out1 = $(filter-out $(firstword $1),$1)
filter_out2 = $(call filter_out1,$(call filter_out1,$1))

# system platform
ifeq ($(platform),)
	platform = unix
	ifeq ($(shell uname -a),)
		platform = win
		EXE_EXT=.exe
	else ifneq ($(findstring Darwin,$(shell uname -a)),)
		platform = osx
		arch = intel
		ifeq ($(shell uname -p),arm)
			arch = arm
		endif
		ifeq ($(shell uname -p),powerpc)
			arch = ppc
		endif
	else ifneq ($(findstring MINGW,$(shell uname -a)),)
		platform = win
		EXE_EXT=.exe
	endif
else ifneq (,$(findstring armv,$(platform)))
	ifeq (,$(findstring classic_,$(platform)))
		override platform += unix
	endif
else ifneq (,$(findstring rpi,$(platform)))
	override platform += unix
endif

MAIN_FBNEO_DIR	:= ../..
VERSION_SCRIPT	:= $(MAIN_FBNEO_DIR)/burner/libretro/link.T

# TARGET
ifeq ($(SUBSET),)
	TARGET_NAME := fbneo
	INCLUDED_MAKEFILE = Makefile.all
else
	TARGET_NAME := fbneo_$(SUBSET)
	INCLUDED_MAKEFILE = $(SUBSET).mk
endif

# Unix
ifneq (,$(findstring unix,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared -Wl,-no-undefined -Wl,--version-script=$(VERSION_SCRIPT)
	ENDIANNESS_DEFINES := -DLSB_FIRST
	LDFLAGS += -lpthread

# Windows
else
ifeq ($(STATIC_LINKING),1)
	TARGET := lib$(TARGET_NAME)_libretro.a
else
	TARGET := $(TARGET_NAME)_libretro.dll
endif
	CC ?= gcc
	CXX ?= g++
	SHARED := -shared -Wl,-no-undefined -Wl,--version-script=$(VERSION_SCRIPT)
	LDFLAGS += -static-libgcc -static-libstdc++
	SPLIT_UP_LINK = 1
endif

ENDIANNESS_DEFINES := -DLSB_FIRST
# some platforms (3ds, maybe others) are unable to find "main" entry symbol if you build with their sdk
# so let's use standard compiler for header generation since this is cross-built anyway
CC_SYSTEM = gcc
CXX_SYSTEM = g++

include Makefile.common
include $(INCLUDED_MAKEFILE)

FBNEO_CXXOBJ := $(SOURCES_CXX:.cpp=.o)
FBNEO_COBJ := $(SOURCES_C:.c=.o)
FBNEO_SOBJ := $(SOURCES_S:.S=.o)

OBJS := $(FBNEO_COBJ) $(FBNEO_CXXOBJ) $(FBNEO_SOBJ)
DEPS := $(OBJS:.o=.d)

FBNEO_DEFINES += -D__LIBRETRO__ \
	$(ENDIANNESS_DEFINES) \
	$(PLATFORM_DEFINES) \
	$(NEON_FLAGS) \
	$(ARM_FLAGS)

INCFLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

ifeq ($(USE_SPEEDHACKS), 1)
	FBNEO_DEFINES += -DUSE_SPEEDHACKS
endif

ifeq ($(FASTCALL), 1)
	FBNEO_DEFINES += -DFASTCALL
endif

ifeq ($(FASTMATH), 1)
	ifeq (,$(findstring msvc,$(platform)))
		# 2019-06-26 :
		# we got some evidence that -ffast-math is actually not safe for fbneo
		# adding -frounding-math to fix arithmetic in neogeo_run.cpp, it might fix other stuff
		# clang might need a minimum version for -frounding-math support
		CFLAGS += -ffast-math -frounding-math
		CXXFLAGS += -ffast-math -frounding-math
	endif
endif

ifeq ($(USE_EXPERIMENTAL_FLAGS), 1)
	EXPERIMENTAL_FLAGS = -fwhole-program -fuse-linker-plugin \
		-fdata-sections -ffunction-sections -Wl,--gc-sections \
		-fno-stack-protector -fno-ident \
		-falign-functions=1 -falign-jumps=1 -falign-loops=1 \
		-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-unroll-loops \
		-fmerge-all-constants -fno-math-errno
	ifeq ($(shell $(CC) -v 2>&1 | grep -c "gcc version"), 1)
		EXPERIMENTAL_FLAGS += -flto=4
	endif
	CFLAGS += $(EXPERIMENTAL_FLAGS)
	CXXFLAGS += $(EXPERIMENTAL_FLAGS)
endif

ifeq ($(DEBUG_ASAN), 1)
	DEBUG = 1
	DEBUG_UBSAN = 0
	CFLAGS += -fsanitize=address
	CXXFLAGS += -fsanitize=address
	LDFLAGS += -lasan -fsanitize=address
endif

ifeq ($(DEBUG_UBSAN), 1)
	DEBUG = 1
	CFLAGS += -fsanitize=undefined
	CXXFLAGS += -fsanitize=undefined
	LDFLAGS += -lubsan -fsanitize=undefined
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g -DFBNEO_DEBUG
	CXXFLAGS += -O0 -g -DFBNEO_DEBUG
else
	ifeq (,$(findstring msvc,$(platform)))
		# sadly, we can't use -Os for memory constrained systems here, the performance impact is too big
		CFLAGS += -O3 -DNDEBUG -fomit-frame-pointer
		CXXFLAGS += -O3 -DNDEBUG -fomit-frame-pointer
	else
		CFLAGS += -O2 -DNDEBUG
		CXXFLAGS += -O2 -DNDEBUG
	endif
endif

CFLAGS += $(fpic) $(FBNEO_DEFINES) -MMD
CXXFLAGS += $(fpic) $(FBNEO_DEFINES) -MMD
LDFLAGS += $(fpic)

ifeq (,$(findstring msvc,$(platform)))
	CFLAGS += -fforce-addr  \
		-Wall -Wno-long-long -Wno-sign-compare -Wno-uninitialized -Wno-unused \
		-Wno-sequence-point -Wno-strict-aliasing
	CXXFLAGS += -fforce-addr \
		-Wall -W -Wshadow -Wno-long-long -Wno-write-strings \
		-Wunknown-pragmas -Wundef -Wno-conversion -Wno-missing-braces -Wno-multichar \
		-Wuninitialized -Wpointer-arith -Wno-inline -Wno-unused-value \
		-Wno-sequence-point -Wno-extra -Wno-strict-aliasing

	ifeq (,$(filter $(platform), ps3 sncps3))
		CFLAGS += -Wno-write-strings -Wno-pedantic
		CXXFLAGS += -Wno-write-strings -pedantic -Wno-address -Wno-narrowing -Wno-pedantic
	endif
endif

# gcc specific flags
ifeq ($(shell $(CC) -v 2>&1 | grep -c "gcc version"), 1)
	CFLAGS += -ffloat-store
	CXXFLAGS += -ffloat-store
	ifeq ($(shell expr `$(CC) -dumpversion | cut -f1 -d.` \>= 5), 1)
		CFLAGS += -finline-limit=1200 -fcheck-new
		CXXFLAGS += -finline-limit=1200 -fcheck-new
	endif
endif

PERL = perl$(EXE_EXT)
PGM_SPRITE_CREATE_EXE = pgmspritecreate$(EXE_EXT)
EXE_PREFIX = ./

# backward compatibility (make less than 4.4 doesn't support .WAIT)
.WAIT:

.PHONY: all clean generate-files generate-files-clean clean-objs

ifeq ($(REGEN_HEADERS), 1)
all: generate-files .WAIT $(TARGET)
else
all: $(TARGET)
endif

generate-files-clean:
	rm -rf $(FBNEO_GENERATED_DIR)/
	rm -rf $(MAIN_FBNEO_DIR)/../gamelist.txt

generate-files:
	@mkdir -p $(FBNEO_GENERATED_DIR) 2>/dev/null || /bin/true
	@echo "Generating $(FBNEO_GENERATED_DIR)/driverlist.h..."
	@echo ""
	$(PERL) $(FBNEO_SCRIPTS_DIR)/gamelist.pl -o $(FBNEO_GENERATED_DIR)/driverlist.h -l $(MAIN_FBNEO_DIR)/../gamelist.txt $(SOURCES_CXX)
	$(CXX_SYSTEM) $(GENERATE_OPTS) -o $(PGM_SPRITE_CREATE_EXE) $(FBNEO_BURN_DRIVERS_DIR)/pgm/pgm_sprite_create.cpp
	@echo ""
	@echo "Generating $(FBNEO_GENERATED_DIR)/pgm_sprite.h..."
	@echo ""
	$(EXE_PREFIX)$(PGM_SPRITE_CREATE_EXE) > $(FBNEO_GENERATED_DIR)/pgm_sprite.h

OBJOUT   = -o
LINKOUT  = -o

ifneq (,$(findstring msvc,$(platform)))
	OBJOUT = -Fo
	LINKOUT = -out:
ifeq ($(STATIC_LINKING),1)
	LD ?= lib.exe
	STATIC_LINKING=0

	ifeq ($(DEBUG), 1)
		CFLAGS += -MTd
		CXXFLAGS += -MTd
	else
		CFLAGS += -MT
		CXXFLAGS += -MT
	endif
else
	LD = link.exe

	ifeq ($(DEBUG), 1)
		CFLAGS += -MDd
		CXXFLAGS += -MDd
	else
		CFLAGS += -MD
		CXXFLAGS += -MD
	endif
endif
else
	LD = $(CXX)
endif

%.o: %.c
	@$(CC) -c $(OBJOUT)$@ $< $(CFLAGS) $(INCFLAGS)

%.o: %.cpp
	@$(CXX) -c $(OBJOUT)$@ $< $(CXXFLAGS) $(INCFLAGS)

%.o: %.S
	@$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
ifeq ($(STATIC_LINKING), 1)
ifeq ($(SPLIT_UP_LINK), 1)
	$(AR) rcs $@ $(foreach OBJS,$(OBJS),$(NEWLINE) $(AR) q $@ $(OBJS))
else
	$(AR) rcs $@ $(OBJS)
endif
else
ifeq ($(SPLIT_UP_LINK), 1)
	$(file >$@.in,$(OBJS))
	$(LD) $(LINKOUT)$@ $(SHARED) @$@.in $(LDFLAGS) $(LIBS)
	@rm $@.in
else
	$(LD) $(LINKOUT)$@ $(SHARED) $^ $(LDFLAGS) $(LIBS)
endif
endif

-include $(DEPS)

clean-objs:
ifeq ($(SPLIT_UP_LINK), 1)
	$(file >$@.in,$(OBJS) $(DEPS))
	xargs rm -f <$@.in
	rm -f $@.in
else
	rm -f $(OBJS) $(DEPS)
endif

clean: clean-objs
	rm -f $(TARGET)
	rm -f $(PGM_SPRITE_CREATE_EXE)
