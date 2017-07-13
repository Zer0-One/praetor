FROM debian
MAINTAINER zero-one <zero-one@zer0-one.net>

# Update/upgrade, and install dependencies
RUN apt-get update && apt-get -y dist-upgrade
RUN apt-get -y install git libjansson4 libjansson-dev doxygen graphviz clang ruby automake autoconf libtool make
RUN cd / && git clone https://github.com/libressl-portable/portable.git
RUN cd portable && ./autogen.sh && ./configure && make check && make install && ldconfig

# Clone and build praetor
RUN cd / && git clone https://github.com/Zer0-One/praetor.git
RUN cd praetor && make

# For a custom config, simply edit config/praetor.conf before building
ADD config/praetor.conf /praetor/config/

# To include plugins, specify a mountpoint path in the config, and mount a volume there at runtime
ENTRYPOINT ["/praetor/bin/praetor", "-f", "-d", "-c", "/praetor/config/praetor.conf"]
