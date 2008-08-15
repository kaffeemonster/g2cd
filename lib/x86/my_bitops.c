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
static inline bool has_cpuid(void)
{
	const size_t id_mask = (1 << 21);
	size_t f;

	f = read_flags();
	write_flags(f | id_mask);
	f = read_flags() & id_mask;
	if (!f)
		return false;
	f = read_flags() & ~id_mask;
	write_flags(f);
	f = read_flags() & id_mask;
	if (f)
		return false;

	return true;
}

static void identify_cpu(void)
{
	struct cpuid_regs a;
	unsigned i;

	if(our_cpu.init_done)
		return;

	if(unlikely(!has_cpuid())) {
		/* distinguish 386 from 486, same trick for EFLAGS bit 18 */
		strcpy(our_cpu.vendor_str.s, "{3|4}86??");
		our_cpu.count = 1;
		our_cpu.family = 4;
		our_cpu.init_done = true;
		logg_pos(LOGF_DEBUG, "Looks like this is an CPU older Pentium I???\n");
		return;
	}

	cpuids(&a, 0x00000000);
	our_cpu.max_basic = a.eax;
	our_cpu.vendor_str.r[0] = a.ebx;
	our_cpu.vendor_str.r[1] = a.edx;
	our_cpu.vendor_str.r[2] = a.ecx;
	our_cpu.vendor_str.s[12] = '\0';
	identify_vendor(&our_cpu);

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

	/* Hmmm, do we have extended model strings? */
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

	/* set the cpu count to a default value, we must have at least one ;) */
	our_cpu.count = 1;

	our_cpu.init_done = true;
	if(our_cpu.vendor != X86_VENDOR_AMD || our_cpu.family != 0x0F)
		return;

	if(our_cpu.max_ext >= 0x80000008) {
		cpuids(&a, 0x80000008);
		our_cpu.num_cores = (a.ecx & 0xFF) + 1;
		if(1 == our_cpu.num_cores)
			return;
		our_cpu.count = our_cpu.num_cores;
	}
	else
	{
#ifdef __linux__
#  define S_STR "\nprocessor"
#  define S_SIZE (sizeof(S_STR)-1)
	{
		FILE *f;

		f = fopen("/proc/cpuinfo", "r");
		/* if we couldn't read it, simply check*/
		if(f)
		{
			char read_buf[4096];
			char *w_ptr;
			size_t ret;

			read_buf[0] = '\n';
			ret = fread(read_buf + 1, 1, sizeof(read_buf) - 2, f);
			read_buf[ret + 1] = '\0';
			w_ptr = read_buf;
			while((w_ptr = strstr(w_ptr, S_STR))) {
				our_cpu.count++;
				w_ptr += S_SIZE;
			}
			fclose(f);
			/* if we only have 1 CPU, no problem */
			if(1 == our_cpu.count)
				return;
		}
	}
#endif
	}
	/* 
	 * early AMD Opterons and everything remotely derived from them
	 * seem to drop the ball on read-modify-write instructions after a
	 * locked instruction (missing internal lfence, they say). Ok, you
	 * also need > 1 Processor.
	 * This is unfortunatly all wild speculation, no (visible) Errata,
	 * no info, but:
	 * Google speaks of Opteron Rev. E Model 32..63 in their perftools stuff
	 * MySQL seem to hit it on 64Bit
	 * Slowlaris trys to detect it, marks everything affected < Model 0x40
	 *  (but since they don't build mashines with every avail. AMD
	 *  processor (only Servers with Opterons...), this smells like a
	 *  sledgehammer)
	 */
	if(our_cpu.vendor == X86_VENDOR_AMD &&
	   our_cpu.family == 0x0F &&
		our_cpu.model  >= 32 &&
		our_cpu.model  <= 63) {
		int ologlevel = server.settings.logging.act_loglevel;
		server.settings.logging.act_loglevel = LOGF_WARN;
		logg(LOGF_WARN, "Warning! Your specific CPU can frobnicate interlocked instruction sequences.\nThis may leed to errors or crashes. But there is a chance i frobnicatet it myself;-)\n");
		server.settings.logging.act_loglevel = ologlevel;
	}
	return;
}

static void identify_vendor(struct cpuinfo *cpu)
{
	if(!strcmp(cpu->vendor_str.s, "GenuineIntel"))
		cpu->vendor = X86_VENDOR_INTEL;
	else if(!strcmp(cpu->vendor_str.s, "AuthenticAMD"))
		cpu->vendor = X86_VENDOR_AMD;
	else
		cpu->vendor = X86_VENDOR_OTHER;
}

void test_cpu_feature(void (**func)(void), struct test_cpu_feature *t, size_t l)
{
	size_t i;
	identify_cpu();
	for(i = 0; i < l; i++)
	{
		if(-1 == t[i].flags_needed ||
		   our_cpu.features[t[i].flags_needed]) {
			*func = t[i].func;
			return;
		}
	}
}

static char const rcsid_mbx[] GCC_ATTR_USED_VAR = "$Id:$";
