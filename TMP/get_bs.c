#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

int get_bs(bsd_t bs_id, unsigned int npages) {

  /* requests a new mapping of npages with ID map_id */

    STATWORD ps;
    disable(ps);
    if( bsm_tab[bs_id].bs_status == BSM_MAPPED && bsm_tab[bs_id].private == 1)
    {
        kprintf("Err");
        return SYSERR;
    }
    else if( npages <= 0 || npages > MAX_BS_PAGES)
    {
        kprintf("Err");
        return SYSERR;
    }
    else if( bs_id > BSCOUNT-1 || bs_id < 0 )
    {
        kprintf("Err");
        return SYSERR;
    }
    //bsm_tab[bs_id].bs_npages = npages;
    //bsm_tab[bs_id].bs_pid = currpid;
    restore(ps);
    return npages;

}