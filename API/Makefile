
# Override on the command line as required.
CROSS := NO
DEBUG := NO
MDSPLUS := YES
MDSPLUS := NO

include ../d-top.mak

LIBS             :=

CONTROL_APPS	:= acqcmd acq2sh
APPS		= $(CONTROL_APPS) \
			acqread \
			api_test acq196_api_test \
			streamer \
			acq_demux \
			acq_demux_continuous			

ifeq ($(MDSPLUS),YES)
APPS	+= 	acq_demux-mds \
			acq_demux-mds-continuous
endif			



ifeq ($(CROSS),YES)
	LIBPATH=$(LIBDIR) -L../lib/
else
#	APPS += dir-watch
	LIBPATH=$(LIBDIR)
endif


DC := $(shell date +%Y%m%d%H%M)
PACKAGE_FILE := ACQ2XX_API.$(DC).tgz

##CXXFLAGS += -I../include -DWITH_NONAMESPACES
CXXFLAGS += -I../include -DAPI
#CXXFLAGS += -I../include -DAPI -pg

# NB: no library yet, that just makes debugging harder!.

ifeq ($(CROSS), NO)
ifeq ($(MDSPLUS),YES)
	CXXFLAGS += -I/usr/local/mdsplus/include
	APPS += acq_demux-mds
	MDS_LIBS=-L/usr/local/mdsplus/lib -lMdsObjectsCppShr -fexceptions -Wl,-E
endif
endif

ifeq ($(DEBUG),YES)
	CXXFLAGS+=-g
endif

LIBNAME=acq2xx_api
LIBSO=lib$(LIBNAME).so
SONAME=$(LIBSO).1
LIBFILE=$(SONAME).0.1

LIB_SRC=acq2xx_api.cpp acq_transport.cpp \
	Dt100Transport.cpp connectto.cpp \
	KddTransport.cpp \
	SoapTransport.cpp \
	Frame.cpp \
	AcqType.cpp \
	WorkingDir.cpp \
	EnvFile.cpp \
	SHM_Buffer.cpp 

DATAMODEL_SRC := AcqDataModel.cpp Acq132.cpp AcqCal.cpp

ACQCMD_SRC	:= acqcmd.cpp $(LIB_SRC)
ACQ2SH_SRC	:= acq2sh.cpp $(LIB_SRC)
ACQREAD_SRC	:= acqread.cpp $(LIB_SRC)
API_TEST_SRC	:= api_test.cpp $(LIB_SRC)
API196_TEST_SRC	:= acq196_api_test.cpp $(LIB_SRC)
STREAMER_SRC	:= streamer.cpp DataStreamer.cpp \
	DirfileFrameHandler.cpp  $(DATAMODEL_SRC) $(LIB_SRC)
ACQ_DEMUX_SRC	:= \
	acq_demux.cpp ProcessController.cpp CommandBuffer.cpp \
	$(DATAMODEL_SRC) $(LIB_SRC)
ACQ_DEMUX_MDS_SRC := \
	$(ACQ_DEMUX_SRC) MdsProcessController.cpp  
ACQ_DEMUX_CONTINUOUS_SRC := \
	acq_demux.cpp ProcessController.cpp CommandBuffer.cpp \
	$(DATAMODEL_SRC) $(LIB_SRC)\
	ContinuousProcessController.cpp

CLIENT_SRC=acq2xx_client.cpp $(CLIENT_STUBS) 
APITEST_SRC=acq2xx_api_test.cpp $(CLIENT_STUBS)

SHM_BUFFER_TEST_SRC := SHM_BufferTest.cpp SHM_Buffer.cpp

SERVER_OBJ=$(patsubst %,$(OBJDIR)/%,$(SERVER_SRC:.cpp=.o))
CLIENT_OBJ=$(patsubst %,$(OBJDIR)/%,$(CLIENT_SRC:.cpp=.o))
APITEST_OBJ=$(patsubst %,$(OBJDIR)/%,$(APITEST_SRC:.cpp=.o))
LIB_OBJ=$(patsubst %,$(OBJDIR)/%,$(LIB_SRC:.cpp=.o))


all: 
	$(MAKE) app





lib: $(LIBDIR)/$(LIBFILE)

$(LIBDIR)/$(LIBFILE): $(LIB_OBJ)
	${CXX} -shared -Wl,-soname,$(SONAME) $(CXXFLAGS) \
	-o $(LIBDIR)/$(LIBFILE) $^ -lstdc++
	${STRIP} $(LIBDIR)/${LIBFILE}
	-(cd $(LIBDIR);ln -s ${LIBFILE} ${SONAME};ln -s $(LIBFILE) $(LIBSO))



app: $(patsubst %,$(EXEDIR)/%, $(APPS))


$(EXEDIR)/acq2xx_api_test: $(APITEST_OBJ)
	$(CXX) -o $@ $^ -L$(LIBPATH)

$(EXEDIR)/acqcmd: $(patsubst %,$(OBJDIR)/%,$(ACQCMD_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 
#-l $(LIBNAME)

$(EXEDIR)/acq2sh: $(patsubst %,$(OBJDIR)/%,$(ACQ2SH_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 
#-l $(LIBNAME)

$(EXEDIR)/acqread: $(patsubst %,$(OBJDIR)/%,$(ACQREAD_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/api_test: $(patsubst %,$(OBJDIR)/%,$(API_TEST_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/acq196_api_test: $(patsubst %,$(OBJDIR)/%,$(API196_TEST_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/streamer: $(patsubst %,$(OBJDIR)/%,$(STREAMER_SRC:.cpp=.o))
	$(CXX) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/acq_demux: $(patsubst %,$(OBJDIR)/%,$(ACQ_DEMUX_SRC:.cpp=.o))
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/acq_demux-mds: $(patsubst %,$(OBJDIR)/%,$(ACQ_DEMUX_MDS_SRC:.cpp=.o))
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH) $(MDS_LIBS) -l popt 

$(EXEDIR)/acq_demux-mds-continuous: $(patsubst %,$(OBJDIR)/%,$(ACQ_DEMUX_MDS_SRC:.cpp=.o))
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH) $(MDS_LIBS) -l popt 

$(EXEDIR)/acq_demux_continuous: $(patsubst %,$(OBJDIR)/%,$(ACQ_DEMUX_CONTINUOUS_SRC:.cpp=.o))
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH) -l popt 

$(EXEDIR)/SHM_BufferTest: $(patsubst %,$(OBJDIR)/%,$(SHM_BUFFER_TEST_SRC:.cpp=.o))
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH)

SHM_BufferTest: $(EXEDIR)/SHM_BufferTest
	echo DONE

$(EXEDIR)/dir-watch:  $(patsubst %,$(OBJDIR)/%,dir-watch.o)
	[ -f /usr/include/inotifytools/inotifytools.h ] \
		|| echo WARNING: yum install inotify-tools-devel
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBPATH) -linotifytools -l popt 

x86_links:
	-ln -s ./x86/bin/acq2xx_client acq2sh
	-ln -s ./x86/bin/acq2xx_client acqcmd
	-ln -s ./x86/bin/acq2xx_client acqstat
	-ln -s ./x86/bin/acq2xx_client setArm
ifeq ($(MDSPLUS),YES)	
	-(cd ./x86/bin; ln -s acq_demux acq_demux-mds)
	-(cd ./x86/bin; ln -s acq_demux acq_demux-mds-continuous)
endif

clean:
	make CROSS=NO clean2
	make CROSS=YES clean2

clean2:
	rm -f $(OBJDIR)/* $(EXEDIR)/* $(LIBDIR)/*

distclean: clean


deploy:
	$(MAKE) CROSS=YES DEBUG=NO
	mkdir -p release/lib release/cgi-bin release/bin
	arm-xscale-linux-gnu-strip xscale/bin/*
	cp xscale/bin/* release/bin; rm release/bin/acqcmd
	cp -a release/bin/*	../../rootfs/extra/local/bin
	cp *.def ../../rootfs/extra/local/etc/sysconfig/
	./make.xscale.aliases ../../rootfs/extra/local/bin
doc:
	doxygen

install_x86:
	mkdir -p $(X86_INSTALL_DIR)/bin $(X86_INSTALL_DIR)/lib
	cp -a x86/bin/* $(X86_INSTALL_DIR)/bin
	(cd $(X86_INSTALL_DIR)/bin;\
		ln -s ./acq2xx_client acq2sh;\
		ln -s ./acq2xx_client acqcmd;\
		ln -s ./acq2xx_client acqstat;\
		ln -s ./acq2xx_client setArm;\
	)	 
	cp -a x86/lib/* $(X86_INSTALL_DIR)/lib
	echo export LD_LIBRARY_PATH=$(X85_INSTALL_DIR)/lib
	echo export PATH=$(X86_INSTALL_DIR)/bin/:$$PATH

tarball: clean
	(cd ..; tar cvzf /tmp/$(PACKAGE_FILE)  \
		--exclude-from ./API/nopack ./API *.mak include)
	git tag -a API_$(DC)
	mv /tmp/$(PACKAGE_FILE) release
	echo tarball $(PACKAGE_FILE) created

release: tarball

package: tarball


# install_rtm_t: commands for use with rtm-t private network

rtm_t:	$(patsubst %,$(EXEDIR)/%, $(CONTROL_APPS))

install_rtm_t: rtm_t
	cp $(EXEDIR)/acq2sh /usr/local/bin/acq2sh.ip
	cp $(EXEDIR)/acqcmd /usr/local/bin/acqcmd.ip

include ../d-bot.mak
