CC=gcc
CXX=g++
RM= /bin/rm -vf
ARCH=UNDEFINED
PWD=pwd
CDR=$(shell pwd)

EDCFLAGS:=$(CFLAGS)
EDLDFLAGS:=$(LDFLAGS)
EDDEBUG:=$(DEBUG)

ifeq ($(ARCH),UNDEFINED)
	ARCH=$(shell uname -m)
endif

UNAME_S := $(shell uname -s)

EDCFLAGS+= -I include/ -I ./ -Wall -O2 -std=gnu11 -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W
CXXFLAGS:= -I include/ -I ./imgui/include -I ./ -Wall -O2 -fpermissive -std=gnu++11 -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W $(CXXFLAGS)
LIBS = -lpthread -lm

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL `pkg-config --static --libs glfw3`
	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	CXXFLAGS:= -arch $(ARCH) $(CXXFLAGS)
	LIBS += -arch $(ARCH) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

GUILIB=imgui/libimgui_glfw.a # ImGui library with glfw backend
PLOTLIB=imgui/libimplot.a # ImPlot library (backend agnostic)

all: $(GUILIB) $(PLOTLIB) test # $(PLOTLIB)
	echo Platform: $(ECHO_MESSAGE)

test: $(GUILIB) $(PLOTLIB) # Build the OpenGL2 test program
	$(CXX) -o test.out test.cpp $(CXXFLAGS) $(GUILIB) \
	$(LIBS)


$(GUILIB): # Build the ImGui library
	cd imgui && make -f Makefile.imgui && cd ..

$(PLOTLIB): # Build the ImPlot library
	cd imgui && make -f Makefile.implot && cd ..

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<
%.o: %.c
	$(CC) $(EDCFLAGS) -o $@ -c $<
	
.PHONY: clean

clean:
	$(RM) test.out

spotless: clean
	cd imgui && make -f Makefile.imgui spotless && cd ..
	cd imgui && make -f Makefile.implot spotless && cd ..

