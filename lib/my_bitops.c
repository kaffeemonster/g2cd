/*
 * my_bitops.c
 * some nity grity bitops
 *
 * Copyright (c) 2008 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id:$
 */

#include "my_bitops.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
#  include "x86/my_bitops.c"
# else
/*
 * We could do the same detect processor thing for ex. sparc: the
 * Processor State Register (psr) contains some hardwired bits
 * called implementation and version.
 * Unfortunatly reading the psr is a priviliged intruction.
 * IDIOTS!!!
 * (If my memory serves me right the same problem applies to ppc, to
 * detect the type: also in a register only reading in priviliged mode)
 * One has to ask the kernel.
 * On Solaris there is sys/processor.h and processor_info(id, struct *).
 * On Linux one has to parse /proc/cpuinfo?
 * But this is a little bit mute on sparc, one prop. has either the
 * orginal OS which does not support newer instructions, so compile
 * would fail even if the instructions are not used, on Linux the user
 * of such archs hopefully compile more stuff themself (tell me a big
 * alpha or sparc distro...) with the aprop. -march=<your cpu>.
 * Ah, yes i forgot, -march an such archs sometomes does not set enough
 * preprocessor magic...
 */
#  include "generic/my_bitops.c"
# endif
#else
# include "generic/my_bitops.c"
#endif

