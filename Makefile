FILES=App.cpp Btree.cpp main.cpp
HEADERS=App.hpp Btree.hpp

all: $(FILES) $(HEADERS)
	g++ $(FILES) -o main.out -Wall -g -fsanitize=address

clean:
	rm *.out
	rm *.o


