EXEC=aw_gp32.exe
PGM=${ARCH}/$(EXEC)

MANAGERS=semaphore timer event io

DEFINES+= -DSYS_LITTLE_ENDIAN -DGP32_PORT

CSRCS = sound.c
COBJS_ = $(CSRCS:.c=.o)
COBJS = $(COBJS_:%=${ARCH}/%)

CXXSRCS = engine.cpp file.cpp graphics.cpp main.cpp mixer.cpp resource.cpp \
	serializer.cpp scaler.cpp script.cpp staticres.cpp \
	splash.cpp systemstub_gp32.cpp sfxplayer.cpp util.cpp
CXXOBJS_ = $(CXXSRCS:.cpp=.o)
CXXOBJS = $(CXXOBJS_:%=${ARCH}/%)

ASSRCS = xlatgp32.S
ASOBJS_ = $(ASSRCS:.S=.o)
ASOBJS = $(ASOBJS_:%=${ARCH}/%)

BSRCS = rootfs.tar
BOBJS_ = $(BSRCS:.tar=.o)
BOBJS = $(BOBJS_:%=${ARCH}/%)

include $(RTEMS_MAKEFILE_PATH)/Makefile.inc
include $(RTEMS_CUSTOM)
include $(PROJECT_ROOT)/make/leaf.cfg

OBJS = $(COBJS) $(CXXOBJS) $(ASOBJS) $(BOBJS)

LD_LIBS += -lz
CXXFLAGS += -Wall -O -g -MMD

${ARCH}/%.o: %.tar
	$(OBJCOPY) -I binary -O elf32-littlearm  -B armv4t $< $@

all:	${ARCH} $(PGM)

$(PGM):	${OBJS} ${LINK_FILES}
	$(make-cxx-exe)

install: $(PGM)
	gplink put $(basename $(PGM)).gxb / -x

