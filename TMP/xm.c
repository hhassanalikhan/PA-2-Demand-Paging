/* xm.c = xmmap xmunmap */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>


/*-------------------------------------------------------------------------
 * xmmap - xmmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap(int virtpage, bsd_t source, int npages)
{
    STATWORD ps;
    disable(ps);
    kprintf("\n\n\n XMMAP CAlled\n\n\n");
    if (  npages < 0 ||  npages >= MAX_BS_PAGES || source < 0 || source > BSCOUNT-1)
    {
      kprintf("\nxmmap error : WRONG SOURCE\n");
      return SYSERR;
    }

    bsm_map(currpid, virtpage, source, npages);
    restore(ps);
    return (OK);
}



/*-------------------------------------------------------------------------
 * xmunmap - xmunmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage )
{
  STATWORD ps;
  disable(ps);
  if(virtpage < HBASE){ 
    kprintf("\nxmummap error : LESS THAN hbase\n");
    return SYSERR;
  }
  bsm_unmap(currpid, virtpage);
  restore(ps);
  return OK;
}

