#include <hermit/syscall.h>
#include <asm/uhyve.h>
#include <asm/page.h>
#include <hermit/spinlock.h>
#include <hermit/logging.h>
#include <hermit/minifs.h>



typedef struct {
	int fd;
	off_t length;
	int ret;
} __attribute__((packed)) uhyve_ftruncate_t;



int sys_ftruncate(int fd, off_t length)
{
	uhyve_ftruncate_t arg;

	if(minifs_enabled)
		return minifs_creat(fd, length); // TODO change _creat

	arg.fd = 0 // (int *) virt_to_phys((size_t)path); TODO
	arg.length = length;
	arg.ret = -1;

	uhyve_send(UHYVE_PORT_TRUNCATE, (unsigned)virt_to_phys((size_t)&arg)); //TODO

	return arg.ret;

}
