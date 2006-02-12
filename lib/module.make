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
	$(MPL)/atomic.h

# epoll emuls
EPOLLSRS = \
	$(MPL)/my_epoll_kepoll.c \
	$(MPL)/my_epoll_devpoll.c \
	$(MPL)/my_epoll_poll.c \
	$(MPL)/my_epoll_kqueue.c

# 
ATOMSRC = \
	$(MPL)/generic/atomic.h \
	$(MPL)/i386/atomic.h \
	$(MPL)/sparc/atomic.h \
	$(MPL)/sparc64/atomic.h

# arch src files
FLSSTSRC = \
	$(MPL)/generic/flsst.c \
	$(MPL)/i386/flsst.c \
	$(MPL)/x86_64/flsst.c \
	$(MPL)/ppc/flsst.c \
	$(MPL)/ppc64/flsst.c \
	$(MPL)/alpha/flsst.c
POPCOUNSTSRC = \
	$(MPL)/generic/popcountst.c \
	$(MPL)/ia64/popcountst.c \
	$(MPL)/sparc/popcountst.c \
	$(MPL)/ppc64/popcountst.c \
	$(MPL)/alpha/popcountst.c
MEMXORSRC = \
	$(MPL)/generic/memxor.c \
	$(MPL)/i386/memxor.c \
	$(MPL)/x86_64/memxor.c \
	$(MPL)/ppc/memxor.c \
	$(MPL)/ppc64/memxor.c

LIBASRCS = \
	$(LIBSRCS) \
	$(EPOLLSRS) \
	$(ATOMSRC) \
	$(FLSSTSRC) \
	$(POPCOUNSTSRC) \
	$(MEMXORSRC)

# base src files
LIBSRCS = \
	$(MPL)/flsst.c \
	$(MPL)/popcountst.c \
	$(MPL)/memxor.c \
	$(MPL)/my_epoll.c \
	$(MPL)/log_facility.c \
	$(MPL)/strnlen.c \
	$(MPL)/hzp.c

# base objectfiles
LIBOBJS = \
	$(MPL)/flsst.o \
	$(MPL)/popcountst.o \
	$(MPL)/memxor.o \
	$(MPL)/my_epoll.o \
	$(MPL)/log_facility.o \
	$(MPL)/hzp.o

# with autoconf it will be added dynamically
LIBOBJS += $(MPL)/strnlen.o

# target for this module
LIBCOMMON = $(MPL)/libcommon.a

# first Target to handel sub-build
alll:
	$(MAKE) -C .. $(LIBCOMMON)

#$(LIBCOMMON): $(LIBOBJS)
#	$(AR) $(ARFLAGS) $@ $%
$(LIBCOMMON): $(LIBCOMMON)($(LIBOBJS))
	$(RANLIB) $@
$(LIBCOMMON)($(LIBOBJS)):

# Dependencies
$(MPL)/flsst.o $(MPL)/popcountst.o $(MPL)/memxor.o $(MPL)/strnlen.o: $(MPL)/my_bitops.h $(MPL)/my_bitopsm.h
$(MPL)/flsst.o: $(FLSSTSRC)
$(MPL)/popcountst.o: $(POPCOUNTSTSRC)
$(MPL)/memxor.o: $(MEMXORSRC)
$(MPL)/log_facility.o: $(MPL)/log_facility.h G2MainServer.h
%(MPL)/hzp.o: $(MPL)/hzp.h $(MPL)/atomic.h
$(MPL)/my_bitops.h: other.h
$(MPL)/my_bitopsm.h: other.h config.h
$(MPL)/my_epoll.o: $(MPL)/my_epoll.h
$(MPL)/my_epoll.h: other.h config.h

# give all files the std deps, so get rebuilt if something changes
$(LIBOBJS): $(LIB_STD_DEPS)

libclean:
	$(RM) $(LIBCOMMON) $(LIBOBJS)

libdclean: libclean
	$(RM) $(MPL)/*.bb $(MPL)/*.bbg $(MPL)/*.da $(MPL)/*.i $(MPL)/*.s $(MPL)/*.bin $(MPL)/*.rtl
