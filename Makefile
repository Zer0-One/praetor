#
# Makefile for praetor.
#

#May be set to either 'clang' or 'gcc', but `make analyze` will not run without
#clang's "scan-build" utility installed.
cc = clang
test_sources = test/*.c test/unity/src/*.c

commit_hash='"$(shell git log -n 1 --pretty=format:%H)"'
praetor_version='"0.1.0"'

.PHONY: all all-debug analyze deps docs test clean

praetor: bin/praetor
praetor-debug: bin/praetor_debug

bin/praetor : src/*.c include/*.h
		mkdir -p bin
		$(cc) -O3 -fPIE -pie -fstack-protector-strong -Wl,-z,now -Wl,-z,relro -std=c99 -pedantic-errors -Wall -Wextra -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -Iinclude/ -ljansson -ltls src/*.c -o $@
		chmod +x bin/praetor

bin/praetor_debug : src/*.c include/*.h
		mkdir -p bin
		$(cc) -g3 -std=c99 -pedantic-errors -Wall -Wextra -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -Iinclude/ -ljansson -ltls src/*.c -o $@
		chmod +x bin/praetor_debug

analyze : src/*.c
		scan-build -v -o analysis $(cc) -g3 -std=c99 -pedantic-errors -Wall -Wextra -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -Iinclude/ -ljansson -ltls $+ -o praetor
		chmod +x bin/praetor

test :
		chmod +x test/unity/auto/*
		ruby test/unity/auto/generate_test_runner.rb test/tests.c test/test_runner.c
		$(cc) -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -I test/unity/src $(test_sources) -o test/test_runner
		chmod +x test/test_runner
		./test/test_runner

docs :
		mkdir -p doc
		doxygen Doxyfile

clean : 
		-rm -r bin
		-find doc/* -type d -exec rm -rf {} +
		-rm test/test_runner
		-rm test/test_runner.c

all : clean docs praetor test
all-debug : clean docs praetor-debug test
