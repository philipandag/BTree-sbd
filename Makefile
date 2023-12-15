FILES=App.cpp Btree.cpp main.cpp
HEADERS=App.hpp Btree.hpp
debug= -fsanitize=address

all: $(FILES) $(HEADERS)
	g++ $(FILES) -o main.out -Wall -g 

clean:
	rm *.out
	rm *.o


