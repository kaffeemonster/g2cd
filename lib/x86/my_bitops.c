/*
 * my_bitops.c
 * some nity grity bitops, x86 implementation
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../other.h"
#include "../log_facility.h"
#include "../my_bitops.h"
#include "x86_features.h"
#include "x86.h"

/*
 * Types
 */
enum cpu_vendor
{
	X86_VENDOR_OTHER,
	X86_VENDOR_INTEL,
	X86_VENDOR_AMD,
	X86_VENDOR_CENTAUR, /* today better known as VIA (C3, C7) */
	X86_VENDOR_TRANSMETA,
	X86_VENDOR_SIS,
	X86_VENDOR_NSC,
	X86_VENDOR_CYRIX,
	X86_VENDOR_NEXGEN,
	X86_VENDOR_UMC,
};

struct cpuinfo
{
	enum cpu_vendor vendor;
	union v_str
	{
		char s[16];
		uint32_t r[3];
	} vendor_str;
	union m_str
	{
		char s[64];
		uint32_t r[12];
	} model_str;
	uint32_t family;
	uint32_t model;
	uint32_t stepping;
	uint32_t max_basic;
	uint32_t max_ext;
	int count;
	int num_cores;
	uint32_t features[7];
	unsigned short clflush_size;
	bool init_done;
};

union cpuid_regs
{
	/*
	 * while the info is always 32 bit (uint32_t), the cpu sets
	 * the whole reg, which means it clears the upper 32 bit...
	 */
	struct {
		size_t eax, ebx, ecx, edx;
	} r;
	size_t a[4];
};

/*
 * Vars
 */
static struct cpuinfo our_cpu;
#define ENUM_CMD(x,y) [y] = str_it(x)
const char x86_cpu_feature_names[][16] =
{
	X86_CPU_FEATURE_ENUM
};
#undef ENUM_CMD

/*
 * Defines
 */
#define CPUID_STEPPING(x)	((x) & 0x0F)
#define CPUID_MODEL(x)	(((x) >> 4) & 0x0F)
#define CPUID_FAMILY(x)	(((x) >> 8) & 0x0F)
#define CPUID_XMODEL(x)	(((x) >> 16) & 0x0F)
#define CPUID_XFAMILY(x)	(((x) >> 20) & 0xFF)
#define CPUID_CLFLUSHS(x)	(((x) >> 8) & 0xFF)

/*
 * Prototypes
 */
static void identify_vendor(struct cpuinfo *);
static void identify_cpu(void) GCC_ATTR_CONSTRUCT;

/*
 * grity asm helper
 */
static always_inline size_t read_flags(void)
{
	size_t f;
	asm volatile (
		"pushf\n\t"
		"pop %0\n\t"
		: "=r" (f)
	);
	return f;
}

static always_inline void write_flags(size_t f)
{
	asm volatile (
		"push %0\n\t"
		"popf\n\t"
		: : "ri" (f) : "cc"
	);
}

static always_inline void cpuid(union cpuid_regs *regs, size_t func)
{
	/* save ebx around cpuid call, PIC code needs it */
	asm volatile (
		"xchg	%1, " PICREG "\n\t"
		"cpuid\n\t"
		"xchg	%1, " PICREG "\n"
		: /* %0 */ "=a" (regs->r.eax),
		         /*
		          * let the compiler choose, unfort. sun studio is to stupid
		          * (puts two oprands into the same register, WTF?)
		          * but also fucks up the whole output/inlining thing
		          * -> sorry fanboys, broken Compiler...
		          */
		  /* %1 */ "=r" (regs->r.ebx),
		  /* %2 */ "=c" (regs->r.ecx),
		  /* %4 */ "=d" (regs->r.edx)
		: /* %5 */ "0" (func),
		  /* %6 */ "2" (regs->r.ecx)
		: "cc"
	);
}

static always_inline void cpuids(union cpuid_regs *regs, size_t func)
{
	regs->r.ecx = 0;
	cpuid(regs, func);
}


/*
 * funcs
 */
static inline bool toggle_eflags_test(const size_t mask)
{
	size_t f;

	/*
	 * This is a single toggle test, inspired by wine.
	 * A more pushf/popf generating "on-check-off-check"
	 * test is in the vcs, if one toggle is to "short".
	 */
	f = read_flags();
	write_flags(f ^ mask);
	if(unlikely(!((f ^ read_flags()) & mask)))
		return false;

	return true;
}

static inline bool has_cpuid(void)
{
	return toggle_eflags_test(1 << 21);
}

static inline bool is_486(void)
{
	return toggle_eflags_test(1 << 18);
}

static inline void cpu_feature_clear(int f)
{
	our_cpu.features[f / 32] &= ~(1 << (f % 32));
}

static inline bool cpu_feature(int f)
{
	return !!(our_cpu.features[f / 32] & (1 << (f % 32)));
}

static __init void identify_cpu(void)
{
	union cpuid_regs a;
	size_t i;

	if(our_cpu.init_done)
		return;
	/*
	 * imidiatly prevent other calls to go trough this function.
	 *
	 * This function is not thread save, but that is not our problem.
	 * This function may use lib funcs because the compiler thought
	 * it would be clever to use a memcpy or such thing somewhere.
	 *
	 * Problem: These functions may want to detect the cpu...
	 * To prevent infinite recursion, just kick them out.
	 * They should fall back to the generic impl. when confronted
	 * with a not yet fully populated our_cpu and later switch to
	 * the right one.
	 */
	our_cpu.init_done = true;
	/* prevent the compiler from moving this around */
	barrier();

	/* set the cpu count to a default value, we must have at least one ;) */
	our_cpu.count = 1;

	i = read_flags();
	/* do we have cpuid? we don't want to SIGILL */
	if(unlikely(!has_cpuid()))
	{
		/*
		 * No? *cough* Ok, maybe rare/exotic chip like Geode
		 * which can switch off cpuid in firmware...
		 */
		 /* distinguish 386 from 486, same trick for EFLAGS bit 18 */
		bool t = is_486();
		write_flags(i);
		barrier();
		if(t) {
			strlitcpy(our_cpu.vendor_str.s, "486?");
			our_cpu.family = 4;
		} else {
			strlitcpy(our_cpu.vendor_str.s, "386??");
			our_cpu.family = 3;
		}
#ifdef __linux__
		/* we may run before main, other thread impl. do not like an print so early */
		logg_pos(LOGF_DEBUG, "Looks like this is an CPU older Pentium I???\n");
#endif
		return;
	}
	write_flags(i);
	barrier();

	/* get vendor string */
	cpuids(&a, 0x00000000);
	our_cpu.max_basic = (uint32_t)a.r.eax;
	our_cpu.vendor_str.r[0] = (uint32_t)a.r.ebx;
	our_cpu.vendor_str.r[1] = (uint32_t)a.r.edx;
	our_cpu.vendor_str.r[2] = (uint32_t)a.r.ecx;
	our_cpu.vendor_str.s[12] = '\0';
	identify_vendor(&our_cpu);

	/* get family/model/stepping stuff */
	if(our_cpu.max_basic >= 0x00000001)
	{
		cpuids(&a, 0x00000001);
		our_cpu.family   = CPUID_FAMILY((uint32_t)a.r.eax);
		our_cpu.model    = CPUID_MODEL((uint32_t)a.r.eax);
		our_cpu.stepping = CPUID_STEPPING((uint32_t)a.r.eax);
	}
	else
	{
		/* strange cpu, prop via or cyrix, cpuid but no ident... */
		our_cpu.family = 5;
		a.r.eax = a.r.ebx = a.r.ecx = a.r.edx = 0;
	}

	switch(our_cpu.vendor)
	{
	case X86_VENDOR_INTEL:
		if(our_cpu.family == 0x0F || our_cpu.family == 0x06)
			our_cpu.model += CPUID_XMODEL((uint32_t)a.r.eax) << 4;
		break;
	case X86_VENDOR_AMD:
		if(our_cpu.family == 0x0F)
			our_cpu.model += CPUID_XMODEL((uint32_t)a.r.eax) << 4;
		break;
	default:
		if(our_cpu.model == 0x0F)
			our_cpu.model += CPUID_XMODEL((uint32_t)a.r.eax) << 4;
		break;
	}

	if(our_cpu.family == 0x0F)
		our_cpu.family += CPUID_XFAMILY((uint32_t)a.r.eax);

	/* and finaly: get the features */
	our_cpu.features[0] = a.r.edx;
	our_cpu.features[1] = a.r.ecx;
	if(cpu_feature(CFEATURE_CLFSH))
		our_cpu.clflush_size = CPUID_CLFLUSHS(a.r.ebx);

	/*
	 * We simply try the extented (AMD)flags, seems this is OK and uses
	 * a loophole in the old chips that a leading 1 Bit was ignored and you
	 * would get the normal flags, but no crash.
	 */
	cpuids(&a, 0x80000000UL);
	if(((uint32_t)a.r.eax & 0xFFFF0000) == 0x80000000)
		our_cpu.max_ext = (uint32_t)a.r.eax; /* looks like we have adv flags */

	/* get extended capabilities */
	if(our_cpu.max_ext >= 0x80000001)
	{
		cpuids(&a, 0x80000001UL);
		our_cpu.features[2] = a.r.edx;
		our_cpu.features[3] = a.r.ecx;
	}

	/* Hmmm, do we have an extended model string? */
	if(our_cpu.max_ext >= 0x80000004)
	{
		char *p, *q;
		cpuids(&a, 0x80000002UL);
		for(i = 0; i < 4; i++) our_cpu.model_str.r[i] = (uint32_t)a.a[i];
		cpuids(&a, 0x80000003UL);
		for(i = 0; i < 4; i++) our_cpu.model_str.r[i+4] = (uint32_t)a.a[i];
		cpuids(&a, 0x80000004UL);
		for(i = 0; i < 4; i++) our_cpu.model_str.r[i+8] = (uint32_t)a.a[i];
		our_cpu.model_str.s[48] = '\0';

		/* Intel chips right-justify these strings, undo this */
		p = q = our_cpu.model_str.s;
		while(' ' == *p)
			p++;
		if(p != q) {
			while(*p)
				*q++ = *p++;
			while(q <= &our_cpu.model_str.s[48])
				*q++ = '\0';
		}
	}

	/* poke on the basic set again */
	if(our_cpu.max_basic >= 0x00000007)
	{
		cpuids(&a, 0x00000007);
		our_cpu.features[4] = a.r.ebx;
	}

	/* poke on the centauer extended feature flags */
// TODO: only do this on centauer?
	cpuids(&a, 0xC0000000UL);
	if(((uint32_t)a.r.eax & 0xFFFF0000) == 0xC0000000) {
		cpuids(&a, 0xC0000001UL);
		our_cpu.features[5] = a.r.edx;
	}

	/*
	 * First gen. x86-64 left out the "legacy" lahf/sahf instruction
	 * in 64 bit mode.
	 * Later gen. added it back, indicated by a cpuid flag.
	 * Unfortunatly some AMD chips forgot to set this cpuid flag.
	 * The AMD erratum says the BIOS sould enable it by poking some
	 * MSR for the affected processors.
	 * But no BIOS without bugs: Some BIOS _always_ enable it, even
	 * if the underlying CPU does not support it...
	 * Since we do not need lahf/sahf, only document this.
#ifdef(__x86_64__)
	if(our_cpu.vendor == X86_VENDOR_AMD &&
	   our_cpu.family == 0x0F &&
	   our_cpu.model  <  0x14)
		cpu_feature_clear(CFEATURE_LAHF);
#endif
	 */

	server.settings.logging.act_loglevel = LOGF_DEVEL;
#ifdef __linux__
	/* other thread impl. do not like an print so early */
	logg_posd(LOGF_DEBUG,
		"Vendor: \"%s\" Family: %d Model: %d Stepping: %d Name: \"%s\"\n",
		our_cpu.vendor_str.s, our_cpu.family, our_cpu.model,
		our_cpu.stepping, our_cpu.model_str.s);
#endif

	/* AMD performance hints */
	if(our_cpu.vendor == X86_VENDOR_AMD)
	{
		if(our_cpu.max_ext >= 0x8000001A) {
			cpuids(&a, 0x8000001AUL);
			our_cpu.features[6] = a.r.eax;
		}
	}

	if(our_cpu.vendor == X86_VENDOR_AMD && our_cpu.max_ext >= 0x80000008) {
		cpuids(&a, 0x80000008UL);
		our_cpu.num_cores = ((uint32_t)a.r.ecx & 0xFF) + 1;
		our_cpu.count = our_cpu.num_cores;
	} else if(our_cpu.vendor == X86_VENDOR_INTEL && our_cpu.max_basic >= 0x0B) {
//TODO: crazy banging on topology leaf
		/* i don't whant to know the apic ids, but... */
		our_cpu.num_cores = 1;
	}
	else /* no core info, estimate... */
		our_cpu.num_cores = 1;

	/* basicaly that's it, we don't need any deeper view into the cpu... */
	/* ... except it is an AMD Opteron */
	if(our_cpu.vendor != X86_VENDOR_AMD || our_cpu.family != 0x0F)
		return;

	/*
	 * Gosh, some weeks after the fact and you look mystified
	 * at your code...
	 *
	 * Update:
	 * Finally problem officaly affirmed.
	 *
	 * AMD erratum 147 check.
	 *
	 * Get the "Revision Guide for AMD Athlon 64 (TM) and AMD
	 * Opteron (TM) Processors", Revision >= 3.79, at least issue Date
	 * >= July 2009.
	 * Have a look at Errata 147.
	 * We should check for MSRC001_1023h bit 33 set, on every cpu,
	 * unfortunatly rdmsr is a privileged instruction...
	 * So stick to model tests.
	 *
	 * From the Erratum:
	 * "This erratum can only occur in multiprocessor or
	 * multicore configurations."
	 * (Hmmm, interresting race with the coherent HT links?
	 *  No coherent link involved, no race?)
	 *
	 * So we need the number of CPUs.
	 *
	 * We should check the SMP case more thoroughly, and maybe
	 * default to give the warning to be on the safe side.
	 *
	 * Unfortunatly this is a PITA:
	 * - Switched off processors, single cpu cpu sets, or only one
	 *   processor in a SMP config is faulty and we are not running
	 *   at test time on it (or would have to play with affinity
	 *   masks, after finding a precise num_possible_cpus in the
	 *   first place -> goto 1).
	 * - Always crying wolf is also not very fruitfull...
	 */

	/* lets ask the OS */
	our_cpu.count = get_cpus_online();
#ifdef __linux__
#  define S_STR "\nprocessor"
#  define S_SIZE (sizeof(S_STR)-1)
	/*
	 * on linux, since we know there is /proc/cpuinfo,
	 * we do not trust the query API if it says single
	 * core, just in case. This is important and we do
	 * not want to get fooled by cpusets or something
	 * like that.
	 */
	if(1 == our_cpu.count)
	{
		FILE *f;
		/* parsing something in /proc is always bad... */
		f = fopen("/proc/cpuinfo", "r");
		if(f)
		{
			char read_buf[4096]; /* should be enough to find more than 1 cpu */
			char *w_ptr;
			size_t ret;

			our_cpu.count = 0;
			read_buf[0] = '\n';
			ret = fread(read_buf + 1, 1, sizeof(read_buf) - 2, f);
			/* if we couldn't read it, simply check */
			read_buf[ret + 1] = '\0'; /* ret should be -1 at worst */
			w_ptr = read_buf;
			while((w_ptr = strstr(w_ptr, S_STR))) {
				our_cpu.count++;
				w_ptr += S_SIZE;
			}
			fclose(f);
			if(0 == our_cpu.count) /* something went wrong... */
				our_cpu.count = 1;
		}
	}
#endif

	/* if we only have 1 CPU, no problem */
	if(1 == our_cpu.count && 1 == our_cpu.num_cores)
		return;

	/*
	 * Early AMD Opterons and everything remotely derived from them
	 * seem to drop the ball on read-modify-write instructions _directly
	 * after_ a locked instruction ("lock foo; bla %reg, mem"), missing
	 * internal lfence, they say. Ok, you also need > 1 Processor,
	 * and this beeing a bad thing to happen (for example: a variable is
	 * modified before the locked instruction is visible, for example to
	 * take a lock).
	 *
	 * I don't know if we are affected (depends on code gererated by the
	 * compiler...) and how to fix it sanely to not add an if() or always
	 * a lfence (self modifing code anyone?), at least warn that something
	 * may be amiss, 'til we know what to do or if we even hit this prob.
	 *
	 * Real world evidence before the errata release (which took a long time,
	 * look at the low number...):
	 * LKML discusion pointing to perftools and MySQL
	 *  (But lost in the noise with a "let's wait for official errata")
	 * Google speaks of Opteron Rev. E Model 32..63 in their perftools stuff.
	 *  (Good informed high volume customer? Internal QA with enough
	 *   problematic machines in their cluster?)
	 * MySQL seem to hit it on 64Bit (Bugreport and triaging).
	 *  (nice finding: i wouldn't had put the blame on the CPU in the first
	 *   place but MySQL in general, esp. since it takes some time to hit it
	 *   (right timing), one would chalk it up to a fluke)
	 * Slowlaris trys to detect it, marks everything affected < Model 0x40
	 *  (but since they don't build machines with every avail. AMD
	 *   processor (only Servers with Opterons...), this smells like a
	 *   sledgehammer)
	 */
	if(our_cpu.vendor == X86_VENDOR_AMD &&
	   our_cpu.family == 0x0F &&
		our_cpu.model  >= 32 &&
		our_cpu.model  <= 63)
	{
		int ologlevel = server.settings.logging.act_loglevel;
		server.settings.logging.act_loglevel = LOGF_WARN;
		logg(LOGF_WARN, "Warning! Your specific CPU can frobnicate interlocked instruction sequences, Errata 147.\nThis may lead to errors or crashes. But there is a chance i frobnicated them myself ;-)\n");
		server.settings.logging.act_loglevel = ologlevel;
	}
	return;
}

static __init int cmp_vendor(const char *str1, const char *str2)
{
	const uint32_t *v1 = (const uint32_t *)str1, *v2 = (const uint32_t *)str2;
	return v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2];
}

static __init void identify_vendor(struct cpuinfo *cpu)
{
	char *s = cpu->vendor_str.s;
	if(cmp_vendor(s, "GenuineIntel"))
		cpu->vendor = X86_VENDOR_INTEL;
	else if(cmp_vendor(s, "AuthenticAMD"))
		cpu->vendor = X86_VENDOR_AMD;
	else if(cmp_vendor(s, "CentaurHauls"))
		cpu->vendor = X86_VENDOR_CENTAUR;
	else if(cmp_vendor(s, "GenuineTMx86"))
		cpu->vendor = X86_VENDOR_TRANSMETA;
	else if(cmp_vendor(s, "TransmetaCPU"))
		cpu->vendor = X86_VENDOR_TRANSMETA;
	else if(cmp_vendor(s, "CyrixInstead"))
		cpu->vendor = X86_VENDOR_CYRIX;
	else if(cmp_vendor(s, "SiS SiS SiS "))
		cpu->vendor = X86_VENDOR_SIS;
	else if(cmp_vendor(s, "Geode by NSC"))
		cpu->vendor = X86_VENDOR_NSC;
	else if(cmp_vendor(s, "NexGenDriven"))
		cpu->vendor = X86_VENDOR_NEXGEN;
	else if(cmp_vendor(s, "UMC UMC UMC"))
		cpu->vendor = X86_VENDOR_UMC;
	else
		cpu->vendor = X86_VENDOR_OTHER;
}

/*
 * emit a emms, or float math (with is normaly verboten)
 * may crash
 */
#define HAVE_EMIT_EMMS
void emit_emms(void)
{
// TODO: do nothing if we have SSE2?
	if(cpu_feature(CFEATURE_3DNOW))
		asm volatile("femms");
	else if(cpu_feature(CFEATURE_MMX))
		asm volatile("emms");
}

/*
 * Callback test if we have 3dNow
 */
int __init test_cpu_feature_3dnow_callback(void)
{
	return cpu_feature(CFEATURE_3DNOW);
}

int __init test_cpu_feature_3dnowprf_callback(void)
{
	return cpu_feature(CFEATURE_3DNOWPRE);
}

/*
 * Callback test if we have CMOV (thanks VIA)
 */
int __init test_cpu_feature_cmov_callback(void)
{
	return cpu_feature(CFEATURE_CMOV);
}

/*
 * Callback test if AVX can be used
 *
 * Unfortunatly Intel *always* fucks it up...
 * even when building great palaces there is always a catch.
 *
 * The AVX cpuid flags are always exposed (unlike SSE), you
 * have to check for yourself if the OS supports saving the
 * AVX state/space. The OS exposes this not-support to the
 * processor (by not setting the "yes, i will save this stuff"
 * bit), and we ask the processor whats up.
 * Instead of "removing" the AVX cpuid flags transparently,
 * we have to do the voodoo dance...
 *
 * So why participate in this dance?
 * Either the OS has AVX support, everything's fine.
 * Or not, the CPU will cowardly refuse executing AVX instruc-
 * tions and you get a SIGILL. Not our problem, user should
 * install OS-updates. Couldn't be that hard....
 *
 * Uhm, well...
 * For the new state/space, there is a new save instruction,
 * xsave/xrstore.
 * Where's the problem? Swap the instruction, in the OS,
 * not our problem.
 *
 * Easy, young hotspur. In its first incarnation xsave was a
 * compatible replacement for fxsave. But there is a catch:
 * xsave is ment to be _upward_ compatible. And at some point
 * all that state xsave may save in future processors, may
 * not fit into the reserved 512byte space.
 * What's the prob? So the OS changes the number passed
 * to malloc, done!
 *
 * No...
 *
 * This space is exposed to userspace in the sigaction signal
 * handlers (u-context and friends). And the stuff xsave and
 * esp. xrstore (a needed header) demand is /very/ ABI-incompatible.
 *
 * And at this point, even hating those super-stable enterprise
 * distros: booom, for every user, also on bleeding edge distros.
 * The same problem as with the 32bit->64bit switch and 32bit
 * only browser plugins.
 * (to recap: plugin only avail as 32bit, browser has to be 32bit,
 * all supporting libs needed as 32bit + 32bit emul in kernel)
 *
 * Now only a minority of apps fiddle with the u-context in their
 * sighandler, but still, the context gets copied around, buffered
 * in libc, passed between kernel<->userspace and at the end of a
 * sighandler (even SIGCHILD and other basic unix stuff) it gets
 * restored from there.
 *
 * So it has to be big enough, everywhere, kernel, syscall, lib{c|s},
 * apps, and better be consistent (no, it will /not/ blend!).
 * Think of obscure API-funcs (sigstack, swapcontext etc.) used by
 * VMs (python, perl, ruby, java), l33t apps, staticaly linked and
 * finaly binary-only. So changing your libc by installing a patched
 * libc package will not solve it.
 * While in the browser case, all those 32bit stuff can go to different
 * directories, now we may need new directories with an ABI bump.
 * Programs compiled against the old ABI need also all libs and foo
 * compiled against the old ABI. And maybe a flag on your ELF to say
 * "Yeah, compiled against new ABI" to activate AVX.
 * So a major update, not only a new kernel, a hole new userspace,
 * not backward compatible. There are maybe a lot of programs this
 * will not affect, but you are better save than sorry, compilers
 * are suposed to autovectorize, use SSE/AVX for fp-math etc.
 *
 * Even while Intel tried to not fuck it up, no, they not thought
 * it through (or maybe they did in a different problem space?
 * "Mo'money")...
 *
 * And this means either one bump with a new eXtensible system
 * or everytime Intel invents some new stuff exposed by xsave.
 *
 * Probably the biggest problem on this matter:
 * We are not talking about a major redesign (32bit->64bit), only
 * some extensions you can live without 99.9% of the time.
 * "Everything still works, why bother?"
 * Wow, this gives us a great critical mass to upgrade...
 *
 * On the one hand you can whine: "Waah, those software idiots,
 * not making this dynamic in the first place, bad design
 * yadda-yadda", on the other hand, who expects the spanish
 * inquisition^W^W new processor registers every two years,
 * compromising simplicity and performance (think task switch)
 * for obscure extensibility in the first place. The register
 * sets of all general purpose processors always shown itself
 * quite stable for the last 30 years. (The ucontext was always
 * a platform dependent thing, standards only saying there is
 * one, not how it should look like, only platform libs
 * pondering with it)
 *
 * This seems to change now.
 *
 * In this hole course of actions xsave can be seen as the
 * (typical) ultimate "once and for all" solution from Intels
 * side (first lacking, than pompous) to bring more dynamic
 * to those hardware dependencies.
 * Plans to "transparently" extend the AVX-registers to 512-bit
 * and more are already anounced in the first AVX "paper-only"
 * release.
 *
 * Other extensions may find their way into HW now that
 * frequency doesn't scale, more cores start to stop scaling
 * (or non super-specialised software having problems to adapt,
 * those software capable to adapt prop. supported clusters
 * before...), the new buzz seems to be special instructions
 * for everything (finaly!), all piggypacked on the SSE/AVX
 * registers or something totaly new.
 * Thanks to xsave no problem.
 *
 * Somehow nice to see CPU-manufactures "learn" again to spend
 * transistors on functional units which "do work" and not on
 * bling bling (cache, bus, buffers).
 * On the other hand back to 386: Will there a "co-pro" for my
 * needs? Propably no!
 * With Intels wild west style to include new instructions and
 * not thinking about regularity of the instruction set but
 * "need" (as in application->bussiness: "MpegDecoder"->"$$$"),
 * and whining how expensive a requested "generic" feature is
 * (e.g. good usable SIMD permutation, not all this (un)pack BS)
 * while integrating MByte of cache and other transistor eater,
 * someone is lost in hoping their needed (missing) instruction
 * will show up if he has no billion dollar business plan and a
 * platinium Intel support contract...
 * So additions once in a while can be seen as the "new green"
 * and be very likely. (Until politics change again...)
 *
 * You can mask out the saving of the new stuff, but that would
 * mean: no AVX for you. (or Extension 08/15)
 *
 * And thats why this hole "does OS suppport this" test was invented
 * IMHO (and has to be done for a long time):
 * Rapid deployment to the market without any thinking while the
 * marketing droids can generate lots of hot air about new fooo
 * (good for sales, stox, reputation). Moving the long tail totaly
 * to the software, effectivly selling snakeoil to the people, because
 * there will be a long time when people do have the most bleeding
 * edge distro (type 3 times "foo-bar update") and hardware, but
 * not a new install with dual ABI (or only new ABI) capability +
 * software making use of the new fooo (because its moot with all
 * this pain).
 * (Switching this during an simple update can be deadly, like i
 * once saw someone trying to upgrade his SuSE. Deinstalling perl
 * to resolve conflicts and prepare installing of a new perl was
 * the last action, the update needed perl for itself to run...)
 *
 * Maybe the kernel manages to save the hole state and you can
 * always use everything, but your ucontext stays compatible (you
 * can not access the new registers form there) until you set a
 * flag or whatever.
 * We will see how things will settle. A.t.m. (2008) they are working
 * on kernel patches to use xsave, but without saving extended
 * space (what will be AVX), only some data that fits into the
 * last bytes of the 512 byte space (till now marked as reserved),
 * and even that seems to be problematic (userspace reuses these
 * bytes).
 */
#define XFEATURE_ENABLED_MASK_R 0
#define XFEATURE_XMM_STATE_SAVE (1<<1)
#define XFEATURE_YMM_STATE_SAVE (1<<2)
int __init test_cpu_feature_avx_callback(void)
{
	uint32_t low, high;

	if(!cpu_feature(CFEATURE_OSXSAVE))
		return 0;

	asm volatile(
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
		"xgetbv\n"
# else
		".byte 0x0F, 0x01, 0xD0"
# endif
#else
		".byte 0x0F, 0x01, 0xD0"
#endif
		/*
		 * "force" compiler to use eax:edx, "=A" does not work for
		 * this on x86-64, seems to expand to rax:rdx, and
		 * 	mov    $0x800000000,%rax
		 * 	test   %rax,%rdx
		 * 	jne    out
		 * 	mov    %rdx,%rax
		 * 	and    $0x6,%eax
		 * 	cmp    $0x6,%rax
		 * doesn't look like the compiler completely understands
		 * where the data is if we insert a test for the 35'th bit...
		 */
		: /* %0 */ "=a" (low),
		  /* %1 */ "=d" (high)
		: /* %2 */ "c" (XFEATURE_ENABLED_MASK_R)
	);

	if((low & (XFEATURE_XMM_STATE_SAVE|XFEATURE_YMM_STATE_SAVE)) !=
	   (XFEATURE_XMM_STATE_SAVE|XFEATURE_YMM_STATE_SAVE))
		return 0;

	return 1;
}

/*
 * Test a feature on a x86-CPU and change function pointer
 */
__init void *test_cpu_feature(const struct test_cpu_feature *t, size_t l)
{
	size_t i;
	identify_cpu();
	for(i = 0; i < l; i++)
	{
		if(-1 == t[i].flags_needed ||
		   cpu_feature(t[i].flags_needed)) {
			if(t[i].callback && !t[i].callback())
				continue;
			return t[i].func;
		}
	}
	return NULL; /* whoever fucked it up, die! */
}

static char const rcsid_mbx[] GCC_ATTR_USED_VAR = "$Id:$";
