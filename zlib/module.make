# name of module
#	normaly this would be set in the root Makefile and always called
#	MP, but for compatibilitis sake, i can not use the needed :=
MPZ = zlib
TARED_FILES += \
	$(MPZ)/module.make \
	$(MPZ)/README.MODULE

TARED_DIRS += \
	$(MPZ)

# target for this module
ZLIB = $(MPZ)/libzlib.a
# dependent header for all files
Z_STD_DEPS_HEADS = \
	$(MPZ)/zlib.h \
	$(MPZ)/zconf.h

# dependencies for all files
Z_STD_DEPS = \
	$(Z_STD_DEPS_HEAD) \
	$(MPZ)/module.make \
	Makefile \
	ccdrv

# all header
ZHEADS = \
	$(MPZ)/crc32.h \
	$(MPZ)/inffast.h \
	$(MPZ)/inflate.h \
	$(MPZ)/trees.h \
	$(MPZ)/zconf.in.h \
	$(MPZ)/zutil.h \
	$(MPZ)/deflate.h \
	$(MPZ)/inffixed.h \
	$(MPZ)/inftrees.h  

# all src files
ZSRCS = \
	$(MPZ)/adler32.c \
	$(MPZ)/compress.c \
	$(MPZ)/crc32.c \
	$(MPZ)/uncompr.c \
	$(MPZ)/deflate.c \
	$(MPZ)/trees.c \
	$(MPZ)/zutil.c \
	$(MPZ)/inflate.c \
	$(MPZ)/infback.c \
	$(MPZ)/inftrees.c \
	$(MPZ)/inffast.c
#	$(MPZ)/gzio.c

# all objectfiles
ZOBJS = \
	$(MPZ)/inflate.o \
	$(MPZ)/inffast.o \
	$(MPZ)/deflate.o \
	$(MPZ)/compress.o \
	$(MPZ)/crc32.o \
	$(MPZ)/adler32.o \
	$(MPZ)/inftrees.o \
	$(MPZ)/trees.o \
	$(MPZ)/zutil.o

#	$(MPZ)/uncompr.o \
#	$(MPZ)/infback.o \
#	$(MPZ)/gzio.o

ZSRCS_586 = $(MPZ)/contrib/asm586/match.S
ZSRCS_686 = $(MPZ)/contrib/asm686/match.S
ZSRCS_X86_1 = $(MPZ)/contrib/inflate86/inffas86.c
ZSRCS_X86_2 = $(MPZ)/contrib/inflate86/inffast.S

# allow sub-build
allz:
	$(MAKE) -C .. $(ZLIB)

#$(LIBCOMMON): $(LIBOBJS)
#	$(AR) $(ARFLAGS) $@ $%
$(ZLIB): $(ZLIB)($(ZOBJS))
	$(RANLIB) $@
$(ZLIB)($(ZOBJS)):

# Dependencies
$(MPZ)/adler32.o:
$(MPZ)/compress.o:
$(MPZ)/crc32.o: $(MPZ)/crc32.h 
$(MPZ)/deflate.o: $(MPZ)/zutil.h
$(MPZ)/example.o:
$(MPZ)/inffast.o: $(MPZ)/zutil.h $(MPZ)/inftrees.h $(MPZ)/inflate.h $(MPZ)/inffast.h
$(MPZ)/inflate.o: $(MPZ)/zutil.h $(MPZ)/inftrees.h $(MPZ)/inflate.h $(MPZ)/inffast.h
$(MPZ)/infback.o: $(MPZ)/zutil.h $(MPZ)/inftrees.h $(MPZ)/inflate.h $(MPZ)/inffast.h
$(MPZ)/inftrees.o: $(MPZ)/zutil.h $(MPZ)/inftrees.h
$(MPZ)/minigzip.o:
$(MPZ)/trees.o: $(MPZ)/deflate.h $(MPZ)/zutil.h $(MPZ)/trees.h
$(MPZ)/uncompr.o:
$(MPZ)/zutil.o: $(MPZ)/zutil.h
#$(MPZ)/gzio.o: $(MPZ)/zutil.h

# give all files the std deps, so get rebuilt if something changes
$(ZOBJS): $(Z_STD_DEPS)

zlibclean:
	$(RM) $(ZLIB) $(ZOBJS)
zlibdclean: zlibclean
	$(RM) $(MPZ)/*.s $(MPZ)/*.i $(MPZ)/*.bb $(MPZ)/*.bbg $(MPZ)/*.da $(MPZ)/*.rtl
