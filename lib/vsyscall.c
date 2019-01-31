#include <inc/vsyscall.h>
#include <inc/lib.h>

static inline int32_t
vsyscall(int num)
{
	// LAB 12: Your code here.
	if (num == VSYS_gettime) {
		// int32_t unix_time = 0;
		// memcpy(&unix_time, (vsys[0]), sizeof(int32_t));
		return vsys[VSYS_gettime];
	} else {
		return -E_INVAL;
	}
}

int vsys_gettime(void)
{
	return vsyscall(VSYS_gettime);
}
