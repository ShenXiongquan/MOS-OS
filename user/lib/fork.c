#include <env.h>
#include <lib.h>
#include <mmu.h>

/* Overview:
 *   Map the faulting page to a private writable copy.
 *
 * Pre-Condition:
 * 	'va' is the address which led to the TLB Mod exception.
 *
 * Post-Condition:
 *  - Launch a 'user_panic' if 'va' is not a copy-on-write page.
 *  - Otherwise, this handler should map a private writable copy of
 *    the faulting page at the same address.
 */

 //实现写时复制的核心算法函数
 //cow_entry 函数是写时复制处理的函数，也是内核会从 do_tlb_mod返回到的函数，负责对带有PTE_COW 标志的页面进行处理
static void __attribute__((noreturn)) cow_entry(struct Trapframe *tf) {
	u_int va = tf->cp0_badvaddr;
	u_int perm;

	/* Step 1: Find the 'perm' in which the faulting address 'va' is mapped. */
	/* Hint: Use 'vpt' and 'VPN' to find the page table entry. If the 'perm' doesn't have
	 * 'PTE_COW', launch a 'user_panic'. */
	/* Exercise 4.13: Your code here. (1/6) */
	perm = PTE_FLAGS(vpt[VPN(va)]);
	if(!(perm&PTE_COW)){
		user_panic("perm doesn't have PTE_COW");
	}
	/* Step 2: Remove 'PTE_COW' from the 'perm', and add 'PTE_D' to it. */
	/* Exercise 4.13: Your code here. (2/6) */
	perm=(perm&~PTE_COW)|PTE_D;

	/* Step 3: Allocate a new page at 'UCOW'. */
	/* Exercise 4.13: Your code here. (3/6) */
	syscall_mem_alloc(0, (void *)UCOW, perm);
	/* Step 4: Copy the content of the faulting page at 'va' to 'UCOW'. */
	/* Hint: 'va' may not be aligned to a page! */
	/* Exercise 4.13: Your code here. (4/6) */
	memcpy((void*)UCOW,(void*)ROUNDDOWN(va,PAGE_SIZE),PAGE_SIZE);
	// Step 5: Map the page at 'UCOW' to 'va' with the new 'perm'.
	/* Exercise 4.13: Your code here. (5/6) */
	syscall_mem_map(0,UCOW,0,va,perm);
	// Step 6: Unmap the page at 'UCOW'.
	/* Exercise 4.13: Your code here. (6/6) */
	syscall_mem_unmap(0,UCOW);

	// Step 7: Return to the faulting routine.
	int r = syscall_set_trapframe(0, tf);//目前已经处于用户态，通过系统调用恢复上下文进行返回。
	user_panic("syscall_set_trapframe returned %d", r);
}

/* Overview:
 *   Grant our child 'envid' access to the virtual page 'vpn' (with address 'vpn' * 'PAGE_SIZE') in
 * our (current env's) address space. 'PTE_COW' should be used to isolate the modifications on
 * unshared memory from a parent and its children.
 *
 * Post-Condition:
 *   If the virtual page 'vpn' has 'PTE_D' and doesn't has 'PTE_LIBRARY', both our original virtual
 *   page and 'envid''s newly-mapped virtual page should be marked 'PTE_COW' and without 'PTE_D',
 *   while the other permission bits are kept.
 *
 *   If not, the newly-mapped virtual page in 'envid' should have the exact same permission as our
 *   original virtual page.
 *
 * Hint:
 *   - 'PTE_LIBRARY' indicates that the page should be shared among a parent and its children.
 *   - A page with 'PTE_LIBRARY' may have 'PTE_D' at the same time, you should handle it correctly.
 *   - You can pass '0' as an 'envid' in arguments of 'syscall_*' to indicate current env because
 *     kernel 'envid2env' converts '0' to 'curenv').
 *   - You should use 'syscall_mem_map', the user space wrapper around 'msyscall' to invoke
 *     'sys_mem_map' in kernel.
 */
 //将当前进程的一个虚拟页面（由参数 vpn 指定）复制给另一个进程（由参数 envid 指定）。它根据虚拟页面的权限和共享状态来确定如何进行映射。
 //duppage 函数是父进程对子进程页面空间进行映射以及相关标志设置的函数
static void duppage(u_int envid, u_int vpn) {
	int r;
	u_int addr;
	u_int perm;

	/* Step 1: Get the permission of the page. */
	/* Hint: Use 'vpt' to find the page table entry. */
	/* Exercise 4.10: Your code here. (1/2) */
	addr=vpn*PAGE_SIZE;
	perm=PTE_FLAGS(vpt[vpn]);
	/* Step 2: If the page is writable, and not shared with children, and not marked as COW yet,
	 * then map it as copy-on-write, both in the parent (0) and the child (envid). */
	/* Hint: The page should be first mapped to the child before remapped in the parent. (Why?)
	 */
	/* Exercise 4.10: Your code here. (2/2) */
	r=0;
	if((perm&PTE_D)&&!(perm&PTE_LIBRARY)){
		perm=(perm&~PTE_D)|PTE_COW;//PTE_D代表可写权限位
		r=1;
	}
	syscall_mem_map(0, addr, envid, addr, perm);//先将子进程映射到父进程映射的物理页实现共享
	if(r)
	syscall_mem_map(0,addr,0,addr,perm);//再用新的映射覆盖自己旧的映射改变权限位

}

/* Overview:
 *   User-level 'fork'. Create a child and then copy our address space.
 *   Set up ours and its TLB Mod user exception entry to 'cow_entry'.
 *
 * Post-Conditon:
 *   Child's 'env' is properly set.
 *
 * Hint:
 *   Use global symbols 'env', 'vpt' and 'vpd'.
 *   Use 'syscall_set_tlb_mod_entry', 'syscall_getenvid', 'syscall_exofork',  and 'duppage'.
 */
 //理清这个函数，就能理解fork的调用过程 
int fork(void) {
	u_int child;
	u_int i;

	/* Step 1: Set our TLB Mod user exception entry to 'cow_entry' if not done yet. */
	if (env->env_user_tlb_mod_entry != (u_int)cow_entry) {
		try(syscall_set_tlb_mod_entry(0, cow_entry));
	}

	/* Step 2: Create a child env that's not ready to be scheduled. */
	// Hint: 'env' should always point to the current env itself, so we should fix it to the
	// correct value.
	child = syscall_exofork();
	if (child == 0) {
		env = envs + ENVX(syscall_getenvid());//env控制块原本指向父进程，现在需要指向子进程
		return 0;
	}//当前进程为子进程而言的操作

	/* Step 3: Map all mapped pages below 'USTACKTOP' into the child's address space. */
	// Hint: You should use 'duppage'.
	/* Exercise 4.15: Your code here. (1/2) */
	for (i = 0; i < VPN(USTACKTOP); i++) {
		if ((vpd[i >> 10] & PTE_V) && (vpt[i] & PTE_V)) {
			duppage(child, i);
		}
	}
	/* Step 4: Set up the child's tlb mod handler and set child's 'env_status' to
	 * 'ENV_RUNNABLE'. */
	/* Hint:
	 *   You may use 'syscall_set_tlb_mod_entry' and 'syscall_set_env_status'
	 *   Child's TLB Mod user exception entry should handle COW, so set it to 'cow_entry'
	 */
	/* Exercise 4.15: Your code here. (2/2) */
	try(syscall_set_tlb_mod_entry(child,cow_entry));
	try(syscall_set_env_status(child,ENV_RUNNABLE));
	return child;//对于父进程而言，返回子进程的pid
}

// int make_shared(void *va) {
//     u_int perm = PTE_D | PTE_V; // 设定页面权限为可写和有效

//     // 检查地址有效性，确保地址是正常映射的
//     if (!((vpd[va >> 22] & PTE_V) || !(vpt[va >> 12] & PTE_V))) {
//         return -1; // 如果地址未映射或映射无效，返回-1
//     }

//     // 请求分配内存页
//     if (syscall_mem_alloc(0, ROUNDDOWN(va, BY2PG), perm) != 0) {
//         return -1; // 如果内存分配失败，返回-1
//     }

//     perm = vpt[VPN(va)] & 0xfff; // 获取当前页面的权限
//     // 检查是否是用户空间地址且页面权限正确
//     if (va >= (void *)UTOP || ((vpd[va >> 22] & PTE_V) && (vpt[va >> 12] & PTE_V) && !(perm & PTE_D))) {
//         return -1; // 如果不是用户空间地址或页面权限不正确，返回-1
//     }

//     perm |= PTE_LIBRARY; // 设置页面权限为库权限
//     u_int addr = VPN(va) * BY2PG; // 获取地址
//     // 映射内存
//     if (syscall_mem_map(0, (void *)addr, 0, (void *)addr, perm) != 0) {
//         return -1; // 如果内存映射失败，返回-1
//     }

//     return ROUNDDOWN(vpt[VPN(va)] & ~0xfff, BY2PG); // 返回页面地址，页面对齐处理
// }