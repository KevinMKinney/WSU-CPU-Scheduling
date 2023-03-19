.SUFFIXES: .c .o
CCFLAGS = -std=c99 -pedantic -Wall -Werror
OPTIONS = -g

build:
	gcc ${CCFLAGS} ${OPTIONS} -o cpuSchedule cpuSchedule.c

clean:
	rm -f cpuSchedule

# to run valgrind/check for memory problems run the following:
# valgrind --track-origins=yes --leak-check=full ./"my-file-name" "args..."