/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL init_bsm()
{
	kprintf("\n\n\n------------------ INIT BS CAlled------------\n\n\n");
	STATWORD ps;
	disable(ps);

    int i = 0;
	for(i = 0; i < BSCOUNT; i++)
	{
		bsm_tab[i].bs_status = BSM_UNMAPPED;
		bsm_tab[i].bs_pid = -1;
		bsm_tab[i].bs_npages = 0;
		bsm_tab[i].private = 0;
		bsm_tab[i].maxIndx = -1;
		bsm_tab[i].bs_sem = 0;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int* avail)
{
	STATWORD ps;
	disable(ps);
	int i = 0;
	for(i = 0; i < BSCOUNT; i++)
	{
		int flag = 0;
		if(proctab[currpid].vcreated == 1 && bsm_tab[i].bs_status == BSM_UNMAPPED)	
			flag = 1;
		else if(proctab[currpid].vcreated != 1)
			flag = 1;	
		if(flag)
		{
			*avail = i;
			restore(ps);
			return OK;	
		}
		
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i)
{
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth)
{
	STATWORD ps;
	disable(ps);
	int i = 0;

	//kprintf("\n---!!!///pfint called---------- %d\n", proctab[pid].vcreated);
	for (i = 0; i < BSCOUNT; i++)
	{

		if(proctab[pid].vcreated == 1)
		{
			if(bsm_tab[i].bs_pid == pid)
			{

				if(vaddr/NBPG >=  bsm_tab[i].bs_vpno)
				{
					*store = i;
					*pageth = (vaddr/NBPG) - bsm_tab[i].bs_vpno;
					restore(ps);
					return OK;
				}
				restore(ps);
				return SYSERR;
			}
		}
		else
		{
			int index = -1;
			int j =0;
			for ( j = 0; j < NPROC; j++)
			{
				if(bsm_tab[i].mappedToProc[j] == pid)
				{
					index = j;
					break;
				}
			}

			if(index > -1 )
			{
				if(vaddr/NBPG >=  bsm_tab[i].proc_vpnos[index])
				{
					*store = i;
					*pageth = (vaddr/NBPG) -  bsm_tab[i].proc_vpnos[index];
					restore(ps);
					return OK;
				}
				else
				{
					restore(ps);
					return SYSERR;
				}
			}
			
		}
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages)
{
	STATWORD ps;
	disable(ps);
	if(source < BSCOUNT && npages < MAX_BS_PAGES)
	{
		kprintf("\n\n\n bs map CAlled\n\n\n");

		if(proctab[currpid].vcreated == 1)
		{
			bsm_tab[source].bs_status = BSM_MAPPED;
			bsm_tab[source].bs_sem = 1;
			proctab[currpid].store = source;
			bsm_tab[source].private = 1;
			bsm_tab[source].bs_pid = pid;
			bsm_tab[source].bs_npages = npages;
			bsm_tab[source].bs_vpno = vpno;
			proctab[currpid].vhpno = vpno;
		}
		else
		{
			bsm_tab[source].private = 0;
			int i = 0;
			for ( i = 0; i < NPROC; ++i)
			{
				if(bsm_tab[source].mappedToProc[i] == -1)
				{
					break;
				}
			}
			if(i < NPROC)
			{
				int indx = i;
				bsm_tab[source].bs_status = BSM_MAPPED;
				bsm_tab[source].bs_sem = 1;
				proctab[currpid].store = source;
				bsm_tab[source].mappedToProc[indx] = pid;
				bsm_tab[source].mappedProcNpages[indx] = npages;
				bsm_tab[source].proc_vpnos[indx] = vpno;
			}
		}
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno)
{
	int i = 0, bs, pageth;
	unsigned long vaddr;
	vaddr = vpno*NBPG;

	for(i = 0; i < NFRAMES; i++)
	{
		if(frm_tab[i].fr_pid == pid && frm_tab[i].fr_type == FR_PAGE)
		{
			bsm_lookup(pid,vaddr,&bs,&pageth);
			write_bs( (i+NFRAMES)*NBPG, bs, pageth);
		}
	}
	if(proctab[currpid].vcreated == 1)
	{
		proctab[currpid].store = -1;
		bsm_tab[bs].bs_npages = 0;
		bsm_tab[bs].bs_pid = -1;
		bsm_tab[bs].bs_sem = 0;
		bsm_tab[bs].bs_status = BSM_UNMAPPED;
		bsm_tab[bs].private = 0;
		bsm_tab[bs].bs_vpno = HBASE;
	}
	else
	{
		proctab[currpid].store = -1;
		for (i = 0; i < NPROC; i++)
		{
			if(bsm_tab[bs].mappedToProc[i] == pid)
			{
				bsm_tab[bs].mappedToProc[i] = -1;
				bsm_tab[bs].mappedProcNpages[i] = -1;
				bsm_tab[bs].proc_vpnos[i] = HBASE;
				bsm_tab[bs].bs_npages = 0;
				bsm_tab[bs].bs_pid = -1;
				bsm_tab[bs].bs_sem = 0;
				bsm_tab[bs].private = 0;
				bsm_tab[bs].bs_vpno = HBASE;
			}

		}
		int flag = 0;
		for ( i = 0; i < NPROC; i++)
		{
			if(bsm_tab[bs].mappedToProc[i] == pid)
			{
				flag = 1;
			}
		}
		if(!flag)
			bsm_tab[bs].bs_status = BSM_UNMAPPED;
	}
	
}


