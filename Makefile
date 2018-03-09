

SRC=mp4_rebuild.cpp p test.cpp 
OBJ=mp4_rebuild.o  test.o 

APP=test
LIB=libmp4rebuild.so


INCLUDES = -I./include

.PONEY:all

all:$(APP) 

$(LIB):$(OBJ)
	@g++  -shared $(OBJ) ./lib/libmp4v2.a -o $@ 

$(APP):$(OBJ) 
	@g++  $(OBJ) ./lib/libmp4v2.a -o $@ 

    
%.o:%.cpp
	g++ -c  -fPIC $(INCLUDES) $< -o $@
	
%.o:%.c
	gcc -c $(INCLUDES) $< -o $@    
    
clean:
	@rm -rf $(OBJ) $(APP)
	
	