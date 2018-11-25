/* frame.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
//#include <kconfig.h>
#include <paging.h>

extern int page_replace_policy;
/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */

int minOf(int a, int b)
{
	if(a>b)
		return b;
	else
		return a;
} 

SYSCALL init_frm()
{
	STATWORD ps;
	disable(ps);
	int i = 0;
	for(i=0; i < NFRAMES; i++)
	{
  		frm_tab[i].fr_status = FRM_UNMAPPED;
  		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_refcnt = frm_tab[i].fr_dirty = frm_tab[i].fr_vpno = 0;

		//INIT queue too
		replacementQueue[i].frameNo = i;
		replacementQueue[i].age = 0;
		replacementQueue[i].prevFrame = 0;
		replacementQueue[i].accessed = 0;
		replacementQueue[i].nextFrame = -1;


  	}
 	restore(ps);
  	return OK;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int* avail)
{
  	STATWORD ps;
  	disable(ps);
  	int i = 0;
  	int newFrame = 0;
  	int flag = 0 ;
  	for(i=0; i < NFRAMES; i++)
	{
  		if(frm_tab[i].fr_status == FRM_UNMAPPED)
  		{
  			*avail = i;
  			flag = 1;
  			restore(ps);
  			return OK;
  		}
  	}
  	if(page_replace_policy == SC || page_replace_policy == AGING)
  	{
	  	newFrame = -1;
	  	replacePage(&newFrame);

		free_frm(newFrame);
		*avail = newFrame;
		restore(ps);
		return OK;
	}
  	
	restore(ps);
	return SYSERR;

}

/*-------------------------------------------------------------------------
 * free_frm - free a frame 
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int frame)
{
	STATWORD ps;
  	disable(ps);
	
	virt_addr_t * vaddr;
  	pd_t* rootDirectory;
  	pt_t* rootPageTable;
  	int accessed = -1;
  	unsigned int pdNumber,ptNumber;
  	unsigned int victimFrame = headerQueue;
  	int curr = 0;
  	int prev, next ;
  	prev = next= -1;
  	int flag = -1;
  	int least = curr ;

	vaddr = (virt_addr_t *)&(frm_tab[frame].fr_vpno);
	rootDirectory = (pd_t*) proctab[frm_tab[frame].fr_pid].pdbr;
	pdNumber =  vaddr->pd_offset;
	ptNumber =  vaddr->pt_offset;
	//set PD root and table root
	pd_t* tempDir = (pd_t*)&rootDirectory[pdNumber];
	rootPageTable = (pt_t*)( (tempDir->pd_base * NBPG));

	//get the page number and backing store for the i-th frame
	int bs = proctab[frm_tab[frame].fr_pid].store;
	int pageth = frm_tab[frame].fr_vpno - proctab[frm_tab[frame].fr_pid].vhpno;

	write_bs((frame+FRAME0)*NBPG, bs, pageth);

	if(frm_tab[tempDir->pd_base - FRAME0].fr_pid == currpid)
	{

		//invalidate tlb 
		//invlpg((unsigned long)frm_tab[tempDir->pd_base-FRAME0].fr_vpno);
	}
	rootPageTable[ptNumber].pt_pres = 0;
	frm_tab[tempDir->pd_base - FRAME0].fr_refcnt -= 1;
	if(frm_tab[tempDir->pd_base - FRAME0].fr_refcnt == 0 )
	{
		frm_tab[tempDir->pd_base - FRAME0].fr_status = FRM_UNMAPPED;
		frm_tab[tempDir->pd_base - FRAME0].fr_pid = -1;
		frm_tab[tempDir->pd_base-FRAME0].fr_type = FR_PAGE;
		frm_tab[tempDir->pd_base-FRAME0].fr_vpno = HBASE;
	}

	replacementQueue[frame].frameNo = frame;
	replacementQueue[frame].age = 0;
	//replacementQueue[i].prevFrame = 0;
	//replacementQueue[i].accessed = 0;
	//replacementQueue[i].nextFrame = -1;
	restore(ps);
	return OK;
}



void replacePage(int* newFrame)
{
	int i = 0;
	virt_addr_t * vaddr;
  	pd_t* rootDirectory;
  	pt_t* rootPageTable;
  	int accessed = -1;
  	unsigned int pdNumber,ptNumber;
  	unsigned int victimFrame = headerQueue;
  	int curr = 0;
  	int prev, next;
  	prev = next = -1;
  	int flag = -1;
  	int least = curr ;
	//if(page_replace_policy == 3)
  	{
  		for( i =0; i< NFRAMES ; i++)
  		{
  			if(frm_tab[curr].fr_type == FR_PAGE)
  			{
	  			vaddr = (virt_addr_t *)&(frm_tab[curr].fr_vpno);
	  			rootDirectory = (pd_t*) proctab[frm_tab[curr].fr_pid].pdbr;
	  			pdNumber =  vaddr->pd_offset;
	  			ptNumber =  vaddr->pt_offset;
	  			//set PD root and table root
	  			pd_t* tempDir = (pd_t*)&rootDirectory[pdNumber];
	  			rootPageTable = (pt_t*)( (tempDir->pd_base * NBPG));
	  			int acc = rootPageTable[ptNumber].pt_acc;
	  			int age = replacementQueue[curr].age;
	  			int maxAge = 255;
	  			int addFactor = 128;
	  			if(page_replace_policy == SC)
	  			{
		  			if(acc == 0)
		  			{
		  				if(prev < 0 || headerQueue < 0)
		  					headerQueue = replacementQueue[0].nextFrame;
		  				else
		  					replacementQueue[prev].nextFrame = replacementQueue[curr].nextFrame; 

		  				//set the nect of current page to -1
		  				replacementQueue[curr].nextFrame = -1;
		  				flag = curr;
		  				break;
		  			}
		  			else
		  				rootPageTable[ptNumber].pt_acc = 0;
		  		}
		  		else if(page_replace_policy == AGING)
		  		{
		  			replacementQueue[curr].age = age>>1;

		  			if(acc == 1)
		  				replacementQueue[curr].age = minOf(replacementQueue[curr].age+addFactor , maxAge);

		  			if(replacementQueue[least].age < replacementQueue[curr].age)
		  				least = curr;

		  			flag = least;
		  		}
	  		}
	  		prev = curr;
			curr = replacementQueue[curr].nextFrame;		
  		}

  		*newFrame = flag;
  		return;

  	}

} 

void updateQueue(int newFrame)
{
	STATWORD ps;
	disable(ps);
	if(headerQueue == -1){ 
		headerQueue = newFrame;
		restore(ps);
		return OK;
	}
	int curr = headerQueue;
	int last = -1;
	while(replacementQueue[curr].nextFrame != -1)
	{
		last = curr;
		curr = replacementQueue[headerQueue].nextFrame;
	}
	if(last != -1)
		curr = last;
	replacementQueue[curr].nextFrame = newFrame;
	replacementQueue[newFrame].nextFrame = -1;
}