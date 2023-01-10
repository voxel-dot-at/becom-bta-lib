CPP = gcc

#i.MX8 cross-compile-toolchain
#PF_SYSROOT=/usr/share/musl-1.2.0-aarch64-llvm8_0.0.1/sysroot/
#CPP = clang-8 --sysroot=$(PF_SYSROOT) --target=aarch64-linux-musleabi -I $(PF_SYSROOT)/include/c++/v1 -nostdlib
#LFLAGS = -L$(PF_SYSROOT)/lib -fuse-ld=lld -lc -lc++ -lunwind -lc++abi $(PF_SYSROOT)/lib/crt1.o $(PF_SYSROOT)/lib/linux/libclang_rt.builtins-aarch64.a

OUTDIR = lib/
SONAME = libbta.so.3
LIBNAME = libbta.so.3.3.6

CFLAGS += -Wall -fPIC -DPLAT_LINUX -O3 -Werror
#CFLAGS += -D_POSIX_C_SOURCE=200809L
#CFLAGS += -std=c99
CFLAGS += -Isdk -Iinc -Icommon
LFLAGS += -fPIC -lrt -lpthread -lm -static-libgcc -shared -Wl,-soname,$(SONAME)

CFLAGS += -DNDEBUG
#CFLAGS += -DDEBUG -ggdb -g

BTA_CODE = sdk/bta.c sdk/bta_frame_queueing.c sdk/bta_helper.c sdk/bta_discovery_helper.c sdk/bta_grabbing.c sdk/bta_processing.c sdk/bta_serialization.c
BTA_CODE += common/bcb_circular_buffer.c common/bitconverter.c common/bta_jpg.c common/bta_oshelper.c common/bvq_queue.c common/calc_bilateral.c
BTA_CODE += common/calcXYZ.c common/crc16.c common/crc32.c common/crc7.c common/fifo.c common/ping.c common/pthread_helper.c common/sockets_helper.c common/timing_helper.c common/undistort.c common/utils.c
BTA_CODE += common/fastBF/fspecial_gauss.c common/fastBF/imfilter.c common/fastBF/maxFilter.c common/fastBF/shiftableBF.c

# **** with(out) ETH support ****
#CFLAGS += -DBTA_WO_ETH
BTA_CODE += sdk/bta_eth.c sdk/bta_eth_helper.c

# **** with(out) legacy P100 USB support ****
CFLAGS += -DBTA_WO_P100
#LFLAGS += -lusb
#BTA_CODE += sdk/bta_p100.c sdk/bta_p100_helper.c

# **** with(out) USB support ****
#CFLAGS += -DBTA_WO_USB
LFLAGS += -lusb-1.0
BTA_CODE += sdk/bta_usb.c

# **** with(out) UART support ****
CFLAGS += -DBTA_WO_UART
#BTA_CODE += sdk/bta_uart.c sdk/bta_uart_helper.c

# **** with(out) STREAM support ****
#CFLAGS += -DBTA_WO_STREAM
BTA_CODE += sdk/bta_stream.c

# **** with(out) JPG support ****
CFLAGS += -DBTA_WO_LIBJPEG
#CFLAGS += -Ilibjpeg-turbo -Ilibjpeg-turbo/build
#LFLAGS += -Llibjpeg-turbo/build/.libs -l:libjpeg.a


BTA_OBJECTS = $(BTA_CODE:%.c=%.o)
BTA_HEADERS = $(wildcard inc/*.h) $(wildcard sdk/*.h) $(wildcard common/fastBf/*.h) $(filter-out common/ping.h, $(wildcard common/*.h))


# Builds object files
%.o: %.c $(BTA_HEADERS) Makefile
	$(CPP) $(CFLAGS) -c $< -o $@


libbta.so: $(BTA_OBJECTS) Makefile
	mkdir -p $(OUTDIR)
	$(CPP) -o $(OUTDIR)$(LIBNAME) $(BTA_OBJECTS) $(LFLAGS)
	cd $(OUTDIR) && rm -f libbta.so.tar.gz
	cd $(OUTDIR) && ldconfig -n .
	cd $(OUTDIR) && ln -sf $(SONAME) libbta.so
	cd $(OUTDIR) && ln -sf $(LIBNAME) $(SONAME)
	cd $(OUTDIR) && tar -czvf libbta.so.tar.gz *


.PHONY: all
all: clean libbta.so

.PHONY: clean
clean:
	rm -rf ./sdk/*.o
	rm -rf ./common/*.o
	rm -rf ./common/fastBF/*.o
	rm -f $(OUTDIR)*
    

.PHONY: help
help:
	@echo
	@echo '   === BltTofApi Makefile help ==='
	@echo
	@echo '   make ................... Build library'
	@echo '   make clean.............. Delete object files and library'
	@echo '   make libbta.so ......... Build library'
#	@echo '   make debug ............. clean and build with cflag -ggdb and -g (NDEBUG still defined, sorry)'
	@echo
