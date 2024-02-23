# Compiler
CC = gcc

# Compilation flags
CFLAGS = -Wall -Werror -pedantic -std=gnu18

# Your login
LOGIN = alfaro

# Path to submit files
SUBMITPATH = ~cs537-1/handin/alfaro/P3
# Source files
SRCS = wsh.c
HEADERS = wsh.h

# Target for compilation
all: wsh

# Target to build wsh binary
wsh: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

# Target to run wsh binary
run: wsh
	./wsh

# Target to create a tar.gz archive
pack: wsh.h wsh.c Makefile README.md
	tar -czvf $(LOGIN).tar.gz $^

# Target to submit the solution
submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)

# Target for running tests (create your own tests)
test: wsh
	~cs537-1/tests/P3/test-cd.csh
	~cs537-1/tests/P3/test-exec.csh
	~cs537-1/tests/P3/test-job-control.csh -c -v
	~cs537-1/tests/P3/test-pipe.csh
# Phony targets
.PHONY: all run pack submit test

# Clean target to remove generated files
clean:
	rm -f wsh $(LOGIN).tar.gz
