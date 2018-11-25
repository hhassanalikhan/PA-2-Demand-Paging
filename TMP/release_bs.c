#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

SYSCALL release_bs(bsd_t bs_id) {


    STATWORD ps;
	disable(ps);
	int ind = 0;
	if(bsm_tab[bs_id].private != 1)
	{
		for ( ind = 0; ind < NPROC; ind++)
		{
			if(bsm_tab[bs_id].mappedToProc[ind] == currpid)
			{
				break;
			}
		}
		if (ind != NPROC)
		{
			bsm_tab[bs_id].mappedToProc[ind] = -1;
			bsm_tab[bs_id].proc_vpnos[ind] = HBASE ;
			bsm_tab[bs_id].mappedProcNpages[ind] = 0 ;
		}

		for ( ind = 0; ind < NPROC; ind++)
		{
			if(bsm_tab[bs_id].mappedToProc[ind] != -1)
			{
				break;
			}
		}
		if(ind == NPROC)
		{
			bsm_tab[bs_id].bs_status = BSM_UNMAPPED;
			bsm_tab[bs_id].bs_pid = -1;
		  	bsm_tab[bs_id].bs_npages = 0;
			bsm_tab[bs_id].private = 0;
			bsm_tab[bs_id].bs_vpno = HBASE;
			bsm_tab[bs_id].bs_sem = 0;
		}
	}
	else
	{
		bsm_tab[bs_id].bs_status = BSM_UNMAPPED;
		bsm_tab[bs_id].bs_pid = -1;
	  	bsm_tab[bs_id].bs_npages = 0;
		bsm_tab[bs_id].private = 0;
		bsm_tab[bs_id].bs_vpno = HBASE;
		bsm_tab[bs_id].bs_sem = 0;
	}
	restore(ps);
   	return OK;
 //   kprintf("To be implemented!\n");
 //  return OK;

}

