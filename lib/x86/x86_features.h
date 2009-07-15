/*
 * x86_features.h
 * x86 feature bits
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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

#ifndef X86_FEATURE_H
# define X86_FEATURE_H

# define FEATURE1(x,y,z) ENUM_CMD(x,  y)
# define FEATURE2(x,y,z) ENUM_CMD(x, (y +  32))
# define FEATURE3(x,y,z) ENUM_CMD(x, (y +  64))
# define FEATURE4(x,y,z) ENUM_CMD(x, (y +  96))
# define FEATURE5(x,y,z) ENUM_CMD(x, (y + 128))
# define X86_CPU_FEATURE_ENUM \
	FEATURE1( FPU      ,  0, "FPU on chip"                       ), \
	FEATURE1( VME      ,  1, "Virtual-8086 Mode Extensions"      ), \
	FEATURE1( DE       ,  2, "Debugging Extensions"              ), \
	FEATURE1( PSE      ,  3, "Page Size Extensions"              ), \
	FEATURE1( TSC      ,  4, "Time Stamp Counter"                ), \
	FEATURE1( MSR      ,  5, "RDMSR avail."                      ), \
	FEATURE1( PAE      ,  6, "Phys. Address Extensions"          ), \
	FEATURE1( MCE      ,  7, "Machine Check Execptions"          ), \
	FEATURE1( CX8      ,  8, "CMPXCHG8 instruction"              ), \
	FEATURE1( APIC     ,  9, "APIC on chip"                      ), \
	FEATURE1( RES_1_10 , 10, "Reserverd"                         ), \
	FEATURE1( SEP      , 11, "SYSENTER avail."                   ), \
	FEATURE1( MTRR     , 12, "Mem. Type Range Regs"              ), \
	FEATURE1( PGE      , 13, "PTE Global Bit"                    ), \
	FEATURE1( MCA      , 14, "Machine Check Arch."               ), \
	FEATURE1( CMOV     , 15, "CMOV avail."                       ), \
	FEATURE1( PAT      , 16, "Page Attribute Table"              ), \
	FEATURE1( PSE36    , 17, "Page Size Extention"               ), \
	FEATURE1( PSN      , 18, "Processor Serial Number"           ), \
	FEATURE1( CFLSH    , 19, "CFLUSH avail."                     ), \
	FEATURE1( DS       , 21, "Debug Store"                       ), \
	FEATURE1( ACPI     , 22, "Thermal Monitoring and Clock Ctrl" ), \
	FEATURE1( MMX      , 23, "MMX Technology"                    ), \
	FEATURE1( FXSR     , 24, "FXSAVE avail."                     ), \
	FEATURE1( SSE      , 25, "SSE Extensions"                    ), \
	FEATURE1( SSE2     , 26, "SSE2 Extensions"                   ), \
	FEATURE1( SS       , 27, "Self Snoop Support"                ), \
	FEATURE1( HTT      , 28, "SMT Multi-threading"               ), \
	FEATURE1( TM       , 29, "Thermal Monitor"                   ), \
	FEATURE1( IA64     , 30, "IA-64, Itanium in x86 mode"        ), \
	FEATURE1( PBE      , 31, "Pending Break Enable"              ), \
	FEATURE2( SSE3     ,  0, "SSE3 Extensions"                   ), \
	FEATURE2( PCLMULQDQ,  1, "PCLMULQDQ avail.(Intel:Res.Mar-09)"), \
	FEATURE2( DTES64   ,  2, "64-Bit Debug Store"                ), \
	FEATURE2( MONITOR  ,  3, "MONITOR avail."                    ), \
	FEATURE2( DSCPL    ,  4, "CPL Qualified Debug Store"         ), \
	FEATURE2( VMX      ,  5, "Virtual Machine Extensions"        ), \
	FEATURE2( SMX      ,  6, "Safer Mode Extensions"             ), \
	FEATURE2( EST      ,  7, "Enhanced Speed Step Technology"    ), \
	FEATURE2( TM2      ,  8, "Thermal Monitor 2"                 ), \
	FEATURE2( SSSE3    ,  9, "Supplemental SSE3 Extentions"      ), \
	FEATURE2( CNXTID   , 10, "L1 Cache Context switch avail."    ), \
	FEATURE2( RES_2_11 , 11, "Reserverd"                         ), \
	FEATURE2( FMA      , 12, "Fused Multiply Add Extension"      ), \
	FEATURE2( CX16     , 13, "CMPXCHG16 avail."                  ), \
	FEATURE2( XTPR     , 14, "xTPR Update Control"               ), \
	FEATURE2( PDCM     , 15, "Performance and Debug Capabillity" ), \
	FEATURE2( RES_2_16 , 16, "Reserved"                          ), \
	FEATURE2( RES_2_17 , 17, "Reserved"                          ), \
	FEATURE2( DCA      , 18, "Direct Cache Access"               ), \
	FEATURE2( SSE4_1   , 19, "SSE4.1 Extensions"                 ), \
	FEATURE2( SSE4_2   , 20, "SSE4.2 Extensions"                 ), \
	FEATURE2( X2APIC   , 21, "x2APIC avail."                     ), \
	FEATURE2( MOVBE    , 22, "MOVBE avail."                      ), \
	FEATURE2( POPCNT   , 23, "POPCNT avail."                     ), \
	FEATURE2( RES_2_24 , 24, "Reserved"                          ), \
	FEATURE2( AES      , 25, "AES Support"                       ), \
	FEATURE2( XSAVE    , 26, "XSAVE avail."                      ), \
	FEATURE2( OSXSAVE  , 27, "OS supports XSAVE"                 ), \
	FEATURE2( AVX      , 28, "AVX Extensions"                    ), \
	FEATURE2( RES_2_29 , 29, "Reserved"                          ), \
	FEATURE2( RES_2_30 , 30, "Reserved"                          ), \
	FEATURE2( RAZ      , 31, "WTF? found in AMD CPUID spec."     ), \
	FEATURE3( MIR_FPU  ,  0, "FPU on Chip, mirrored"             ), \
	FEATURE3( MIR_VME  ,  1, "Virtual Mode Extensions, mirrored" ), \
	FEATURE3( MIR_DE   ,  2, "Debugging Extensions, mirrored"    ), \
	FEATURE3( MIR_PSE  ,  3, "Page Size Extensions, mirrored"    ), \
	FEATURE3( MIR_TSC  ,  4, "Time Stamp Counter, mirrored"      ), \
	FEATURE3( MIR_MSR  ,  5, "RDMSR avail., mirrored"            ), \
	FEATURE3( MIR_PAE  ,  6, "Phys. Address Extensions, mirrored"), \
	FEATURE3( MIR_MCE  ,  7, "Machine Check Execptions, mirrored"), \
	FEATURE3( MIR_CX8  ,  8, "CMPXCHG8 instruction, mirrored"    ), \
	FEATURE3( MIR_APIC ,  9, "APIC on chip, mirrored"            ), \
	FEATURE3( RES_3_10 , 10, "Reserverd"                         ), \
	FEATURE3( SYSCALL  , 11, "SYSCALL avail."                    ), \
	FEATURE3( MIR_MTRR , 12, "Mem. Type Range Regs, mirrored"    ), \
	FEATURE3( MIR_PGE  , 13, "PTE Global Bit, mirrored"          ), \
	FEATURE3( MIR_MCA  , 14, "Machine Check Arch., mirrored"     ), \
	FEATURE3( MIR_CMOV , 15, "CMOV avail., mirrored"             ), \
	FEATURE3( MIR_PAT  , 16, "Page Attribute Table, mirrored"    ), \
	FEATURE3( MIR_PSE36, 17, "Page Size Extention, mirrored"     ), \
	FEATURE3( RES_3_18 , 18, "Reserverd"                         ), \
	FEATURE3( RES_3_19 , 19, "Reserverd"                         ), \
	FEATURE3( NX       , 20, "Execute Disable"                   ), \
	FEATURE3( RES_3_21 , 21, "Reserverd"                         ), \
	FEATURE3( MMXEXT   , 22, "MMX Technology Extensions"         ), \
	FEATURE3( MIR_MMX  , 23, "MMX Technology, mirrored"          ), \
	FEATURE3( MIR_FXSR , 24, "FXSAVE avail., mirrored"           ), \
	FEATURE3( FFXSR    , 25, "FXSAVE optimizations"              ), \
	FEATURE3( PAGE1GB  , 26, "1 Giga byte pages supported"       ), \
	FEATURE3( RDTSCP   , 27, "RDTSCP avail."                     ), \
	FEATURE3( LM       , 29, "Longmode, 64Bit avail."            ), \
	FEATURE3( 3DNOWEXT , 30, "3DNow! Extended supported"         ), \
	FEATURE3( 3DNOW    , 31, "3DNow! supported"                  ), \
	FEATURE4( LAHF     ,  0, "LAHF avail. (in 64Bit)"            ), \
	FEATURE4( CMPLEGA  ,  1, "If yes HyperThreading not valid"   ), \
	FEATURE4( SVM      ,  2, "Secure Virtual Machine"            ), \
	FEATURE4( EAPIC    ,  3, "Extended APIC space"               ), \
	FEATURE4( AMOVCR   ,  4, "CR8 in 32Bit mode"                 ), \
	FEATURE4( ABM      ,  5, "Advanced bit manipulation"         ), \
	FEATURE4( SSE4A    ,  6, "SSE4a Extensions"                  ), \
	FEATURE4( MASSE    ,  7, "Misaligned SSE mode"               ), \
	FEATURE4( 3DNOWPRE ,  8, "3DNow! Prefetch instructions"      ), \
	FEATURE4( OSVW     ,  9, "OS Visible Workaround"             ), \
	FEATURE4( IBS      , 10, "Instruction Based Sampling"        ), \
	FEATURE4( XOP      , 11, "XOP Extensions"                    ), \
	FEATURE4( SKINIT   , 12, "SKINIT avail."                     ), \
	FEATURE4( WDT      , 13, "Watchdog timer"                    ), \
	FEATURE4( FMA4     , 16, "FMA AMD-Style"                     ), \
	FEATURE4( CVT16    , 18, "Half Float extention"              ), \
	FEATURE5( PL_RNG   ,  2, "Padlock Random Number Generator"   ), \
	FEATURE5( PL_RNG_E ,  3, "Padlock RNG enabled"               ), \
	FEATURE5( PL_ACE   ,  6, "Padlock Advanced Coding ..."       ), \
	FEATURE5( PL_ACE_E ,  7, "Padlock ACE enabled"               ), \
	FEATURE5( PL_ACE2  ,  8, "Padlock ACE2 avail."               ), \
	FEATURE5( PL_ACE2_E,  9, "Padlock ACE2 enabled"              ), \
	FEATURE5( PL_PHE   , 10, "Padlock Hashing Engine"            ), \
	FEATURE5( PL_PHE_E , 11, "Padlock HE enabled"                ), \
	FEATURE5( PL_PMM   , 12, "Padlock Montgommery Multiplier"    ), \
	FEATURE5( PL_PMM_E , 13, "Padlock MM enabled"                ),

# define ENUM_CMD(x,y) CFEATURE_##x = y
enum x86_cpu_features
{
	X86_CPU_FEATURE_ENUM
};
# undef ENUM_CMD

extern const char x86_cpu_feature_names[][16] GCC_ATTR_VIS("hidden");

#endif
