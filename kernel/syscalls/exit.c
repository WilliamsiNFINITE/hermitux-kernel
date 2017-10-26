#include <hermit/syscall.h>
#include <asm/uhyve.h>
#include <asm/page.h>
#include <hermit/spinlock.h>

extern spinlock_irqsave_t lwip_lock;
extern volatile int libc_sd;

void NORETURN do_exit(int arg);

typedef struct {
	int sysnr;
	int arg;
} __attribute__((packed)) sys_exit_t;

/** @brief To be called by the systemcall to exit tasks */
void NORETURN sys_exit(int arg)
{
	if (is_uhyve()) {
		uhyve_send(UHYVE_PORT_EXIT, (unsigned) virt_to_phys((size_t) &arg));
	} else {
		sys_exit_t sysargs = {__NR_exit, arg};

		spinlock_irqsave_lock(&lwip_lock);
		if (libc_sd >= 0)
		{
			int s = libc_sd;

			lwip_write(s, &sysargs, sizeof(sysargs));
			libc_sd = -1;

			spinlock_irqsave_unlock(&lwip_lock);

			// switch to LwIP thread
			reschedule();

			lwip_close(s);
		} else {
			spinlock_irqsave_unlock(&lwip_lock);
		}
	}

	do_exit(arg);
}


