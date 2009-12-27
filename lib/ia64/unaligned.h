#ifndef _ASM_IA64_UNALIGNED_H
#define _ASM_IA64_UNALIGNED_H

#include "../unaligned/le_struct.h"
#include "../unaligned/be_byteshift.h"
#include "../unaligned/generic.h"

#define get_unaligned	__get_unaligned_le
#define put_unaligned	__put_unaligned_le

#endif /* _ASM_IA64_UNALIGNED_H */
