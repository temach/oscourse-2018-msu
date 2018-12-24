/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <kern/kclock.h>

// See https://wiki.osdev.org/RTC: 0x70 is used to set address for CMOS RAM (address is 0x0A, 0x0B, 0x0C as only 3 regs are availiable)
// 0x71 is used to send data to that address.
void
rtc_init(void)
{
	nmi_disable();
	// LAB 4: your code here

	// select reg B with port io
	outb(IO_RTC_CMND, RTC_BREG);
	// read data with port io from selected reg
	uint8_t reg_b = inb(IO_RTC_DATA);
	reg_b |= RTC_PIE;
	// write data with port io to selected reg
	outb(IO_RTC_DATA, reg_b);

	// select reg A with port io
	outb(IO_RTC_CMND, RTC_AREG);
	uint8_t reg_a = inb(IO_RTC_DATA);
	// set oscilator frequency the low 4 bits of A register
	reg_a |= 0x0F;
	outb(IO_RTC_DATA, reg_a);

	nmi_enable();
}

uint8_t
rtc_check_status(void)
{
	uint8_t status = 0;
	// LAB 4: your code here

	nmi_disable();
	// select reg C with port io
	outb(IO_RTC_CMND, RTC_CREG);
	// read data with port io from selected reg
	status = inb(IO_RTC_DATA);
	nmi_enable();

	return status;
}

