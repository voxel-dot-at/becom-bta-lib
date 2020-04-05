#CPP = $(CROSS_COMPILE)g++

OUTDIR = ../lib/
SONAME = libbta.so.3
LIBNAME = libbta.so.3.0.5

CFLAGS += -Wall -Werror -fPIC -DPLAT_LINUX -O3
#CFLAGS += -std=c99 -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE
CFLAGS += -I. -I../inc -I../common
LFLAGS += -shared -fPIC -lrt -lpthread -static-libgcc -Wl,-soname,$(SONAME)

# **** ETH support ****
#CFLAGS += -DBTA_WO_ETH

# **** USB support ****
CFLAGS += -DBTA_WO_USB
#LFLAGS += -lusb-1.0

# **** UART support ****
CFLAGS += -DBTA_WO_UART

# **** STREAM support ****
CFLAGS += -DBTA_WO_STREAM

# **** JPG support ****
CFLAGS += -DBTA_WO_LIBJPEG
#CFLAGS += -I../libjpeg-turbo
#CFLAGS += -I../libjpeg-turbo/build
#LFLAGS += -L../libjpeg-turbo/build/.libs
#LFLAGS += -l:libjpeg.a


BTA_CODE = $(wildcard *.c) $(filter-out ../common/ping.c, $(wildcard ../common/*.c))
BTA_OBJECTS = $(BTA_CODE:%.c=%.o)
BTA_HEADERS = $(wildcard ../inc/*.h) $(wildcard *.h) $(filter-out ../common/ping.h, $(wildcard ../common/*.h))


# Builds object files
%.o: %.c $(BTA_HEADERS) Makefile
	$(CXX) $(CFLAGS) -c $< -o $@


libbta.so: $(BTA_OBJECTS) Makefile
	mkdir -p $(OUTDIR)
	$(CXX) -o $(OUTDIR)$(LIBNAME) $(BTA_OBJECTS) $(LFLAGS)
	cd $(OUTDIR) && rm -f libbta.so.tar.gz
	cd $(OUTDIR) && ldconfig -n .
	cd $(OUTDIR) && ln -sf $(SONAME) libbta.so
	cd $(OUTDIR) && ln -sf $(LIBNAME) $(SONAME)
	cd $(OUTDIR) && tar -czvf libbta.so.tar.gz *



.PHONY: clean
clean:
	rm -f $(BTA_OBJECTS)
	rm -f $(OUTDIR)*


.PHONY: cb
CFLAGS += -DNDEBUG
cb: clean libbta.so


.PHONY: debug
debug: CFLAGS += -DDEBUG -ggdb -g
debug: clean libbta.so


.PHONY: help
help:
	@echo
	@echo '   === BltTofApi Makefile help ==='
	@echo
	@echo '   make clean.............. Delete object files and library'
	@echo '   make ................... Build library'
	@echo '   make libbta.so ......... Build library'
	@echo '   make cb ................ clean and build'
	@echo '   make debug ............. clean and build with cflag -ggdb and -g (NDEBUG still defined, sorry)'
	@echo
