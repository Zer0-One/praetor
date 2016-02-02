#
# Makefile for praetor. The 'install' and 'uninstall' targets have only been
# tested on systems with a GNU userland. Edit it to suit your needs.
#
# Compiler: gcc

cc = gcc
sources = src/*.c src/util/*.c
bin = bin

commit_hash='"$(shell git log -n 1 --pretty=format:%H)"'
praetor_version='"$(shell cat VERSION)"'

all : clean docs praetor
all-debug : clean docs praetor-debug

docs :
		mkdir doc
		doxygen Doxyfile

praetor:
		-mkdir bin
		$(cc) -O2 -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) -ljansson $(sources) -o $(bin)/praetor
		chmod +x $(bin)/praetor

praetor-debug:
		-mkdir bin
		$(cc) -g -std=c99 -pedantic-errors -Wall -D_XOPEN_SOURCE=600 -DCOMMIT_HASH=$(commit_hash) -DPRAETOR_VERSION=$(praetor_version) $(sources) -o $(bin)/praetor_debug
		chmod +x $(bin)/praetor_debug

clean : 
		-rm -r bin
		-rm -r doc

# should probably leave this out, or auto-generate it for specific platform
#install :
#		echo "Please enter your root password to continue..."
#		su
#		-useradd -r -s /bin/false praetor
#		-mkdir /etc/praetor
#		-mkdir -p /usr/share/praetor/plugins
#		-mkdir /var/lib/praetor
#		-cp config/* /etc/praetor/
#		-chown -R praetor:praetor /etc/praetor/* /usr/share/praetor/plugins /var/lib/praetor
#		-chmod -R 660 /etc/praetor/*
#		cp bin/praetor /usr/bin/
#
#uninstall :
#		echo "Please enter your root password to continue..."
#		su
#		-userdel -f praetor
#		-rm -r /etc/praetor
#		-rm -r /usr/share/praetor
#		-rm -r /var/lib/praetor
#		-rm /usr/bin/praetor
