/*
 * x86_features.h
 * x86 feature bits
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

#ifndef X86_FEATURE_H
# define X86_FEATURE_H

# define FEATURE1(x,y) ENUM_CMD(x, y)
# define FEATURE2(x,y) ENUM_CMD(x, (y+32))
# define FEATURE3(x,y) ENUM_CMD(x, (y+64))
# define FEATURE4(x,y) ENUM_CMD(x, (y+96))
# define X86_CPU_FEATURE_ENUM \
	FEATURE1( FPU     ,  0 ), \
	FEATURE1( VME     ,  1 ), \
	FEATURE1( DE      ,  2 ), \
	FEATURE1( PSE     ,  3 ), \
	FEATURE1( TSC     ,  4 ), \
	FEATURE1( MSR     ,  5 ), \
	FEATURE1( PAE     ,  6 ), \
	FEATURE1( MCE     ,  7 ), \
	FEATURE1( CX8     ,  8 ), \
	FEATURE1( APIC    ,  9 ), \
	FEATURE1( SEP     , 11 ), \
	FEATURE1( MTRR    , 12 ), \
	FEATURE1( PGE     , 13 ), \
	FEATURE1( MCA     , 14 ), \
	FEATURE1( CMOV    , 15 ), \
	FEATURE1( PAT     , 16 ), \
	FEATURE1( PSE36   , 17 ), \
	FEATURE1( PSN     , 18 ), \
	FEATURE1( CFLSH   , 19 ), \
	FEATURE1( DS      , 21 ), \
	FEATURE1( ACPI    , 22 ), \
	FEATURE1( MMX     , 23 ), \
	FEATURE1( FXSR    , 24 ), \
	FEATURE1( SSE     , 25 ), \
	FEATURE1( SSE2    , 26 ), \
	FEATURE1( SS      , 27 ), \
	FEATURE1( HTT     , 28 ), \
	FEATURE1( TM      , 29 ), \
	FEATURE1( PBE     , 31 ), \
	FEATURE2( SSE3    ,  0 ), \
	FEATURE2( MONITOR ,  3 ), \
	FEATURE2( DSCPL   ,  4 ), \
	FEATURE2( VMX     ,  5 ), \
	FEATURE2( SMX     ,  6 ), \
	FEATURE2( EST     ,  7 ), \
	FEATURE2( TM2     ,  8 ), \
	FEATURE2( SSSE3   ,  9 ), \
	FEATURE2( CNXTID  , 10 ), \
	FEATURE2( CX16    , 13 ), \
	FEATURE2( XTPR    , 14 ), \
	FEATURE2( PDCM    , 15 ), \
	FEATURE2( DCA     , 18 ), \
	FEATURE2( SSE4_1  , 19 ), \
	FEATURE2( SSE4_2  , 20 ), \
	FEATURE2( X2APIC  , 21 ), \
	FEATURE2( MOVBE   , 22 ), \
	FEATURE2( POPCNT  , 23 ), \
	FEATURE2( XSAVE   , 26 ), \
	FEATURE2( OSXSAVE , 27 ), \
	FEATURE3( SYSCALL , 11 ), \
	FEATURE3( NX      , 20 ), \
	FEATURE3( MMXEXT  , 22 ), \
	FEATURE3( FFXSR   , 25 ), \
	FEATURE3( PAGE1GB , 26 ), \
	FEATURE3( RDTSCP  , 27 ), \
	FEATURE3( LM      , 29 ), \
	FEATURE3( 3DNOWEXT, 30 ), \
	FEATURE3( 3DNOW   , 31 ), \
	FEATURE4( LAHF    ,  0 ), \
	FEATURE4( CMPLEGA ,  1 ), \
	FEATURE4( SVM     ,  2 ), \
	FEATURE4( EAPIC   ,  3 ), \
	FEATURE4( AMOVCR  ,  4 ), \
	FEATURE4( ABM     ,  5 ), \
	FEATURE4( SSE4A   ,  6 ), \
	FEATURE4( MASSE   ,  7 ), \
	FEATURE4( 3DNOWPRE,  8 ), \
	FEATURE4( OSVW    ,  9 ), \
	FEATURE4( IBS     , 10 ), \

# define ENUM_CMD(x,y) CFEATURE_##x = y
enum x86_cpu_features
{
	X86_CPU_FEATURE_ENUM
};
# undef ENUM_CMD

extern const char x86_cpu_feature_names[][16] GCC_ATTR_VIS("hidden");

#endif
