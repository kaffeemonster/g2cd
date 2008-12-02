# name of module
#	normaly this would be set in the root Makefile and always called
#	MP, but for compatibilitis sake, i can not use the needed :=
MPL = lib
TARED_FILES += \
	$(MPL)/module.make \
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
	$(MPL)/hzp.h \
	$(MPL)/atomic.h \
	$(MPL)/list.h \
	$(MPL)/hlist.h \
	$(MPL)/hthash.h \
	$(MPL)/backtrace.h \
	$(MPL)/x86/x86_features.h \
	$(MPL)/x86/x86.h

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
FLSSTSRC = \
	$(MPL)/generic/flsst.c \
	$(MPL)/x86/flsst.c \
	$(MPL)/mips/flsst.c \
	$(MPL)/ppc/flsst.c \
	$(MPL)/alpha/flsst.c \
	$(MPL)/arm/flsst.c
POPCOUNSTSRC = \
	$(MPL)/generic/popcountst.c \
	$(MPL)/x86/popcountst.c \
	$(MPL)/ia64/popcountst.c \
	$(MPL)/sparc64/popcountst.c \
	$(MPL)/ppc/popcountst.c \
	$(MPL)/alpha/popcountst.c
MEMXORSRC = \
	$(MPL)/generic/memxor.c \
	$(MPL)/x86/memxor.c \
	$(MPL)/x86/memxor_tmpl.c \
	$(MPL)/ppc/memxor.c
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
STRNLENSRC = \
	$(MPL)/generic/strnlen.c \
	$(MPL)/x86/strnlen.c
MY_BITOPSSRC = \
	$(MPL)/generic/my_bitops.c \
	$(MPL)/x86/my_bitops.c

LIBASRCS = \
	$(LIBSRCS) \
	$(EPOLLSRS) \
	$(ATOMSRC) \
	$(FLSSTSRC) \
	$(POPCOUNSTSRC) \
	$(MEMXORSRC) \
	$(MEMANDSRC) \
	$(MEMNEGSRC) \
	$(STRNLENSRC) \
	$(MY_BITOPSSRC)

# base src files
LIBSRCS = \
	$(MPL)/flsst.c \
	$(MPL)/popcountst.c \
	$(MPL)/memxor.c \
	$(MPL)/memand.c \
	$(MPL)/memneg.c \
	$(MPL)/my_epoll.c \
	$(MPL)/log_facility.c \
	$(MPL)/strnlen.c \
	$(MPL)/mempcpy.c \
	$(MPL)/strpcpy.c \
	$(MPL)/strnpcpy.c \
	$(MPL)/adler32.c \
	$(MPL)/recv_buff.c \
	$(MPL)/hzp.c \
	$(MPL)/backtrace.c \
	$(MPL)/atomic.c

BITOPOBJS = \
	$(MPL)/flsst.o \
	$(MPL)/popcountst.o \
	$(MPL)/memxor.o \
	$(MPL)/memand.o \
	$(MPL)/memneg.o \
	$(MPL)/strnlen.o \
	$(MPL)/mempcpy.o \
	$(MPL)/strpcpy.o \
	$(MPL)/strnpcpy.o \
	$(MPL)/adler32.o \
	$(MPL)/my_bitops.o

# base objectfiles
LIBOBJS = \
	$(BITOPOBJS) \
	$(MPL)/my_epoll.o \
	$(MPL)/log_facility.o \
	$(MPL)/recv_buff.o \
	$(MPL)/hzp.o \
	$(MPL)/backtrace.o \
	$(MPL)/atomic.o

# target for this module
LIBCOMMON = $(MPL)/libcommon.a

# first Target to handel sub-build
alll:
	$(MAKE) -C .. $(LIBCOMMON)

# not parralel compatible
$(LIBCOMMON): $(LIBCOMMON)($(LIBOBJS))
	$(RANLIB) $@
$(LIBCOMMON)($(LIBOBJS)): arflock

# Dependencies
$(BITOPOBJS): $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(MPL)/flsst.o: $(FLSSTSRC)
$(MPL)/popcountst.o: $(POPCOUNTSTSRC)
$(MPL)/memxor.o: $(MEMXORSRC)
$(MPL)/memand.o: $(MEMANDSRC)
$(MPL)/memneg.o: $(MEMNEGSRC)
$(MPL)/my_epoll.o: $(MPL)/my_epoll.h $(EPOLLSRS)
$(MPL)/log_facility.o: $(MPL)/log_facility.h $(MPL)/sec_buffer.h $(MPL)/itoa.h G2MainServer.h
$(MPL)/hzp.o: $(MPL)/hzp.h $(MPL)/atomic.h
$(MPL)/hzp.h: $(MPL)/atomic.h
$(MPL)/backtrace.o: $(MPL)/backtrace.h $(MPL)/log_facility.h $(MPL)/itoa.h config.h
$(MPL)/atomic.o: $(MPL)/atomic.h $(MPL)/generic/atomic.h $(MPL)/generic/atomic.c
$(MPL)/atomic.h: $(ATOMICSRC)
$(MPL)/my_bitops.h: $(MPL)/other.h
$(MPL)/my_bitops.o: $(MY_BITOPSSRC)
$(MPL)/my_bitopsm.h: $(MPL)/other.h config.h
$(MPL)/my_epoll_devpoll.c: $(MPL)/hzp.h
$(MPL)/my_epoll.h: $(MPL)/other.h config.h
$(MPL)/combo_addr.h: $(MPL)/other.h $(MPL)/hthash.h
$(MPL)/x86/memxor.c $(MPL)/x86/memand.c $(MPL)/x86/memneg.c: $(MPL)/x86/x86.h
$(MPL)/x86/my_bitops.c: $(MPL)/x86/x86_features.h
$(MPL)/x86/memxor.c: $(MPL)/x86/memxor_tmpl.c
$(MPL)/x86/memand.c: $(MPL)/x86/memand_tmpl.c
$(MPL)/x86/memneg.c: $(MPL)/x86/memneg_tmpl.c


# give all files the std deps, so get rebuilt if something changes
$(LIBOBJS): $(LIB_STD_DEPS)

libclean:
	$(RM) $(LIBCOMMON) $(LIBOBJS)

libdclean: libclean
	$(RM) $(MPL)/*.bb $(MPL)/*.bbg $(MPL)/*.da $(MPL)/*.i $(MPL)/*.s $(MPL)/*.bin $(MPL)/*.rtl
