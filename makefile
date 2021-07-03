export CXX := clang++
export CXXFLAGS := -g -std=c++17 -Wall -Wextra -Weffc++ -pedantic -pthread -fno-rtti -fno-exceptions -march=native -DVK_NO_PROTOTYPES -DMVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS=0 -DMVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER=1 -DUSING_MOLTEN_VK # -I$(SDL2PATH)
export LDFLAGS := #-lSDL2




export TARGET_BINARY := rp
export OBJ := main.o render.o volk.o vma.o
export SHADER_OBJ := 


.PHONY: all clean


all: $(TARGET_BINARY)


$(TARGET_BINARY): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET_BINARY) $(OBJ)

depend:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -E -MM $(OBJ:.o=.cpp) > .depend

clean:
	-rm *.o
	-rm rp

include .depend