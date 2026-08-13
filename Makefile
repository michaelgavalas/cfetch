CC = gcc
objects = cfetch.o
target = app

all: $(objects)
	$(CC) $^ -o $(target)

$(objects): %.o: %.c
	$(CC) -c $^ -o $@

clean:
	rm -f *.o $(target)
