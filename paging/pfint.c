/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

#define NOT_FOUND -1

extern int page_replace_policy;

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint()
{

	
	STATWORD ps;
  	disable(ps);
	int i = 0;
	virt_addr_t * vaddr;
  	pd_t* rootDirectory;
  	pt_t* rootPageTable;
  	int accessed = -1;
  	unsigned int pdNumber,ptNumber;
  	unsigned int jj = read_cr2();

  	unsigned int victimFrame = headerQueue;
  	int curr = 0;
  	int prev = -1, next = -1;
  	int flag = -1;
  	int least = curr ;
  	int newFrame = -1;
	vaddr = (virt_addr_t* )&jj;
	
	rootDirectory = (pd_t*) proctab[currpid].pdbr;
	
	pdNumber =  vaddr->pd_offset;

	ptNumber =  vaddr->pt_offset;
	
	//set PD root and table root
	pd_t* tempDir = (pd_t*)&(rootDirectory[pdNumber]);
	rootPageTable = (pt_t*)(tempDir->pd_base * NBPG);

	if(rootDirectory[pdNumber].pd_pres == 0)
	{
		unsigned int newFrame  = -1;

		if(get_frm(&newFrame) == SYSERR)
  		{
  			kill(currpid);
			restore(ps);
  			return SYSERR;
  		}
  		int temp = newFrame;
  		newFrame = (FRAME0 + newFrame)*NBPG;
  		pt_t* tableAddr = (pt_t*)newFrame;
		//creating new table
		for(i = 0;i < NFRAMES; i++)
		{
			tableAddr[i].pt_pres = 0;
			tableAddr[i].pt_write = 0;
			tableAddr[i].pt_user = 0;
			tableAddr[i].pt_pwt = 0;
			tableAddr[i].pt_pcd = 0;
			tableAddr[i].pt_acc = 0;
			tableAddr[i].pt_dirty = 0;
			tableAddr[i].pt_mbz = 0;
			tableAddr[i].pt_global = 0;
			tableAddr[i].pt_avail = 0;
			tableAddr[i].pt_base = 0;
		}

		//create the directory entry 
		rootDirectory[pdNumber].pd_pres = 1;
		rootDirectory[pdNumber].pd_write = 1;
		rootDirectory[pdNumber].pd_user = 0;
		rootDirectory[pdNumber].pd_pwt = 0;
		rootDirectory[pdNumber].pd_pcd = 0;
		rootDirectory[pdNumber].pd_acc = 0;
		rootDirectory[pdNumber].pd_mbz = 0;
		rootDirectory[pdNumber].pd_fmb = 0;
		rootDirectory[pdNumber].pd_global = 0;
		rootDirectory[pdNumber].pd_avail = 0;
		rootDirectory[pdNumber].pd_base = temp + FRAME0;

		//set the table frame
		frm_tab[temp].fr_type = FR_TBL;
		frm_tab[temp].fr_status = FRM_MAPPED;
		frm_tab[temp].fr_pid = currpid;
	}

	rootPageTable = (pt_t*)(tempDir->pd_base * NBPG);
	if(rootPageTable[ptNumber].pt_pres == 0)
	{

		unsigned int newFrame;
		if(get_frm(&newFrame) == SYSERR)
  		{
  			kill(currpid);
			restore(ps);
  			return SYSERR;
  		}
  		
  		//set the new frame for data 
  		frm_tab[newFrame].fr_status = FRM_MAPPED;
		frm_tab[tempDir->pd_base - FRAME0].fr_refcnt++;
		frm_tab[newFrame].fr_pid = currpid;
		frm_tab[newFrame].fr_type = FR_PAGE;
		frm_tab[newFrame].fr_vpno = jj/NBPG;
		frm_tab[newFrame].fr_dirty = 0;

		//set the page table entry
		
		rootPageTable[ptNumber].pt_write = 1;
		rootPageTable[ptNumber].pt_base = (FRAME0 + newFrame);

		updateQueue(newFrame);
		//kprintf("\n---!!!///pfint called-----adas----- %d\n", vaddr->pd_offset);
		//get the evicted page's mapping from store 
		int bs,pageth;

		bsm_lookup(currpid,vaddr,&bs,&pageth);
		read_bs((char*)((FRAME0+newFrame)*NBPG),bs,pageth);
		rootPageTable[ptNumber].pt_pres = 1;
		write_cr3(proctab[currpid].pdbr);
		restore(ps);
  		return OK;
	}
}