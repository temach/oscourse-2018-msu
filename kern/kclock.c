/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <kern/kclock.h>
#include <inc/time.h>
#include <inc/vsyscall.h>
#include <kern/vsyscall.h>

#include <inc/string.h>


#define RTC_GETTIME_REG_NUM (6)

static void wait_while_updating(void)
{
	uint8_t reg_a = ~0;
	// loop while the update in progress bit is set
	while (reg_a & RTC_UPDATE_IN_PROGRESS) {
		// select reg A with port io
		outb(IO_RTC_CMND, RTC_AREG);
		// read the register
		reg_a = inb(IO_RTC_DATA);
	}
}

static void read_once(uint8_t bcd_data_out[RTC_GETTIME_REG_NUM])
{
	uint8_t cmnd[RTC_GETTIME_REG_NUM] = {
		RTC_SEC,
		RTC_MIN,
		RTC_HOUR,
		RTC_DAY,
		RTC_MON,
		RTC_YEAR,
	};

	int32_t i = 0;
	for (i = 0; i < RTC_GETTIME_REG_NUM; i++) {
		// send command to micro that loads certain register into data line
		outb(IO_RTC_CMND, cmnd[i]);
		// read the data line
		bcd_data_out[i] = inb(IO_RTC_DATA);
	}
}

// See https://wiki.osdev.org/RTC: 0x70 is used to set address for CMOS RAM (address is 0x0A, 0x0B, 0x0C as only 3 regs are availiable)
// 0x71 is used to send data to that address.
int gettime(void)
{
	nmi_disable();

	// LAB 12: your code here
	int32_t tries = 0;
	uint8_t bcd_data_one[RTC_GETTIME_REG_NUM];
	while (tries++ < 3) {
		uint8_t bcd_data_two[RTC_GETTIME_REG_NUM];
		wait_while_updating();
		read_once(bcd_data_two);
		if (memcmp(bcd_data_one, bcd_data_two, RTC_GETTIME_REG_NUM) == 0) {
			// we read twice successfully
			break;
		}
		memmove(bcd_data_one, bcd_data_two, RTC_GETTIME_REG_NUM);
	}

	nmi_enable();

	if (tries >= 3) {
		panic("failed to read clock.");
	}

	struct tm time = {
		BCD2BIN(bcd_data_one[0]),
		BCD2BIN(bcd_data_one[1]),
		BCD2BIN(bcd_data_one[2]),
		BCD2BIN(bcd_data_one[3]),
		// the spec for RTC says months are 1-12, must convert to 0-11
		BCD2BIN(bcd_data_one[4]) - 1,
		// the spec for RTC says year is 0-99, must convert to 100-199, because we are past 2000
		BCD2BIN(bcd_data_one[5]) + 100,
	};

	int32_t unix_time = timestamp(&time);

	// struct tm check = {};
	// mktime(unix_time, &check);

	return unix_time;
}

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
	// force 24 hours instead of AM/PM
	reg_b |= RTC_24;
	// force binary-coded-decimal instead of binary
	reg_b &= ~RTC_DM;
	// write data with port io to selected reg
	outb(IO_RTC_DATA, reg_b);

	// select reg A with port io
	outb(IO_RTC_CMND, RTC_AREG);
	uint8_t reg_a = inb(IO_RTC_DATA);
	// set oscilator frequency the low 4 bits of A register
	reg_a |= 0x0F;
	outb(IO_RTC_DATA, reg_a);

	nmi_enable();

	vsys[VSYS_gettime] = gettime();
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

unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC_CMND, reg);
	return inb(IO_RTC_DATA);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC_CMND, reg);
	outb(IO_RTC_DATA, datum);
}

