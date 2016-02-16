/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */
#define CONFIGURE_INIT
#include "system.h"
#include <rtems/untar.h>

#include "engine.h"
#include "systemstub.h"


extern int _binary_rootfs_tar_start;
extern int _binary_rootfs_tar_size;
#define ROOTFS_START _binary_rootfs_tar_start
#define ROOTFS_SIZE _binary_rootfs_tar_size

rtems_task main_task(
  rtems_task_argument 
)
{
	const char *dataPath = "data";
	const char *savePath = "save";

	if(Untar_FromMemory((char *)(&ROOTFS_START), (unsigned long)&ROOTFS_SIZE)!=0)
		printf("Error Untar_FromMemory\n");

	SystemStub *stub = SystemStub_GP32_create();
	Engine *e = new Engine(stub, dataPath, savePath);
	e->run();
	printf("e->run()..ok\n");
	delete e;
	delete stub;
	exit(0);
}
