#
# Makefile for praetor.
#
# Compiler: gcc

cc = gcc
test_sources = test/*.c test/unity/src/*.c

commit_hash='"$(shell git log -n 1 --pretty=format:%H)"'
praetor_version='"$(shell cat VERSION)"'

.PHONY: all all-debug deps docs test clean

praetor: bin/praetor
praetor-debug: bin/praetor_debug

bin/praetor: src/*.c src/util/*.c include/*.h
		-mkdir bin
		$(cc) -O2 -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -Iinclude/ -ljansson -ltls $+ -o $@
		chmod +x bin/praetor

bin/praetor_debug: src/*.c src/util/*.c include/*.h
		-mkdir bin
		$(cc) -g -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -Iinclude/ -ljansson -ltls $+ -o $@
		chmod +x bin/praetor_debug

deps:
		cd deps/libressl/ && ./autogen.sh && ./configure && make -j4 check && sudo make install

test:
		chmod +x test/unity/auto/*
		ruby test/unity/auto/generate_test_runner.rb test/tests.c test/test_runner.c
		$(cc) -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -I test/unity/src $(test_sources) -o test/test_runner
		chmod +x test/test_runner
		./test/test_runner

docs :
		-mkdir doc
		doxygen Doxyfile

clean : 
		-rm -r bin
		-rm -r doc
		-rm test/test_runner
		-rm test/test_runner.c

all : clean docs praetor test
all-debug : clean docs praetor-debug test
