#include <hermit/syscall.h>
#include <asm/uhyve.h>
#include <asm/page.h>
#include <hermit/spinlock.h>
#include <hermit/logging.h>
#include <hermit/minifs.h>

	
typedef struct {
	char *path;
	off_t length;
	int ret;
} __attribute__((packed)) uhyve_truncate_t; // faut garder packed...


int sys_truncate(const char *path, off_t length)
{

	uhyve_truncate_t arg;

	if(minifs_enabled)
		return minifs_creat(path, length); // TODO change _creat

	arg.path = (char *)virt_to_phys((size_t)path);
	arg.length = length;
	arg.ret = -1;

	uhyve_send(UHYVE_PORT_TRUNCATE, (unsigned)virt_to_phys((size_t)&arg));

	return arg.ret;

}
