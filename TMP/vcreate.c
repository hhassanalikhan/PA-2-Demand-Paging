/* vcreate.c - vcreate */
    
#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

/*
static unsigned long esp;
*/

LOCAL	newpid();
/*------------------------------------------------------------------------
 *  create  -  create a process to start running a procedure
 *------------------------------------------------------------------------
 */
SYSCALL vcreate(procaddr,ssize,hsize,priority,name,nargs,args)
	int	*procaddr;		/* procedure address		*/
	int	ssize;			/* stack size in words		*/
	int	hsize;			/* virtual heap size in pages	*/
	int	priority;		/* process priority > 0		*/
	char	*name;			/* name (for debugging)		*/
	int	nargs;			/* number of args that follow	*/
	long	args;			/* arguments (treated like an	*/
					/* array in the code)		*/
{
	STATWORD ps;
	disable(ps);
	int bs = -1;
	int newPid = -1;
	struct mblock *memBlock;
	newPid = create(procaddr,ssize,priority,name,nargs,args);
	 
	if(newPid > -1)
	{
			int retVal = get_bsm(&bs);
			if(retVal != SYSERR)
			{
				//map to bs and update proc struture with store and vhpno
				bsm_map(newPid,HBASE,bs,hsize);
				proctab[newPid].store = bs;
				proctab[newPid].vhpnpages = hsize;
				proctab[newPid].vhpno = HBASE;
				bsm_tab[bs].private = 1;

				//initialize free list 
				proctab[newPid].vmemlist->mnext = HBASE * NBPG;
				memBlock = BACKING_STORE_BASE + (bs * BACKING_STORE_UNIT_SIZE);
				memBlock->mlen = hsize * NBPG;
				memBlock->mnext = NULL;
				proctab[newPid].vcreated = 1;
				restore(ps);
				return newPid;
			}
			else
			{
				kill(newPid);
				restore(ps);
				return SYSERR;
			}

	}
	else
	{
			restore(ps);
			return SYSERR;
	}

}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL	newpid()
{
	int	pid;			/* process id to return		*/
	int	i;

	for (i=0 ; i<NPROC ; i++) {	/* check all NPROC slots	*/
		if ( (pid=nextproc--) <= 0)
			nextproc = NPROC-1;
		if (proctab[pid].pstate == PRFREE)
			return(pid);
	}
	return(SYSERR);
}
