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
	$(MPL)/i386 \
	$(MPL)/x86_64 \
	$(MPL)/mips \
	$(MPL)/ppc \
	$(MPL)/ppc64 \
	$(MPL)/sparc \
	$(MPL)/sparc64 \
	$(MPL)/ia64 \
	$(MPL)/alpha

# dependencies for all files
LIB_STD_DEPS = \
	$(MPL)/module.make \
	Makefile \
	ccdrv

# all header
LIBHEADS = \
	$(MPL)/my_bitops.h \
	$(MPL)/my_bitopsm.h \
	$(MPL)/my_epoll.h \
	$(MPL)/log_facility.h \
	$(MPL)/sec_buffer.h \
	$(MPL)/hzp.h \
	$(MPL)/atomic.h \
	$(MPL)/i386/x86.h

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
	$(MPL)/i386/atomic.h \
	$(MPL)/x86_64/atomic.h \
	$(MPL)/mips/atomic.h \
	$(MPL)/ia64/atomic.h \
	$(MPL)/ppc/atomic.h \
	$(MPL)/sparc/atomic.h \
	$(MPL)/sparc64/atomic.h \
	$(MPL)/alpha/atomic.h

# arch src files
FLSSTSRC = \
	$(MPL)/generic/flsst.c \
	$(MPL)/i386/flsst.c \
	$(MPL)/x86_64/flsst.c \
	$(MPL)/mips/flsst.c \
	$(MPL)/ppc/flsst.c \
	$(MPL)/ppc64/flsst.c \
	$(MPL)/alpha/flsst.c
POPCOUNSTSRC = \
	$(MPL)/generic/popcountst.c \
	$(MPL)/ia64/popcountst.c \
	$(MPL)/sparc64/popcountst.c \
	$(MPL)/ppc64/popcountst.c \
	$(MPL)/alpha/popcountst.c
MEMXORSRC = \
	$(MPL)/generic/memxor.c \
	$(MPL)/i386/memxor.c \
	$(MPL)/x86_64/memxor.c \
	$(MPL)/ppc/memxor.c \
	$(MPL)/ppc64/memxor.c
MEMANDSRC = \
	$(MPL)/generic/memand.c \
	$(MPL)/i386/memand.c \
	$(MPL)/x86_64/memand.c \
	$(MPL)/ppc/memand.c \
	$(MPL)/ppc64/memand.c
MEMNEGSRC = \
	$(MPL)/generic/memneg.c \
	$(MPL)/i386/memneg.c \
	$(MPL)/x86_64/memneg.c \
	$(MPL)/ppc/memneg.c \
	$(MPL)/ppc64/memneg.c

LIBASRCS = \
	$(LIBSRCS) \
	$(EPOLLSRS) \
	$(ATOMSRC) \
	$(FLSSTSRC) \
	$(POPCOUNSTSRC) \
	$(MEMXORSRC) \
	$(MEMANDSRC) \
	$(MEMNEGSRC)

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
	$(MPL)/hzp.c \
	$(MPL)/atomic.c

# base objectfiles
LIBOBJS = \
	$(MPL)/flsst.o \
	$(MPL)/popcountst.o \
	$(MPL)/memxor.o \
	$(MPL)/memand.o \
	$(MPL)/memneg.o \
	$(MPL)/my_epoll.o \
	$(MPL)/log_facility.o \
	$(MPL)/hzp.o \
	$(MPL)/atomic.o

# with autoconf it will be added dynamically
LIBOBJS += $(MPL)/strnlen.o

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
$(MPL)/flsst.o $(MPL)/popcountst.o $(MPL)/memxor.o $(MPL)/memand.o $(MPL)/memneg.o $(MPL)/strnlen.o: $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(MPL)/flsst.o: $(FLSSTSRC)
$(MPL)/popcountst.o: $(POPCOUNTSTSRC)
$(MPL)/memxor.o: $(MEMXORSRC)
$(MPL)/memand.o: $(MEMANDSRC)
$(MPL)/memneg.o: $(MEMNEGSRC)
$(MPL)/my_epoll.o: $(MPL)/my_epoll.h $(EPOLLSRS)
$(MPL)/log_facility.o: $(MPL)/log_facility.h $(MPL)/sec_buffer.h G2MainServer.h
$(MPL)/hzp.o: $(MPL)/hzp.h $(MPL)/atomic.h
$(MPL)/hzp.h: $(MPL)/atomic.h
$(MPL)/atomic.o: $(MPL)/atomic.h $(MPL)/generic/atomic.h $(MPL)/generic/atomic.c
$(MPL)/my_bitops.h: other.h
$(MPL)/my_bitopsm.h: other.h config.h
$(MPL)/my_epoll_devpoll.c: $(MPL)/hzp.h
$(MPL)/my_epoll.h: other.h config.h
$(MPL)/i386/memxor.c $(MPL)/x86_64/memxor.c $(MPL)/i386/memand.c $(MPL)/x86_64/memand.c $(MPL)/i386/memneg.c $(MPL)/x86_64/memneg.c: $(MPL)/i386/x86.h

# give all files the std deps, so get rebuilt if something changes
$(LIBOBJS): $(LIB_STD_DEPS)

libclean:
	$(RM) $(LIBCOMMON) $(LIBOBJS)

libdclean: libclean
	$(RM) $(MPL)/*.bb $(MPL)/*.bbg $(MPL)/*.da $(MPL)/*.i $(MPL)/*.s $(MPL)/*.bin $(MPL)/*.rtl
