// User-level IPC library routines

#include <env.h>
#include <lib.h>
#include <mmu.h>

// Send val to whom.  This function keeps trying until
// it succeeds.  It should panic() on any error other than
// -E_IPC_NOT_RECV.
//
// Hint: use syscall_yield() to be CPU-friendly.
void ipc_send(u_int whom, u_int val, const void *srcva, u_int perm) {
	int r;
	while ((r = syscall_ipc_try_send(whom, val, srcva, perm)) == -E_IPC_NOT_RECV) {
		syscall_yield();
	}
	user_assert(r == 0);
}

// Receive a value.  Return the value and store the caller's envid
// in *whom.
//
// Hint: use env to discover the value and who sent it.
u_int ipc_recv(u_int *whom, void *dstva, u_int *perm) {
	int r = syscall_ipc_recv(dstva);
	if (r != 0) {
		user_panic("syscall_ipc_recv err: %d", r);
	}

	if (whom) {
		*whom = env->env_ipc_from;
	}

	if (perm) {
		*perm = env->env_ipc_perm;
	}

	return env->env_ipc_value;
}

// kern/syscall_all.c
// extern struct Env envs[NENV]; //注意 extern！！
// int sys_ipc_try_broadcast(u_int value, u_int srcva, u_int perm) {
// 	struct Env *e;
// 	struct Page *p;

// 	/* Step 1: Check if 'srcva' is either zero or a legal address. */
// 	/* 抄的sys_ipc_try_send */
// 	if (srcva != 0 && is_illegal_va(srcva)) {
// 		return -E_IPC_NOT_RECV;
// 	}
    
// 	/* 函数核心：遍历envs找后代进程 */
// 	int signal[NENV];
// 	for (u_int i = 0; i < NENV; i++) {
// 		if (curenv->env_id == envs[i].env_parent_id) {
// 			signal[i] = 1;
// 		} else {
// 			signal[i] = 0;
// 		}
// 	}
// 	int flag = 0;
// 	while(flag == 0) {
// 		flag = 1;
// 		for (u_int i = 0; i < NENV; i++) {
// 			if (signal[i] == 1) {
//     				for (u_int j = 0; j < NENV; j++) {
// 					if (signal[j] == 0 && envs[i].env_id == envs[j].env_parent_id) {
// 						signal[j] = 1;
// 						flag = 0;
// 					}
// 				}
// 			}
// 		}
// 	}
	
// 	/* Step 3: Check if the target is waiting for a message. */
// 	/* 基于sys_ipc_try_send修改 */
// 	for (u_int i = 0; i < NENV; i++) {
// 		if(signal[i] == 1) {
// 			e = &(envs[i]);
//             /* 以下都是抄的sys_ipc_try_send */
//             if (e->env_ipc_recving == 0) {
// 				return -E_IPC_NOT_RECV;
// 			}
// 			e->env_ipc_value = value;
// 			e->env_ipc_from = curenv->env_id;
// 			e->env_ipc_perm = PTE_V | perm;
// 			e->env_ipc_recving = 0;
// 			e->env_status = ENV_RUNNABLE;
// 			TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
// 			if (srcva != 0) {
// 				p = page_lookup(curenv->env_pgdir, srcva, NULL);
// 				if(p == NULL) return -E_INVAL;
// 				if (page_insert(e->env_pgdir, e->env_asid, p, e->env_ipc_dstva, perm) != 0) { 
// 		            return -E_INVAL;
//  		       }
// 			}
// 		}
// 	}
// 	return 0;
// }

