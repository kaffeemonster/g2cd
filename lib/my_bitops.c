/*
 * my_bitops.c
 * some nity grity bitops
 *
 * Copyright (c) 2008-2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif
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
 * detect the type: also in a register only readable in priviliged mode)
 * One has to ask the kernel.
 * On Solaris there is sys/processor.h and processor_info(id, struct *).
 * On Linux one has to parse /proc/cpuinfo?
 * But this is a little bit mute on sparc, one prop. has either the
 * orginal OS which does not support newer instructions, so compile
 * would fail even if the instructions are not used, on Linux the user
 * of such archs hopefully compile more stuff themself (tell me a big
 * alpha or sparc distro...) with the aprop. -march=<your cpu>.
 * Ah, yes i forgot, -march on such archs sometimes does not set enough
 * preprocessor magic...
 */
#  include "generic/my_bitops.c"
# endif
#else
# include "generic/my_bitops.c"
#endif

/*
 * General code to get the number of "engines".
 *
 * This is all a major PITA, esp. since multithreading
 * is still seen as "extra" and the info "how many cpus
 * do i have" is not standard, at least not long enough
 * to be really sunken in.
 * We could touch the cpus themself, but then we would prop.
 * get to the next point of pain: Say the OS to shedule
 * us once on every CPU...
 * One "winner" is the sysconf thingy, but still.
 *
 * All in all this is a bunch of C&P code from various
 * places, little details of: "this plattform has this
 * extra notch", and folklore.
 *
 * We take all of it, also the win32 version, to have
 * it documented
 */
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
# include <sys/pstat.h>
#else
# include <unistd.h>
#endif

#ifndef _SC_NPROCESSORS_ONLN
# ifdef _SC_NPROC_ONLN
#   define _SC_NPROCESSORS_ONLN _SC_NPROC_ONLN
# elif defined _SC_NPROCESSORS_CONF
#   define _SC_NPROCESSORS_ONLN _SC_NPROCESSORS_CONF
# elif defined _SC_CRAY_NCPU
#   define _SC_NPROCESSORS_ONLN _SC_CRAY_NCPU
# elif defined HAVE_SYS_SYSCTL_H
#  include <sys/types.h>
#  include <sys/param.h>
#  include <sys/sysctl.h>
# endif
#endif

unsigned get_cpus_online(void)
{
#ifdef _WIN32
	SYSTEM_INFO info;

	GetSystemInfo(&info);
	if((int)info.dwNumberOfProcessors > 0)
		return (unsigned)info.dwNumberOfProcessors;
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
	struct pst_dynamic psd;

	if(pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) == 1)
		return (unsigned)psd.psd_proc_cnt;
#endif
#ifdef _SC_NPROCESSORS_ONLN
	{
		long ncpus;
		if((ncpus = (long)sysconf(_SC_NPROCESSORS_ONLN)) > 0)
			return (unsigned)ncpus;
	}
#elif defined HAVE_SYS_SYSCTL_H
	{
		int ncpus;
		size_t len = sizeof(ncpus);
# ifdef HAVE_SYSCTLBYNAME
		/* try the online-cpu-thingy first (only osx?) */
		if(!sysctlbyname("hw.activecpu", &ncpus, &len , NULL, 0))
			return (unsigned)ncpu;
		if(!sysctlbyname("hw.ncpu", &ncpus, &len , NULL, 0))
			return (unsigned)ncpu;
# else
		int mib[2] = {CTL_HW, HW_NCPU};
		if(!sysctl(mib, 2, &ncpus, &len, NULL, 0))
			return (unsigned)ncpus
# endif
	}
#endif
	/*
	 * if old solaris does not provide NPROCESSORS,
	 * one maybe bang on processor_info() and friends
	 * from <sys/processor.h>, see above
	 */
// TODO: warn user?
	/* we have at least one cpu, we are running on it... */
	return 1;
}

#ifndef HAVE_EMIT_EMMS
void emit_emms(void)
{
}
#endif

static char const rcsid_mbo[] GCC_ATTR_USED_VAR = "$Id:$";
