########################################################
#
#  Makefile for drsLog applications
#  Sawaiz Syed
#  executables under linux
#
########################################################

DOS           = OS_LINUX

CFLAGS        = -g -O2 -Wall -Wuninitialized -fno-strict-aliasing -Iinclude -I/usr/local/include -D$(DOS) -DHAVE_USB -DHAVE_LIBUSB10
LIBS          = -lpthread -lutil -lusb-1.0

CPP_OBJ       = DRS.o averager.o
OBJECTS       = musbstd.o mxml.o strlcpy.o

all: drsLog

drsLog: $(OBJECTS) DRS.o averager.o drsLog.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o drsLog.o -o drsLog $(LIBS)

drsLog.o: src/drsLog.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

$(CPP_OBJ): %.o: src/%.cpp include/%.h include/DRS.h
	$(CXX) $(CFLAGS) $(WXFLAGS) -c $<

$(OBJECTS): %.o: src/%.c include/DRS.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o drsLog *.dat *.root
