.SUFFIXES: .c .o
CCFLAGS = -std=c99 -pedantic -Wall -Werror -pthread
OPTIONS = -g

build:
	gcc ${CCFLAGS} ${OPTIONS} -o exec cpuSchedule.c

clean:
	rm -f exec

# to run valgrind/check for memory problems run the following:
# valgrind --track-origins=yes --leak-check=full ./"my-file-name" "args..."
