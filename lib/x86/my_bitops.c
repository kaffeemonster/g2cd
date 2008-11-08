/*
 * my_bitops.c
 * some nity grity bitops, x86 implementation
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../../other.h"
#include "../log_facility.h"
#include "x86_features.h"

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
	bool features[128];
	bool init_done;
};

struct cpuid_regs
{
	uint32_t eax, ebx, ecx, edx;
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
	asm volatile(
		"pushf\n\t"
		"pop %0\n\t"
		: "=r" (f)
	);
	return f;
}

static always_inline void write_flags(size_t f)
{
	asm volatile(
		"push %0\n\t"
		"popf\n\t"
		: : "r" (f) : "cc"
	);
}

static always_inline void cpuid(struct cpuid_regs *regs, uint32_t func)
{
	/* save ebx around cpuid call, PIC code needs it */
	asm volatile (
		"xchg	%1, %%ebx\n\t"
		"cpuid\n\t"
		"xchg	%1, %%ebx\n"
		: /* %0 */ "=a" (regs->eax),
		  /* %1 */ "=r" (regs->ebx),
		  /* %2 */ "=c" (regs->ecx),
		  /* %4 */ "=d" (regs->edx)
		: /* %5 */ "0" (func),
		  /* %6 */ "2" (regs->ecx)
		: "cc"
	);
}

static always_inline void cpuids(struct cpuid_regs *regs, uint32_t func)
{
	regs->ecx = 0;
	cpuid(regs, func);
}


/*
 * funcs
 */
static inline bool toggle_eflags_test(const size_t mask)
{
	size_t f;

	f = read_flags();
	write_flags(f | mask);
	f = read_flags() & mask;
	if (!f)
		return false;
	f = read_flags() & ~mask;
	write_flags(f);
	f = read_flags() & mask;
	if (f)
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

static void identify_cpu(void)
{
	struct cpuid_regs a;
	unsigned i;

	if(our_cpu.init_done)
		return;

	/* set the cpu count to a default value, we must have at least one ;) */
	our_cpu.count = 1;

	/* do we have cpuid? we don't want to SIGILL */
	if(unlikely(!has_cpuid())) {
		/*
		 * No? *cough* Ok, maybe rare/exotic chip like Geode
		 * which can switch of cpuid in firmware...
		 */
		 /* distinguish 386 from 486, same trick for EFLAGS bit 18 */
		if(is_486()) {
			strcpy(our_cpu.vendor_str.s, "486?");
			our_cpu.family = 4;
		} else {
			strcpy(our_cpu.vendor_str.s, "386??");
			our_cpu.family = 3;
		}
		our_cpu.init_done = true;
		logg_pos(LOGF_DEBUG, "Looks like this is an CPU older Pentium I???\n");
		return;
	}

	/* get vendor string */
	cpuids(&a, 0x00000000);
	our_cpu.max_basic = a.eax;
	our_cpu.vendor_str.r[0] = a.ebx;
	our_cpu.vendor_str.r[1] = a.edx;
	our_cpu.vendor_str.r[2] = a.ecx;
	our_cpu.vendor_str.s[12] = '\0';
	identify_vendor(&our_cpu);

	/* get family/model/stepping stuff */
	if(our_cpu.max_basic >= 0x00000001)
	{
		cpuids(&a, 0x00000001);
		our_cpu.family   = CPUID_FAMILY(a.eax);
		our_cpu.model    = CPUID_MODEL(a.eax);
		our_cpu.stepping = CPUID_STEPPING(a.eax);
	}
	else
	{
		/* strange cpu, prop via or cyrix, cpuid but no ident... */
		our_cpu.family = 5;
		a.eax = a.ebx = a.ecx = a.edx = 0;
	}

	switch(our_cpu.vendor)
	{
	case X86_VENDOR_INTEL:
		if(our_cpu.family == 0x0F || our_cpu.family == 0x06)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	case X86_VENDOR_AMD:
		if(our_cpu.family == 0x0F)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	default:
		if(our_cpu.model == 0x0F)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	}

	if(our_cpu.family == 0x0F)
		our_cpu.family += CPUID_XFAMILY(a.eax);

	/* and finaly: get the features */
	for(i = 0; i < 32; i++) {
		if(a.edx & (1 << i))
			our_cpu.features[i] = true;
	}
	for(i = 0; i < 32; i++) {
		if(a.ecx & (1 << i))
			our_cpu.features[i + 32] = true;
	}

	/*
	 * We simply try the extented (AMD)flags, seems this is OK and uses
	 * a loophole in the old chips that a leading 1 Bit was ignored and you
	 * would get the normal flags, but no crash.
	 */
	cpuids(&a, 0x80000000);
	if((a.eax & 0xFFFF0000) == 0x80000000)
		our_cpu.max_ext = a.eax; /* looks like we have adv flags */

	/* get extended capabilities */
	if(our_cpu.max_ext >= 0x80000001)
	{
		cpuids(&a, 0x80000001);
		for(i = 0; i < 32; i++) {
			if(a.edx & (1 << i))
				our_cpu.features[i + 64] = true;
		}
		for(i = 0; i < 32; i++) {
			if(a.ecx & (1 << i))
				our_cpu.features[i + 96] = true;
		}
	}

	/* Hmmm, do we have a extended model string? */
	if(our_cpu.max_ext >= 0x80000004)
	{
		char *p, *q;
		cpuids((struct cpuid_regs *)&our_cpu.model_str.r[0], 0x80000002);
		cpuids((struct cpuid_regs *)&our_cpu.model_str.r[4], 0x80000003);
		cpuids((struct cpuid_regs *)&our_cpu.model_str.r[8], 0x80000004);
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

	server.settings.logging.act_loglevel = LOGF_DEVEL;
	logg_posd(LOGF_DEBUG,
		"Vendor: \"%s\" Family: %d Model: %d Stepping: %d Name: \"%s\"\n",
		our_cpu.vendor_str.s, our_cpu.family, our_cpu.model,
		our_cpu.stepping, our_cpu.model_str.s);

	/* basicaly that's it, we don't need any deeper view into the cpu... */
	our_cpu.init_done = true;
	/* ... except it is an AMD Opteron */
	if(our_cpu.vendor != X86_VENDOR_AMD || our_cpu.family != 0x0F)
		return;

	/*
	 * Gosh, some weeks after the fact and you look mystified
	 * at your code...
	 * This is only a problem if we are on multicore chips???
	 */
	if(our_cpu.max_ext >= 0x80000008) {
		cpuids(&a, 0x80000008);
		our_cpu.num_cores = (a.ecx & 0xFF) + 1;
		if(1 == our_cpu.num_cores)
			return;
		our_cpu.count = our_cpu.num_cores;
	}
	else
	{
		/* no core info, estimate... */
#ifdef __linux__
#  define S_STR "\nprocessor"
#  define S_SIZE (sizeof(S_STR)-1)
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
			read_buf[ret + 1] = '\0';
			w_ptr = read_buf;
			while((w_ptr = strstr(w_ptr, S_STR))) {
				our_cpu.count++;
				w_ptr += S_SIZE;
			}
			fclose(f);
			if(0 == our_cpu.count) /* something went wrong... */
				our_cpu.count = 1;
			/* if we only have 1 CPU, no problem */
			if(1 == our_cpu.count)
				return;
		}
	}
#endif
	}
	/*
	 * early AMD Opterons and everything remotely derived from them
	 * seem to drop the ball on read-modify-write instructions _directly
	 * after_ a locked instruction ("lock foo; bla reg, mem", missing
	 * internal lfence, they say). Ok, you also need > 1 Processor.
	 * This is unfortunatly all wild speculation, no (visible) Errata,
	 * no info, but:
	 * Google speaks of Opteron Rev. E Model 32..63 in their perftools stuff
	 * MySQL seem to hit it on 64Bit
	 * Slowlaris trys to detect it, marks everything affected < Model 0x40
	 *  (but since they don't build machines with every avail. AMD
	 *  processor (only Servers with Opterons...), this smells like a
	 *  sledgehammer)
	 *
	 * I don't know if we are affected (depends on code gererated by
	 * compiler...) and how to fix it sanely to not add an if() or always
	 * an lfence (self modifing code anyone?) at least warn that something
	 * may be amies, 'til we know what to do or if we even hit this prob.
	 */
	if(our_cpu.vendor == X86_VENDOR_AMD &&
	   our_cpu.family == 0x0F &&
		our_cpu.model  >= 32 &&
		our_cpu.model  <= 63)
	{
		int ologlevel = server.settings.logging.act_loglevel;
		server.settings.logging.act_loglevel = LOGF_WARN;
		logg(LOGF_WARN, "Warning! Your specific CPU can frobnicate interlocked instruction sequences, they say.\nThis may leed to errors or crashes. But there is a chance i frobnicated them myself ;-)\n");
		server.settings.logging.act_loglevel = ologlevel;
	}
	return;
}

static void identify_vendor(struct cpuinfo *cpu)
{
	char *s = cpu->vendor_str.s;
	if(!strcmp(s, "GenuineIntel"))
		cpu->vendor = X86_VENDOR_INTEL;
	else if(!strcmp(s, "AuthenticAMD"))
		cpu->vendor = X86_VENDOR_AMD;
	else if(!strcmp(s, "CentaurHauls"))
		cpu->vendor = X86_VENDOR_CENTAUR;
	else if(!strcmp(s, "GenuineTMx86"))
		cpu->vendor = X86_VENDOR_TRANSMETA;
	else if(!strcmp(s, "TransmetaCPU"))
		cpu->vendor = X86_VENDOR_TRANSMETA;
	else if(!strcmp(s, "CyrixInstead"))
		cpu->vendor = X86_VENDOR_CYRIX;
	else if(!strcmp(s, "SiS SiS SiS "))
		cpu->vendor = X86_VENDOR_SIS;
	else if(!strcmp(s, "Geode by NSC"))
		cpu->vendor = X86_VENDOR_NSC;
	else if(!strcmp(s, "NexGenDriven"))
		cpu->vendor = X86_VENDOR_NEXGEN;
	else if(!strcmp(s, "UMC UMC UMC"))
		cpu->vendor = X86_VENDOR_UMC;
	else
		cpu->vendor = X86_VENDOR_OTHER;
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
 * sighandler it gets restored from there.
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
 * will not affect, but you are better save than sorry, compiler are
 * suposed to autovectorize, use SSE for fp-math etc.
 * (even while Intel tried to not fuck it up, no they not thought it through)
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
 * compromising simplicity and performance for obscure
 * extensibility in the first place (the ucontext was always
 * a platform dependent thing, standards only saying there is
 * one, not how it should look like, only platform libs
 * pondering with it). The register sets of all general
 * purpose processors always shown itself quite stable
 * for the last 30 years...
 *
 * This seems to change now.
 *
 * In this hole course of actions xsave can be seen as the
 * ultimate "once and for all" solution from the HW manufactures
 * side to bring more dynamic to those hardware dependencies.
 * Plans to "transparently" extend the AVX-register to 512-bit
 * and more are already anounced in the first AVX release.
 * Other extensions may find their way into HW now that
 * frequency doesn't scale, more cores start to stop scaling
 * (or non super-specialised software having problems to adapt,
 * those software capable to adapt prop. supported clusters
 * before...), the new buzz seems to be special instructions
 * for everything (finaly!), all piggypacked on the SSE/AVX
 * registers or something totaly new.
 * Thanks to xsave no problem.
 * Somehow nice to see CPU-manufactures "learn" again to spend
 * transistors on functional units which "do work" and not on
 * bling bling (cache, bus, buffers).
 * On the other hand back to 386: is there a co-pro (for my needs)?
 * With Intels wild west style to include new instructions and
 * not thinking about regularity of the instruction set but "need",
 * and whining how expensive something is while integrating MByte
 * of cache and other transitor eater, someone is lost in hoping
 * their needed instruction will show up if he has no billion dollar
 * business plan and a premier Intel support contract...
 * So additions once in a while can be seen as the "new green" and
 * be very likely. (Until politics change again...)
 *
 * You can mask out the saving of the new stuff, but that would
 * mean: no AVX for you. (or Extension 314)
 * And thats why this hole "does OS suppport this" test was invented
 * and has to be done for a long time:
 * There will be a long time when people do have the most bleeding
 * edge distro (type 3 times "foo-bar update") and hardware, but
 * not a new install with dual ABI (or only new ABI) capability.
 * (Switching this during an simple update can be deadly, like i
 * once saw someone trying to upgrade his SuSE. Desinstalling perl
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
int test_cpu_feature_avx_callback(void)
{
	uint32_t low, high;

	if(!our_cpu.features[CFEATURE_OSXSAVE])
		return 0;

	asm volatile(
#if HAVE_BINUTILS_VERSION >= 219
		"xgetbv\n"
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
void test_cpu_feature(void *f, const struct test_cpu_feature *t, size_t l)
{
	size_t i;
	void (**func)(void) = f;
	identify_cpu();
	for(i = 0; i < l; i++)
	{
		if(-1 == t[i].flags_needed ||
		   our_cpu.features[t[i].flags_needed]) {
			if(t[i].callback && !t[i].callback())
				continue;
			*func = t[i].func;
			return;
		}
	}
}

static char const rcsid_mbx[] GCC_ATTR_USED_VAR = "$Id:$";
