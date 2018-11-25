/* initialize.c - nulluser, sizmem, sysinit */

#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <sleep.h>
#include <mem.h>
#include <tty.h>
#include <q.h>
#include <io.h>
#include <paging.h>

/*#define DETAIL */
#define HOLESIZE	(600)	
#define	HOLESTART	(640 * 1024)
#define	HOLEEND		((1024 + HOLESIZE) * 1024)  
/* Extra 600 for bootp loading, and monitor */

void AddGlobalTable(int);
extern	int	main();	/* address of user's main prog	*/
extern	int	start();
extern void page_directory_allocation(int);



LOCAL		sysinit();

/* Declarations of major kernel variables */
struct	pentry	proctab[NPROC]; /* process table			*/
int	nextproc;		/* next process slot to use in create	*/
struct	sentry	semaph[NSEM];	/* semaphore table			*/
int	nextsem;		/* next sempahore slot to use in screate*/
struct	qent	q[NQENT];	/* q table (see queue.c)		*/
int	nextqueue;		/* next slot in q structure to use	*/
char	*maxaddr;		/* max memory address (set by sizmem)	*/
struct	mblock	memlist;	/* list of free memory blocks		*/
#ifdef	Ntty
struct  tty     tty[Ntty];	/* SLU buffers and mode control		*/
#endif


replacementItem replacementQueue[NFRAMES];
bs_map_t bsm_tab[BSCOUNT];
fr_map_t frm_tab[NFRAMES];
 int headerQueue = -1;	
int page_replace_policy = SC;

/* active system status */
int	numproc;		/* number of live user processes	*/
int	currpid;		/* id of currently running process	*/
int	reboot = 0;		/* non-zero after first boot		*/

int	rdyhead,rdytail;	/* head/tail of ready list (q indicies)	*/
char 	vers[80];
int	console_dev;		/* the console device			*/



/************************************************************************/
/***				NOTE:				      ***/
/***								      ***/
/***   This is where the system begins after the C environment has    ***/
/***   been established.  Interrupts are initially DISABLED, and      ***/
/***   must eventually be enabled explicitly.  This routine turns     ***/
/***   itself into the null process after initialization.  Because    ***/
/***   the null process must always remain ready to run, it cannot    ***/
/***   execute code that might cause it to be suspended, wait for a   ***/
/***   semaphore, or put to sleep, or exit.  In particular, it must   ***/
/***   not do I/O unless it uses kprintf for polled output.           ***/
/***								      ***/
/************************************************************************/

/*------------------------------------------------------------------------
 *  nulluser  -- initialize system and become the null process (id==0)
 *------------------------------------------------------------------------
 */
nulluser()				/* babysit CPU when no one is home */
{
        int userpid;

	console_dev = SERIAL0;		/* set console to COM0 */

	initevec();

	kprintf("system running up!\n");
	sysinit();

	enable();		/* enable interrupts */

	sprintf(vers, "PC Xinu %s", VERSION);
	kprintf("\n\n%s\n", vers);
	if (reboot++ < 1)
		kprintf("\n");
	else
		kprintf("   (reboot %d)\n", reboot);


	kprintf("%d bytes real mem\n",
		(unsigned long) maxaddr+1);
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) 0);
	kprintf(" to %d\n", (unsigned long) (maxaddr) );
#endif	

	kprintf("%d bytes Xinu code\n",
		(unsigned long) ((unsigned long) &end - (unsigned long) start));
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) start);
	kprintf(" to %d\n", (unsigned long) &end );
#endif

#ifdef DETAIL	
	kprintf("%d bytes user stack/heap space\n",
		(unsigned long) ((unsigned long) maxaddr - (unsigned long) &end));
	kprintf("    %d", (unsigned long) &end);
	kprintf(" to %d\n", (unsigned long) maxaddr);
#endif	
	
	kprintf("clock %sabled\n", clkruns == 1?"en":"dis");


	/* create a process to execute the user's main program */
	userpid = create(main,INITSTK,INITPRIO,INITNAME,INITARGS);
	resume(userpid);

	while (TRUE)
		/* empty */;
}

/*------------------------------------------------------------------------
 *  sysinit  --  initialize all Xinu data structeres and devices
 *------------------------------------------------------------------------
 */
LOCAL
sysinit()
{
	static	long	currsp;
	int	i,j;
	struct	pentry	*pptr;
	struct	sentry	*sptr;
	struct	mblock	*mptr;


	numproc = 0;			/* initialize system variables */
	nextproc = NPROC-1;
	nextsem = NSEM-1;
	nextqueue = NPROC;		/* q[0..NPROC-1] are processes */

	/* initialize free memory list */
	/* PC version has to pre-allocate 640K-1024K "hole" */
	if (maxaddr+1 > HOLESTART) {
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = (struct mblock *)HOLEEND;
		mptr->mlen = (int) truncew(((unsigned) HOLESTART -
	     		 (unsigned)&end));
        mptr->mlen -= 4;

		mptr = (struct mblock *) HOLEEND;
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned)maxaddr - HOLEEND -
	      		NULLSTK);
/*
		mptr->mlen = (int) truncew((unsigned)maxaddr - (4096 - 1024 ) *  4096 - HOLEEND - NULLSTK);
*/
	} else {
		/* initialize free memory list */
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned)maxaddr - (int)&end -
			NULLSTK);
	}
	

	for (i=0 ; i<NPROC ; i++)	/* initialize process table */
		proctab[i].pstate = PRFREE;


#ifdef	MEMMARK
	_mkinit();			/* initialize memory marking */
#endif

#ifdef	RTCLOCK
	clkinit();			/* initialize r.t.clock	*/
#endif

	mon_init();     /* init monitor */

#ifdef NDEVS
	for (i=0 ; i<NDEVS ; i++ ) {	    
	    init_dev(i);
	}
#endif

	pptr = &proctab[NULLPROC];	/* initialize null process entry */
	pptr->pstate = PRCURR;
	for (j=0; j<7; j++)
		pptr->pname[j] = "prnull"[j];
	pptr->plimit = (WORD)(maxaddr + 1) - NULLSTK;
	pptr->pbase = (WORD) maxaddr - 3;
/*
	pptr->plimit = (WORD)(maxaddr + 1) - NULLSTK - (4096 - 1024 )*4096;
	pptr->pbase = (WORD) maxaddr - 3 - (4096-1024)*4096;
*/
	pptr->pesp = pptr->pbase-4;	/* for stkchk; rewritten before used */
	*( (int *)pptr->pbase ) = MAGIC;
	pptr->paddr = (WORD) nulluser;
	pptr->pargs = 0;
	pptr->pprio = 0;
	currpid = NULLPROC;

	for (i=0 ; i<NSEM ; i++) {	/* initialize semaphores */
		(sptr = &semaph[i])->sstate = SFREE;
		sptr->sqtail = 1 + (sptr->sqhead = newqueue());
	}

	rdytail = 1 + (rdyhead=newqueue());/* initialize ready list */

		
	
	/*The NULL process is somewhat of a special case, as it builds itself in the function sysinit(). The NULL process should not have a private heap (like any processes created with create()).
The following should occur at system initialization:

Set the DS and SS segments' limits to their highest values. This will allow processes to use memory up to the 4 GB limit without generating general protection faults. Make sure the initial stack pointer (initsp) is set to a real physical address (the highest physical address) as it is in normal Xinu. See i386.c. And don't forget to "steal" physical memory frames 2048 - 4096 for backing store purposes.
Initialize all necessary data structures.
Create the page tables which will map pages 0 through 4095 to the physical 16 MB. These will be called the global page tables.
Allocate and initialize a page directory for the NULL process.
Set the PDBR register to the page directory for the NULL process.
Install the page fault interrupt service routine.
Enable paging.*/

	i = 0;
	//init arrays of non-private BS structure 
	for ( i = 0; i < BSCOUNT; ++i)
	{
		int j =0;
		for (j = 0; j < NULLPROC; ++j)
		{
			bsm_tab[i].mappedToProc[j] = -1;
			bsm_tab[i].proc_vpnos[j] = HBASE;
			bsm_tab[i].mappedProcNpages[j] = -1;
		}
		
	}
	//kprintf("\n\n\n------------------ INIT BS calling------------\n\n\n");
	init_bsm();
	//kprintf("\n\n\n------------------2 INIT frame call-ing-----------\n\n\n");
	init_frm();
	
	//Add global table
	AddGlobalTable(NULLPROC);

	//Add directory
	int directoryFrame = 0;
	get_frm(&directoryFrame);
	proctab[NULLPROC].pdbr = (FRAME0 + directoryFrame)*NBPG;
	frm_tab[directoryFrame].fr_type = FR_DIR;
	frm_tab[directoryFrame].fr_status = FRM_MAPPED;
	frm_tab[directoryFrame].fr_pid = NULLPROC;
	pd_t *Maindirectory = (FRAME0+ directoryFrame)*NBPG;
	for(i = 0; i < 1024; i++)
	{
		Maindirectory[i].pd_write = 1;
	}
	for(i = 0; i < 4; i++)
	{
		Maindirectory[i].pd_pres = 1;
		Maindirectory[i].pd_base = FRAME0+i;
	}

	//Done

	//write pdbr
	write_cr3(proctab[NULLPROC].pdbr);

	//set page fault routine 
	SYSCALL pfintr();
	
	set_evec(14,(u_long)pfintr);

	//enable paging 
	enable_paging(); 

	return(OK);

}

stop(s)
char	*s;
{
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while(1)
		/* empty */;
}

delay(n)
int	n;
{
	DELAY(n);
}

/*------------------------------------------------------------------------
 * sizmem - return memory size (in pages)
 *------------------------------------------------------------------------
 */
long sizmem()
{
	unsigned char	*ptr, *start, stmp, tmp;
	int		npages;

	/* at least now its hacked to return
	   the right value for the Xinu lab backends (16 MB) */

	return 4096; 

	start = ptr = 0;
	npages = 0;
	stmp = *start;
	while (1) {
		tmp = *ptr;
		*ptr = 0xA5;
		if (*ptr != 0xA5)
			break;
		*ptr = tmp;
		++npages;
		ptr += NBPG;
		if ((int)ptr == HOLESTART) {	/* skip I/O pages */
			npages += (1024-640)/4;
			ptr = (unsigned char *)HOLEEND;
		}
	}
	return npages;
}


void AddGlobalTable(int pid)
{

	int pageFrame = 0;
	pt_t *page_entry;
	//Global Page table
	int globalArray[4];
	int i;int j = 0;
	for(i = 0; i < 4; i++)
	{
		get_frm(&pageFrame);
		frm_tab[pageFrame].fr_status = FRM_MAPPED;
		frm_tab[pageFrame].fr_type = FR_TBL;
		frm_tab[pageFrame].fr_pid = pid;
		page_entry = (FRAME0+pageFrame);
		page_entry =  (FRAME0+pageFrame)* NBPG;
		int constAddition = i*FRAME0;
		globalArray[i] = i;
		for(j = 0; j < 1024; j++)
		{
			  page_entry->pt_pres =  page_entry->pt_global = page_entry->pt_write = 1;
  		      page_entry->pt_pwt = page_entry->pt_mbz = page_entry->pt_user =   page_entry->pt_dirty = page_entry->pt_pcd = page_entry->pt_acc =  page_entry->pt_avail = 0;
  		      page_entry->pt_base = constAddition+j;
  		      page_entry++;	
  		     
		}
	}
}