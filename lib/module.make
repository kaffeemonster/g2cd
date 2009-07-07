# name of module
#	normaly this would be set in the root Makefile and always called
#	MP, but for compatibilitis sake, i can not use the needed :=
MPL = lib

AES_TABS = \
	$(MPL)/aes_ft_tab.bin \
	$(MPL)/aes_fl_tab.bin \
	$(MPL)/aes_it_tab.bin \
	$(MPL)/aes_il_tab.bin

TARED_FILES += \
	$(MPL)/module.make \
	$(MPL)/aes_tab_gen.c \
	$(AES_TABS) \
	$(LIBHEADS) \
	$(LIBASRCS)

TARED_DIRS += \
	$(MPL) \
	$(MPL)/generic \
	$(MPL)/x86 \
	$(MPL)/mips \
	$(MPL)/ppc \
	$(MPL)/sparc \
	$(MPL)/sparc64 \
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
	$(MPL)/log_facility.h \
	$(MPL)/sec_buffer.h \
	$(MPL)/combo_addr.h \
	$(MPL)/recv_buff.h \
	$(MPL)/udpfromto.h \
	$(MPL)/hzp.h \
	$(MPL)/atomic.h \
	$(MPL)/ansi_prng.h \
	$(MPL)/aes.h \
	$(MPL)/itoa.h \
	$(MPL)/rbtree.h \
	$(MPL)/list.h \
	$(MPL)/hlist.h \
	$(MPL)/hthash.h \
	$(MPL)/backtrace.h \
	$(MPL)/x86/x86_features.h \
	$(MPL)/x86/x86.h \
	$(MPL)/ppc/ppc_altivec.h

# epoll emuls
EPOLLSRS = \
	$(MPL)/my_epoll_kepoll.c \
	$(MPL)/my_epoll_devpoll.c \
	$(MPL)/my_epoll_poll.c \
	$(MPL)/my_epoll_kqueue.c

# 
ATOMSRC = \
	$(MPL)/generic/atomic.h \
	$(MPL)/generic/atomic.c \
	$(MPL)/x86/atomic.h \
	$(MPL)/mips/atomic.h \
	$(MPL)/ia64/atomic.h \
	$(MPL)/ppc/atomic.h \
	$(MPL)/sparc/atomic.h \
	$(MPL)/sparc64/atomic.h \
	$(MPL)/alpha/atomic.h \
	$(MPL)/arm/atomic.h

# arch src files
AESSRC = \
	$(MPL)/generic/aes.c \
	$(MPL)/x86/aes.c
FLSSTSRC = \
	$(MPL)/generic/flsst.c \
	$(MPL)/x86/flsst.c \
	$(MPL)/mips/flsst.c \
	$(MPL)/ppc/flsst.c \
	$(MPL)/alpha/flsst.c \
	$(MPL)/arm/flsst.c
POPCOUNTSTSRC = \
	$(MPL)/generic/popcountst.c \
	$(MPL)/x86/popcountst.c \
	$(MPL)/ia64/popcountst.c \
	$(MPL)/sparc64/popcountst.c \
	$(MPL)/ppc/popcountst.c \
	$(MPL)/alpha/popcountst.c
CPY_RESTSRC = \
	$(MPL)/generic/cpy_rest.c \
	$(MPL)/ppc/cpy_rest.c \
	$(MPL)/x86/cpy_rest.c
MEMXORCPYSRC = \
	$(MPL)/generic/memxorcpy.c \
	$(MPL)/x86/memxorcpy.c \
	$(MPL)/x86/memxorcpy_tmpl.c \
	$(MPL)/ppc/memxorcpy.c
MEMANDSRC = \
	$(MPL)/generic/memand.c \
	$(MPL)/x86/memand.c \
	$(MPL)/x86/memand_tmpl.c \
	$(MPL)/ppc/memand.c
MEMNEGSRC = \
	$(MPL)/generic/memneg.c \
	$(MPL)/x86/memneg.c \
	$(MPL)/x86/memneg_tmpl.c \
	$(MPL)/ppc/memneg.c
MEM_SEARCHRNSRC = \
	$(MPL)/generic/mem_searchrn.c \
	$(MPL)/x86/mem_searchrn.c \
	$(MPL)/ppc/mem_searchrn.c
STRNLENSRC = \
	$(MPL)/generic/strnlen.c \
	$(MPL)/x86/strnlen.c \
	$(MPL)/ppc/strnlen.c
STRLENSRC = \
	$(MPL)/generic/strlen.c \
	$(MPL)/x86/strlen.c \
	$(MPL)/ppc/strlen.c
STRCHRNULSRC = \
	$(MPL)/generic/strchrnul.c \
	$(MPL)/x86/strchrnul.c \
	$(MPL)/ppc/strchrnul.c
STRNCASECMP_ASRC = \
	$(MPL)/generic/strncasecmp_a.c \
	$(MPL)/x86/strncasecmp_a.c \
	$(MPL)/ppc/strncasecmp_a.c
STRNPCPYSRC = \
	$(MPL)/generic/strnpcpy.c \
	$(MPL)/x86/strnpcpy.c \
	$(MPL)/ppc/strnpcpy.c
ADLER32SRC = \
	$(MPL)/generic/adler32.c \
	$(MPL)/x86/adler32.c \
	$(MPL)/ppc/adler32.c
MY_BITOPSSRC = \
	$(MPL)/generic/my_bitops.c \
	$(MPL)/x86/my_bitops.c

LIBASRCS = \
	$(LIBSRCS) \
	$(EPOLLSRS) \
	$(ATOMSRC) \
	$(AESSRC) \
	$(FLSSTSRC) \
	$(POPCOUNTSTSRC) \
	$(CPY_RESTSRC) \
	$(MEMXORCPYSRC) \
	$(MEMANDSRC) \
	$(MEMNEGSRC) \
	$(MEM_SEARCHRNSRC) \
	$(STRNLENSRC) \
	$(STRCHRNULSRC) \
	$(STRNCASECMP_ASRC) \
	$(STRLENSRC) \
	$(STRNPCPYSRC) \
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
	$(MPL)/flsst.c \
	$(MPL)/hzp.c \
	$(MPL)/inet_ntop.c \
	$(MPL)/inet_pton.c \
	$(MPL)/introsort.c \
	$(MPL)/log_facility.c \
	$(MPL)/memxorcpy.c \
	$(MPL)/memand.c \
	$(MPL)/memneg.c \
	$(MPL)/memcpy.c \
	$(MPL)/mempcpy.c \
	$(MPL)/mem_searchrn.c \
	$(MPL)/my_epoll.c \
	$(MPL)/my_bitops.c \
	$(MPL)/popcountst.c \
	$(MPL)/recv_buff.c \
	$(MPL)/rbtree.c \
	$(MPL)/udpfromto.c \
	$(MPL)/strnlen.c \
	$(MPL)/strlen.c \
	$(MPL)/strchr.c \
	$(MPL)/strchrnul.c \
	$(MPL)/strncasecmp_a.c \
	$(MPL)/strpcpy.c \
	$(MPL)/strnpcpy.c \
	$(MPL)/vsnprintf.c

BITOPOBJS = \
	$(MPL)/adler32.o \
	$(MPL)/bitfield_rle.o \
	$(MPL)/cpy_rest.o \
	$(MPL)/flsst.o \
	$(MPL)/introsort.o \
	$(MPL)/memxorcpy.o \
	$(MPL)/memand.o \
	$(MPL)/memneg.o \
	$(MPL)/memcpy.o \
	$(MPL)/mempcpy.o \
	$(MPL)/mem_searchrn.o \
	$(MPL)/popcountst.o \
	$(MPL)/strnlen.o \
	$(MPL)/strlen.o \
	$(MPL)/strchr.o \
	$(MPL)/strchrnul.o \
	$(MPL)/strncasecmp_a.o \
	$(MPL)/strpcpy.o \
	$(MPL)/strnpcpy.o \
	$(MPL)/my_bitops.o

LIBBINOBJS = \
	$(MPL)/aes_tab.o \
	$(MPL)/five_tab.o

# base objectfiles
LIBOBJS = \
	$(BITOPOBJS) \
	$(LIBBINOBJS) \
	$(MPL)/aes.o \
	$(MPL)/atomic.o \
	$(MPL)/ansi_prng.o \
	$(MPL)/backtrace.o \
	$(MPL)/hzp.o \
	$(MPL)/inet_ntop.o \
	$(MPL)/inet_pton.o \
	$(MPL)/log_facility.o \
	$(MPL)/my_epoll.o \
	$(MPL)/recv_buff.o \
	$(MPL)/rbtree.o \
	$(MPL)/udpfromto.o \
	$(MPL)/vsnprintf.o

# target for this module
LIBCOMMON = $(MPL)/libcommon.a

# first Target to handel sub-build
alll:
	$(MAKE) -C .. $(LIBCOMMON)

# not parralel compatible
$(LIBCOMMON): $(LIBCOMMON)($(LIBOBJS))
	$(RANLIB) $@
$(LIBCOMMON)($(LIBOBJS)): arflock

$(MPL)/aes_tab_gen: $(MPL)/aes_tab_gen.c ccdrv $(MPL)/module.make Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) $(MPL)/aes_tab_gen.c -o $@

$(AES_TABS): $(MPL)/aes_tab_gen
	@./ccdrv -s$(VERBOSE) "TAB[$@]" $(MPL)/aes_tab_gen $@

$(MPL)/aes_tab.o: $(AES_TABS) bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -e -a $(AS) -o $@ $(AES_TABS)

$(MPL)/five_tab_gen: $(MPL)/five_tab_gen.c ccdrv $(MPL)/module.make Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) $(MPL)/five_tab_gen.c -o $@

$(MPL)/five_tab.bin: $(MPL)/five_tab_gen
	@./ccdrv -s$(VERBOSE) "TAB[$@]" $(MPL)/five_tab_gen $@

$(MPL)/five_tab.o: $(MPL)/five_tab.bin bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -e -a $(AS) -o $@ $(MPL)/five_tab.bin

# Dependencies
$(BITOPOBJS): $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(MPL)/flsst.o: $(FLSSTSRC)
$(MPL)/popcountst.o: $(POPCOUNTSTSRC)
$(MPL)/cpy_rest.o: $(CPY_RESTSRC)
$(MPL)/memxorcpy.o: $(MEMXORCPYSRC)
$(MPL)/memand.o: $(MEMANDSRC)
$(MPL)/memneg.o: $(MEMNEGSRC)
$(MPL)/mem_searchrn.o: $(MEM_SEARCHRNSRC)
$(MPL)/strnlen.o: $(STRNLENSRC)
$(MPL)/strlen.o: $(STRLENSRC)
$(MPL)/strchrnul.o: $(STRCHRNULSRC)
$(MPL)/strncasecmp_a.o: $(STRNCASECMP_ASRC)
$(MPL)/strnpcpy.o: $(STRNPCPYSRC)
$(MPL)/adler32.o: $(ADLER32SRC)
$(MPL)/ansi_prng.o: $(MPL)/ansi_prng.h $(MPL)/aes.h
$(MPL)/aes.o: $(AESSRC) $(MPL)/aes.h
$(MPL)/my_epoll.o: $(MPL)/my_epoll.h $(EPOLLSRS)
$(MPL)/log_facility.o: $(MPL)/log_facility.h $(MPL)/sec_buffer.h $(MPL)/itoa.h G2MainServer.h
$(MPL)/vsnprintf.o: $(MPL)/log_facility.h $(MPL)/itoa.h
$(MPL)/hzp.o: $(MPL)/hzp.h $(MPL)/atomic.h
$(MPL)/hzp.h: $(MPL)/atomic.h
$(MPL)/inet_ntop.o: $(MPL)/combo_addr.h $(MPL)/itoa.h
$(MPL)/inet_pton.o: $(MPL)/combo_addr.h
$(MPL)/backtrace.o: $(MPL)/backtrace.h $(MPL)/log_facility.h $(MPL)/itoa.h config.h
$(MPL)/atomic.o: $(MPL)/atomic.h $(MPL)/generic/atomic.h $(MPL)/generic/atomic.c
$(MPL)/atomic.h: $(ATOMICSRC)
$(MPL)/my_bitops.h: $(MPL)/other.h
$(MPL)/my_bitops.o: $(MY_BITOPSSRC)
$(MPL)/my_bitopsm.h: $(MPL)/other.h config.h
$(MPL)/my_epoll_devpoll.c: $(MPL)/hzp.h
$(MPL)/my_epoll.h: $(MPL)/other.h config.h
$(MPL)/combo_addr.h: $(MPL)/other.h $(MPL)/hthash.h
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
	$(RM) $(AES_TABS) $(MPL)/*.bb $(MPL)/*.bbg $(MPL)/*.da $(MPL)/*.i $(MPL)/*.s $(MPL)/*.bin $(MPL)/*.rtl

libdepend: $(LIBSRCS)
	@$(PORT_PR) "\tDEP[$(MPL)/$@]\n"; $(CC) -MM -MG $(CFLAGS) $(CPPFLAGS) $(LIBSRCS) > $(MPL)/$@;
