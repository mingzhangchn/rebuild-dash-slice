

SRC=mp4_rebuild.cpp p test.cpp 
OBJ=mp4_rebuild.o  test.o 

APP=mp4rebuild


INCLUDES = -I./include

.PONEY:all

all:$(APP)



$(APP):$(OBJ) 
	@g++  $(OBJ) ./lib/libmp4v2.a -o $@ 

    
%.o:%.cpp
	g++ -c  -fPIC $(INCLUDES) $< -o $@
	
%.o:%.c
	gcc -c $(INCLUDES) $< -o $@    
    
clean:
	@rm -rf $(OBJ) $(APP)
	
	