# d-tacq projects top level makefile include
# $file:$

ifeq ($(CROSS),"")
CROSS := NO
endif

ifeq (YES, ${CROSS})
        TOOLPREFIX=arm-xscale-linux-gnu-
	OUTDIR=xscale
	ARCH=xscale
else
        TOOLPREFIX=
	OUTDIR=x86
	ARCH=x86
endif

OBJDIR := $(OUTDIR)/OBJ
EXEDIR := $(OUTDIR)/bin
LIBDIR := $(OUTDIR)/lib

CC     := ${TOOLPREFIX}gcc
CXX    := ${TOOLPREFIX}g++
LD     := ${TOOLPREFIX}g++
AR     := ${TOOLPREFIX}ar rc
RANLIB := ${TOOLPREFIX}ranlib
STRIP  := ${TOOLPREFIX}strip

DEBUG_CFLAGS     := -Wall -Wno-unknown-pragmas -Wno-format -g -DDEBUG
RELEASE_CFLAGS   := -Wall -Wno-unknown-pragmas -Wno-format -O3

ifeq (YES, ${CROSS})
DEBUG_CFLAGS += -D__arm
RELEASE_CFLAGS += -D__arm
endif

DEBUG_CXXFLAGS   := ${DEBUG_CFLAGS}
RELEASE_CXXFLAGS := ${RELEASE_CFLAGS}

DEBUG_LDFLAGS    := -g
RELEASE_LDFLAGS  :=



ifeq (YES, ${DEBUG})
   CFLAGS       := ${DEBUG_CFLAGS}
   CXXFLAGS     := ${DEBUG_CXXFLAGS}
   LDFLAGS      := ${DEBUG_LDFLAGS}
else
   CFLAGS       := ${RELEASE_CFLAGS}
   CXXFLAGS     := ${RELEASE_CXXFLAGS}
   LDFLAGS      := ${RELEASE_LDFLAGS}
endif

ifeq (YES, ${PROFILE})
   CFLAGS   := ${CFLAGS} -pg -O3
   CXXFLAGS := ${CXXFLAGS} -pg -O3
   LDFLAGS  := ${LDFLAGS} -pg
endif

CFLAGS   := ${CFLAGS}   ${DEFS}
CXXFLAGS := ${CXXFLAGS} ${DEFS}

X_IGNORE := $(shell mkdir -p $(OBJDIR) $(EXEDIR) $(LIBDIR))

lib: CFLAGS += -fPIC
lib: CXXFLAGS += -fPIC
