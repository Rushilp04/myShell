all: compile
compile: p3.c
	gcc -Wall -Werror -fsanitize=address,undefined p3.c -o mysh