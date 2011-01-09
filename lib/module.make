# name of module
#	normaly this would be set in the root Makefile and always called
#	MP, but for compatibilitis sake, i can not use the needed :=
MPL = lib

AES_TABS = \
	$(MPL)/aes_ft_tab.bin \
	$(MPL)/aes_fl_tab.bin \
	$(MPL)/aes_it_tab.bin \
	$(MPL)/aes_il_tab.bin

TCHAR_TABS_BE = \
	$(MPL)/tchar_tolower_be.bin \
	$(MPL)/tchar_c1table_be.bin \
	$(MPL)/tchar_c3table_be.bin

TCHAR_TABS = \
	$(MPL)/tchar_tolower.bin \
	$(MPL)/tchar_c1table.bin \
	$(MPL)/tchar_c3table.bin

TARED_FILES += \
	$(MPL)/module.make \
	$(MPL)/aes_tab_gen.c \
	$(MPL)/five_tab_gen.c \
	$(TCHAR_TABS_BE) \
	$(LIBHEADS) \
	$(LIBASRCS)

TARED_DIRS += \
	$(MPL) \
	$(MPL)/generic \
	$(MPL)/unaligned \
	$(MPL)/x86 \
	$(MPL)/mips \
	$(MPL)/ppc \
	$(MPL)/parisc \
	$(MPL)/sparc \
	$(MPL)/ia64 \
	$(MPL)/alpha \
	$(MPL)/arm

# dependencies for all files
LIB_STD_DEPS = \
	$(MPL)/module.make \
	Makefile \
	ccdrv

# all header
LIBHEADS = \
	$(MPL)/other.h \
	$(MPL)/my_bitops.h \
	$(MPL)/my_bitopsm.h \
	$(MPL)/my_epoll.h \
	$(MPL)/my_pthread.h \
	$(MPL)/tchar.h \
	$(MPL)/log_facility.h \
	$(MPL)/sec_buffer.h \
	$(MPL)/swab.h \
	$(MPL)/unaligned.h \
	$(MPL)/combo_addr.h \
	$(MPL)/config_parser.h \
	$(MPL)/recv_buff.h \
	$(MPL)/udpfromto.h \
	$(MPL)/guid.h \
	$(MPL)/hzp.h \
	$(MPL)/atomic.h \
	$(MPL)/ansi_prng.h \
	$(MPL)/aes.h \
	$(MPL)/byteorder.h \
	$(MPL)/itoa.h \
	$(MPL)/rbtree.h \
	$(MPL)/list.h \
	$(MPL)/hlist.h \
	$(MPL)/hthash.h \
	$(MPL)/backtrace.h \
	$(MPL)/alpha/alpha.h \
	$(MPL)/arm/my_neon.h \
	$(MPL)/mips/my_mips.h \
	$(MPL)/x86/x86_features.h \
	$(MPL)/x86/x86.h \
	$(MPL)/ppc/ppc_altivec.h \
	$(MPL)/ppc/ppc.h \
	$(MPL)/parisc/parisc.h \
	$(MPL)/sparc/sparc_vis.h \
	$(MPL)/ia64/ia64.h \
	$(MPL)/generic/little_endian.h \
	$(MPL)/generic/big_endian.h

# unaligned access
UNALIGNEDSRC = \
	$(MPL)/alpha/unaligned.h \
	$(MPL)/arm/unaligned.h \
	$(MPL)/ia64/unaligned.h \
	$(MPL)/mips/unaligned.h \
	$(MPL)/ppc/unaligned.h \
	$(MPL)/parisc/unaligned.h \
	$(MPL)/sparc/unaligned.h \
	$(MPL)/x86/unaligned.h \
	$(MPL)/generic/unaligned.h \
	$(MPL)/unaligned/access_ok.h \
	$(MPL)/unaligned/be_byteshift.h \
	$(MPL)/unaligned/be_memmove.h \
	$(MPL)/unaligned/be_struct.h \
	$(MPL)/unaligned/generic.h \
	$(MPL)/unaligned/le_byteshift.h \
	$(MPL)/unaligned/le_memmove.h \
	$(MPL)/unaligned/le_struct.h \
	$(MPL)/unaligned/memmove.h \
	$(MPL)/unaligned/packed_struct.h

# epoll emuls
EPOLLSRS = \
	$(MPL)/my_epoll_kepoll.c \
	$(MPL)/my_epoll_devpoll.c \
	$(MPL)/my_epoll_poll.c \
	$(MPL)/my_epoll_pollset.c \
	$(MPL)/my_epoll_eport.c \
	$(MPL)/my_epoll_win.c \
	$(MPL)/my_epoll_kqueue.c

# atomic sources
ATOMSRC = \
	$(MPL)/generic/atomic.h \
	$(MPL)/generic/atomic.c \
	$(MPL)/x86/atomic.h \
	$(MPL)/mips/atomic.h \
	$(MPL)/ia64/atomic.h \
	$(MPL)/ppc/atomic.h \
	$(MPL)/sparc/atomic.h \
	$(MPL)/alpha/atomic.h \
	$(MPL)/arm/atomic.h

# arch src files
AESSRC = \
	$(MPL)/generic/aes.c \
	$(MPL)/arm/aes.c \
	$(MPL)/ppc/aes.c \
	$(MPL)/x86/aes.c
FLSSTSRC = \
	$(MPL)/generic/flsst.c \
	$(MPL)/x86/flsst.c \
	$(MPL)/mips/flsst.c \
	$(MPL)/ppc/flsst.c \
	$(MPL)/alpha/flsst.c \
	$(MPL)/ia64/flsst.c \
	$(MPL)/arm/flsst.c
POPCOUNTSTSRC = \
	$(MPL)/generic/popcountst.c \
	$(MPL)/arm/popcountst.c \
	$(MPL)/x86/popcountst.c \
	$(MPL)/ia64/popcountst.c \
	$(MPL)/sparc/popcountst.c \
	$(MPL)/ppc/popcountst.c \
	$(MPL)/alpha/popcountst.c
CPY_RESTSRC = \
	$(MPL)/generic/cpy_rest.c \
	$(MPL)/ppc/cpy_rest.c \
	$(MPL)/sparc/cpy_rest.c \
	$(MPL)/x86/cpy_rest.c
MEMXORCPYSRC = \
	$(MPL)/generic/memxorcpy.c \
	$(MPL)/arm/memxorcpy.c \
	$(MPL)/x86/memxorcpy.c \
	$(MPL)/x86/memxorcpy_tmpl.c \
	$(MPL)/ppc/memxorcpy.c \
	$(MPL)/sparc/memxorcpy.c
MEMANDSRC = \
	$(MPL)/generic/memand.c \
	$(MPL)/arm/memand.c \
	$(MPL)/x86/memand.c \
	$(MPL)/x86/memand_tmpl.c \
	$(MPL)/ppc/memand.c \
	$(MPL)/sparc/memand.c
MEMNEGSRC = \
	$(MPL)/generic/memneg.c \
	$(MPL)/arm/memneg.c \
	$(MPL)/x86/memneg.c \
	$(MPL)/x86/memneg_tmpl.c \
	$(MPL)/ppc/memneg.c \
	$(MPL)/sparc/memneg.c
MEMCPYSRC = \
	$(MPL)/generic/memcpy.c \
	$(MPL)/sparc/memcpy.c \
	$(MPL)/x86/memcpy_tmpl.c \
	$(MPL)/x86/memcpy.c
MEMPCPYSRC = \
	$(MPL)/generic/mempcpy.c \
	$(MPL)/sparc/mempcpy.c
MEMCPY_REV_SRC = \
	$(MPL)/generic/memcpy_rev.c
MEMCHRSRC = \
	$(MPL)/generic/memchr.c \
	$(MPL)/alpha/memchr.c \
	$(MPL)/arm/memchr.c \
	$(MPL)/ia64/memchr.c \
	$(MPL)/parisc/memchr.c \
	$(MPL)/x86/memchr.c \
	$(MPL)/ppc/memchr.c
MEMPOPCNTSRC = \
	$(MPL)/generic/mempopcnt.c \
	$(MPL)/alpha/mempopcnt.c \
	$(MPL)/arm/mempopcnt.c \
	$(MPL)/mips/mempopcnt.c \
	$(MPL)/sparc/mempopcnt.c \
	$(MPL)/ia64/mempopcnt.c \
	$(MPL)/ppc/mempopcnt.c \
	$(MPL)/x86/mempopcnt.c
MEM_SEARCHRNSRC = \
	$(MPL)/generic/mem_searchrn.c \
	$(MPL)/alpha/mem_searchrn.c \
	$(MPL)/arm/mem_searchrn.c \
	$(MPL)/ia64/mem_searchrn.c \
	$(MPL)/x86/mem_searchrn.c \
	$(MPL)/ppc/mem_searchrn.c
MEM_SPN_FF_SRC = \
	$(MPL)/generic/mem_spn_ff.c \
	$(MPL)/alpha/mem_spn_ff.c \
	$(MPL)/ppc/mem_spn_ff.c \
	$(MPL)/x86/mem_spn_ff.c
STRNLENSRC = \
	$(MPL)/generic/strnlen.c \
	$(MPL)/alpha/strnlen.c \
	$(MPL)/arm/strnlen.c \
	$(MPL)/parisc/strnlen.c \
	$(MPL)/ia64/strnlen.c \
	$(MPL)/mips/strnlen.c \
	$(MPL)/x86/strnlen.c \
	$(MPL)/ppc/strnlen.c
STRLENSRC = \
	$(MPL)/generic/strlen.c \
	$(MPL)/alpha/strlen.c \
	$(MPL)/arm/strlen.c \
	$(MPL)/mips/strlen.c \
	$(MPL)/ia64/strlen.c \
	$(MPL)/parisc/strlen.c \
	$(MPL)/x86/strlen.c \
	$(MPL)/ppc/strlen.c
STRCHRNULSRC = \
	$(MPL)/generic/strchrnul.c \
	$(MPL)/alpha/strchrnul.c \
	$(MPL)/arm/strchrnul.c \
	$(MPL)/mips/strchrnul.c \
	$(MPL)/ia64/strchrnul.c \
	$(MPL)/parisc/strchrnul.c \
	$(MPL)/x86/strchrnul.c \
	$(MPL)/ppc/strchrnul.c
STRRCHRSRC = \
	$(MPL)/generic/strrchr.c \
	$(MPL)/alpha/strrchr.c \
	$(MPL)/arm/strrchr.c \
	$(MPL)/parisc/strrchr.c \
	$(MPL)/ia64/strrchr.c \
	$(MPL)/ppc/strrchr.c \
	$(MPL)/x86/strrchr.c
STRNCASECMP_ASRC = \
	$(MPL)/generic/strncasecmp_a.c \
	$(MPL)/alpha/strncasecmp_a.c \
	$(MPL)/ia64/strncasecmp_a.c \
	$(MPL)/parisc/strncasecmp_a.c \
	$(MPL)/x86/strncasecmp_a.c \
	$(MPL)/ppc/strncasecmp_a.c
STRLPCPYSRC = \
	$(MPL)/generic/strlpcpy.c \
	$(MPL)/x86/strlpcpy.c \
	$(MPL)/ppc/strlpcpy.c
STRREVERSE_LSRC = \
	$(MPL)/generic/strreverse_l.c \
	$(MPL)/ppc/strreverse_l.c \
	$(MPL)/arm/strreverse_l.c \
	$(MPL)/x86/strreverse_l.c
TO_BASE16SRC = \
	$(MPL)/generic/to_base16.c \
	$(MPL)/arm/to_base16.c \
	$(MPL)/alpha/to_base16.c \
	$(MPL)/ia64/to_base16.c \
	$(MPL)/ppc/to_base16.c \
	$(MPL)/x86/to_base16.c
TO_BASE32SRC = \
	$(MPL)/generic/to_base32.c \
	$(MPL)/alpha/to_base32.c \
	$(MPL)/arm/to_base32.c \
	$(MPL)/ia64/to_base32.c \
	$(MPL)/x86/to_base32.c
TSTRLENSRC = \
	$(MPL)/generic/tstrlen.c \
	$(MPL)/alpha/tstrlen.c \
	$(MPL)/arm/tstrlen.c \
	$(MPL)/mips/tstrlen.c \
	$(MPL)/ia64/tstrlen.c \
	$(MPL)/parisc/tstrlen.c \
	$(MPL)/x86/tstrlen.c \
	$(MPL)/ppc/tstrlen.c
TSTRCHRNULSRC = \
	$(MPL)/generic/tstrchrnul.c \
	$(MPL)/alpha/tstrchrnul.c \
	$(MPL)/arm/tstrchrnul.c \
	$(MPL)/mips/tstrchrnul.c \
	$(MPL)/ia64/tstrchrnul.c \
	$(MPL)/parisc/tstrchrnul.c \
	$(MPL)/x86/tstrchrnul.c \
	$(MPL)/ppc/tstrchrnul.c
TSTRNCMPSRC = \
	$(MPL)/generic/tstrncmp.c
ADLER32SRC = \
	$(MPL)/generic/adler32.c \
	$(MPL)/alpha/adler32.c \
	$(MPL)/arm/adler32.c \
	$(MPL)/ia64/adler32.c \
	$(MPL)/mips/adler32.c \
	$(MPL)/sparc/adler32.c \
	$(MPL)/x86/adler32.c \
	$(MPL)/ppc/adler32.c
MY_BITOPSSRC = \
	$(MPL)/generic/my_bitops.c \
	$(MPL)/x86/my_bitops.c

LIBASRCS = \
	$(LIBSRCS) \
	$(UNALIGNEDSRC) \
	$(EPOLLSRS) \
	$(ATOMSRC) \
	$(AESSRC) \
	$(FLSSTSRC) \
	$(POPCOUNTSTSRC) \
	$(CPY_RESTSRC) \
	$(MEMXORCPYSRC) \
	$(MEMANDSRC) \
	$(MEMNEGSRC) \
	$(MEMCPYSRC) \
	$(MEMPCPYSRC) \
	$(MEMCPY_REV_SRC) \
	$(MEMCHRSRC) \
	$(MEMPOPCNTSRC) \
	$(MEM_SEARCHRNSRC) \
	$(MEM_SPN_FF_SRC) \
	$(STRNLENSRC) \
	$(STRCHRNULSRC) \
	$(STRRCHRSRC) \
	$(STRNCASECMP_ASRC) \
	$(STRLENSRC) \
	$(STRLPCPYSRC) \
	$(STRREVERSE_LSRC) \
	$(TO_BASE16SRC) \
	$(TO_BASE32SRC) \
	$(TSTRLENSRC) \
	$(TSTRCHRNULSRC) \
	$(TSTRNCMPSRC) \
	$(ADLER32SRC) \
	$(MY_BITOPSSRC)

# base src files
LIBSRCS = \
	$(MPL)/adler32.c \
	$(MPL)/aes.c \
	$(MPL)/ansi_prng.c \
	$(MPL)/atomic.c \
	$(MPL)/backtrace.c \
	$(MPL)/bitfield_rle.c \
	$(MPL)/cpy_rest.c \
	$(MPL)/config_parser.c \
	$(MPL)/combo_addr.c \
	$(MPL)/entities.c \
	$(MPL)/flsst.c \
	$(MPL)/guid.c \
	$(MPL)/hzp.c \
	$(MPL)/inet_ntop.c \
	$(MPL)/inet_pton.c \
	$(MPL)/introsort.c \
	$(MPL)/log_facility.c \
	$(MPL)/memxorcpy.c \
	$(MPL)/memand.c \
	$(MPL)/memneg.c \
	$(MPL)/mempopcnt.c \
	$(MPL)/memcpy.c \
	$(MPL)/mempcpy.c \
	$(MPL)/memmove.c \
	$(MPL)/memchr.c \
	$(MPL)/mem_searchrn.c \
	$(MPL)/mem_spn_ff.c \
	$(MPL)/my_epoll.c \
	$(MPL)/my_pthread.c \
	$(MPL)/my_bitops.c \
	$(MPL)/popcountst.c \
	$(MPL)/recv_buff.c \
	$(MPL)/rbtree.c \
	$(MPL)/udpfromto.c \
	$(MPL)/print_ts.c \
	$(MPL)/strnlen.c \
	$(MPL)/strlen.c \
	$(MPL)/strchr.c \
	$(MPL)/strrchr.c \
	$(MPL)/strchrnul.c \
	$(MPL)/strncasecmp_a.c \
	$(MPL)/strpcpy.c \
	$(MPL)/strlpcpy.c \
	$(MPL)/strreverse_l.c \
	$(MPL)/to_base16.c \
	$(MPL)/to_base32.c \
	$(MPL)/tstrlen.c \
	$(MPL)/tstrchrnul.c \
	$(MPL)/tstrncmp.c \
	$(MPL)/tchar.c \
	$(MPL)/vsnprintf.c

BITOPOBJS = \
	$(MPL)/adler32.o \
	$(MPL)/bitfield_rle.o \
	$(MPL)/cpy_rest.o \
	$(MPL)/config_parser.o \
	$(MPL)/entities.o \
	$(MPL)/flsst.o \
	$(MPL)/introsort.o \
	$(MPL)/memxorcpy.o \
	$(MPL)/memand.o \
	$(MPL)/memneg.o \
	$(MPL)/mempopcnt.o \
	$(MPL)/memcpy.o \
	$(MPL)/mempcpy.o \
	$(MPL)/memmove.o \
	$(MPL)/memchr.o \
	$(MPL)/mem_searchrn.o \
	$(MPL)/mem_spn_ff.o \
	$(MPL)/popcountst.o \
	$(MPL)/strnlen.o \
	$(MPL)/strlen.o \
	$(MPL)/strchr.o \
	$(MPL)/strrchr.o \
	$(MPL)/strchrnul.o \
	$(MPL)/strncasecmp_a.o \
	$(MPL)/strpcpy.o \
	$(MPL)/strlpcpy.o \
	$(MPL)/strreverse_l.o\
	$(MPL)/to_base16.o \
	$(MPL)/to_base32.o \
	$(MPL)/my_bitops.o

TCHAROBJS = \
	$(MPL)/tstrlen.o \
	$(MPL)/tstrchrnul.o \
	$(MPL)/tstrncmp.o \
	$(MPL)/tchar.o

LIBBINOBJS = \
	$(MPL)/aes_tab.o \
	$(MPL)/five_tab.o \
	$(MPL)/tchar_tab.o

# base objectfiles
LIBOBJS = \
	$(BITOPOBJS) \
	$(TCHAROBJS) \
	$(LIBBINOBJS) \
	$(MPL)/aes.o \
	$(MPL)/atomic.o \
	$(MPL)/ansi_prng.o \
	$(MPL)/backtrace.o \
	$(MPL)/combo_addr.o \
	$(MPL)/guid.o \
	$(MPL)/hzp.o \
	$(MPL)/inet_ntop.o \
	$(MPL)/inet_pton.o \
	$(MPL)/log_facility.o \
	$(MPL)/my_epoll.o \
	$(MPL)/my_pthread.o \
	$(MPL)/recv_buff.o \
	$(MPL)/rbtree.o \
	$(MPL)/udpfromto.o \
	$(MPL)/print_ts.o \
	$(MPL)/vsnprintf.o

# target for this module
LIBCOMMON = $(MPL)/libcommon.a

# first Target to handel sub-build
alll:
	$(MAKE) -C .. $(LIBCOMMON)

# not parralel compatible
#$(LIBCOMMON): $(LIBCOMMON)($(LIBOBJS))
#	$(RANLIB) $@
#$(LIBCOMMON)($(LIBOBJS)): arflock

$(LIBCOMMON): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@
$(LIBCOMMON): arflock

$(MPL)/aes_tab_gen: $(MPL)/aes_tab_gen.c ccdrv $(MPL)/module.make Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) $(MPL)/aes_tab_gen.c -o $@

$(AES_TABS): $(MPL)/aes_tab_gen
	@./ccdrv -s$(VERBOSE) "TAB[$@]" $(MPL)/aes_tab_gen $@

$(MPL)/aes_tab.o: $(AES_TABS) bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -e -a $(AS) $(BIN2O_OPTS) -o $@ $(AES_TABS)

$(MPL)/five_tab_gen: $(MPL)/five_tab_gen.c ccdrv $(MPL)/module.make Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) $(MPL)/five_tab_gen.c -o $@

$(MPL)/five_tab.bin: $(MPL)/five_tab_gen
	@./ccdrv -s$(VERBOSE) "TAB[$@]" $(MPL)/five_tab_gen $@

$(MPL)/five_tab.o: $(MPL)/five_tab.bin bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -e -a $(AS) $(BIN2O_OPTS) -o $@ $(MPL)/five_tab.bin

$(MPL)/tchar_tab.o: $(TCHAR_TABS) bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -e -a $(AS) $(BIN2O_OPTS) -o $@ $(TCHAR_TABS)

$(MPL)/tchar_tolower.bin: $(MPL)/tchar_tolower_be.bin
	@if [ "$(TARGET_ENDIAN)" = "little" ] ; then \
		./ccdrv -s$(VERBOSE) "SWAP[$@]" dd bs=1 if=$(MPL)/tchar_tolower_be.bin conv=swab of=$@ > /dev/null ; \
	else \
		./ccdrv -s$(VERBOSE) "CP[$@]" cp -f $(MPL)/tchar_tolower_be.bin $@ ; \
	fi
$(MPL)/tchar_c1table.bin: $(MPL)/tchar_c1table_be.bin
	@if [ "$(TARGET_ENDIAN)" = "little" ] ; then \
		./ccdrv -s$(VERBOSE) "SWAP[$@]" dd bs=1 if=$(MPL)/tchar_c1table_be.bin conv=swab of=$@ > /dev/null ; \
	else \
		./ccdrv -s$(VERBOSE) "CP[$@]" cp -f $(MPL)/tchar_c1table_be.bin $@ ; \
	fi
$(MPL)/tchar_c3table.bin: $(MPL)/tchar_c3table_be.bin
	@if [ "$(TARGET_ENDIAN)" = "little" ] ; then \
		./ccdrv -s$(VERBOSE) "SWAP[$@]" dd bs=1 if=$(MPL)/tchar_c3table_be.bin conv=swab of=$@ > /dev/null ; \
	else \
		./ccdrv -s$(VERBOSE) "CP[$@]" cp -f $(MPL)/tchar_c3table_be.bin $@ ; \
	fi

$(TCHAR_TABS): $(MPL)/module.make Makefile ./ccdrv

# Dependencies
$(BITOPOBJS): $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(TCHAROBJS): $(MPL)/tchar.h $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(MPL)/flsst.o: $(FLSSTSRC)
$(MPL)/popcountst.o: $(POPCOUNTSTSRC)
$(MPL)/cpy_rest.o: $(CPY_RESTSRC)
$(MPL)/memxorcpy.o: $(MEMXORCPYSRC)
$(MPL)/memand.o: $(MEMANDSRC)
$(MPL)/memneg.o: $(MEMNEGSRC)
$(MPL)/memcpy.o: $(MEMCPYSRC) $(MEMCPY_REV_SRC)
$(MPL)/mempcpy.o: $(MEMPCPYSRC)
$(MPL)/memchr.o: $(MEMCHRSRC)
$(MPL)/mempopcnt.o: $(MEMPOPCNTSRC)
$(MPL)/mem_searchrn.o: $(MEM_SEARCHRNSRC)
$(MPL)/mem_spn_ff.o: $(MEM_SPN_FF_SRC)
$(MPL)/strnlen.o: $(STRNLENSRC)
$(MPL)/strlen.o: $(STRLENSRC)
$(MPL)/strchrnul.o: $(STRCHRNULSRC)
$(MPL)/strrchr.o: $(STRRCHRSRC)
$(MPL)/strncasecmp_a.o: $(STRNCASECMP_ASRC)
$(MPL)/strlpcpy.o: $(STRLPCPYSRC)
$(MPL)/strreverse_l.o: $(STRREVERSE_LSRC)
$(MPL)/to_base16.o: $(TO_BASE16SRC)
$(MPL)/to_base32.o: $(TO_BASE32SRC)
$(MPL)/tstrlen.o: $(TSTRLENSRC)
$(MPL)/tstrchrnul.o: $(TSTRCHRNULSRC)
$(MPL)/tstrncmp.o: $(TSTRNCMPSRC)
$(MPL)/adler32.o: $(ADLER32SRC)
$(MPL)/ansi_prng.o: $(MPL)/ansi_prng.h $(MPL)/aes.h
$(MPL)/aes.o: $(AESSRC) $(MPL)/aes.h
$(MPL)/my_epoll.o: $(MPL)/my_epoll.h $(EPOLLSRS)
$(MPL)/log_facility.o: $(MPL)/log_facility.h $(MPL)/sec_buffer.h $(MPL)/itoa.h $(MPL)/my_bitopsm.h G2MainServer.h
%(MPL)/print_ts.o: $(MPL)/itoa.h
$(MPL)/vsnprintf.o: $(MPL)/log_facility.h $(MPL)/itoa.h
$(MPL)/hzp.o: $(MPL)/hzp.h $(MPL)/atomic.h
$(MPL)/hzp.h: $(MPL)/atomic.h
$(MPL)/guid.o: $(MPL)/guid.h $(MPL)/aes.h $(MPL)/ansi_prng.h $(MPL)/hthash.h
$(MPL)/inet_ntop.o: $(MPL)/combo_addr.h
$(MPL)/inet_pton.o: $(MPL)/combo_addr.h
$(MPL)/combo_addr.o: $(MPL)/combo_addr.h $(MPL)/other.h $(MPL)/hthash.h
$(MPL)/backtrace.o: $(MPL)/backtrace.h $(MPL)/log_facility.h $(MPL)/itoa.h config.h
$(MPL)/config_parser.o: $(MPL)/config_parser.h $(MPL)/log_facility.h G2Connection.h
$(MPL)/atomic.o: $(MPL)/atomic.h $(MPL)/generic/atomic.h $(MPL)/generic/atomic.c
$(MPL)/atomic.h: $(ATOMICSRC)
$(MPL)/my_bitops.h: $(MPL)/other.h
$(MPL)/my_bitops.o: $(MY_BITOPSSRC)
$(MPL)/my_bitopsm.h: $(MPL)/other.h config.h
$(MPL)/my_epoll_devpoll.c: $(MPL)/hzp.h
$(MPL)/my_epoll.h: $(MPL)/other.h config.h
$(MPL)/unaligned.h: $(UNALIGNEDSRC)
$(MPL)/x86/memxorcpy.c $(MPL)/x86/memand.c $(MPL)/x86/memneg.c: $(MPL)/x86/x86.h
$(MPL)/x86/my_bitops.c: $(MPL)/x86/x86_features.h
$(MPL)/x86/memxorcpy.c: $(MPL)/x86/memxorcpy_tmpl.c
$(MPL)/x86/memand.c: $(MPL)/x86/memand_tmpl.c
$(MPL)/x86/memneg.c: $(MPL)/x86/memneg_tmpl.c


# give all files the std deps, so get rebuilt if something changes
$(LIBOBJS): $(LIB_STD_DEPS)

libclean:
	$(RM) $(LIBCOMMON) $(LIBOBJS) $(MPL)/aes_tab_gen $(MPL)/five_tab_gen

libdclean: libclean
	$(RM) $(AES_TABS) $(TCHAR_TABS) $(MPL)/five_tab.bin $(MPL)/*.bb $(MPL)/*.bbg $(MPL)/*.da $(MPL)/*.i $(MPL)/*.s $(MPL)/*.rtl

libdepend: $(LIBSRCS)
	@$(PORT_PR) "\tDEP[$(MPL)/$@]\n"; $(CC) -MM -MG $(CFLAGS) $(CPPFLAGS) $(LIBSRCS) > $(MPL)/$@;
