SRC_CPP=$(wildcard TetrisBot/*.cpp)
SRC_O=$(SRC_CPP:.cpp=.o)

build: $(SRC_O)
	em++ --bind -O2 $(SRC_O) -o Tetris.js

test: build
	node Tetris.js

clean:
	rm -f $(SRC_O)

%.o: %.cpp
	echo $? -> $@
	em++ -O2 $? -o $@ -std=c++0x