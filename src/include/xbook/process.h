#ifndef _XBOOK_PROCESS_H
#define _XBOOK_PROCESS_H

#include "task.h"
#include "elf32.h"
#include "rawblock.h"
#include <sys/usrmsg.h>

task_t *process_create(char *name, char **argv);
int proc_vmm_init(task_t *task);
int proc_vmm_exit(task_t *task);

int proc_stack_init(task_t *task, trap_frame_t *frame, 
        int argc, char **argv);
void proc_heap_init(task_t *task);
int proc_load_image(vmm_t *vmm, struct Elf32_Ehdr *elf_header, raw_block_t *rb);
int proc_fork(long *retval);
void proc_exit(int status);
pid_t proc_wait(int *status);
int proc_exec(char *name, char **argv);
#endif /* _XBOOK_PROCESS_H */
