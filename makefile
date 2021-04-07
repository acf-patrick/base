.SUFFIXE:

CXX = g++-8
CFLAGS = -std=c++2a -W

CORE = core/ecs/component/componentManager.cpp\
 core/ecs/entity/entity.cpp\
 core/application/application.cpp\
 core/entry/entry.cpp
 
CLIENT = test.cpp

HEADER = core/
LIBS = libs/

LIB = -ltmx -lyaml-cpp -lSDL2 #-lSDL2_image -lSDL2_ttf -lSDL2_mixer
DEP = -lxml2 -lz

SRC = $(CORE) $(CLIENT)

ifeq ($(OS), Windows_NT)
	L = -L$(LIBS)win/ $(LIB) $(DEP)
	HEADER = $(HEADER) core/tmx/ core/tmx/zlib/
else
	L = -L$(LIBS)linux/ $(LIB) -L. $(DEP)
endif

all : obj
	@echo "... Linkage ..."
	$(CXX) obj/* $(L) -o bin

obj: $(SRC)
	@echo "... Compiling ..."
	@mkdir -p obj
	$(CXX) $(CFLAGS) -I $(HEADER) -c $?
	@mv *.o obj/

clean:
	@rm -f bin -r obj/