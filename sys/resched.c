/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sched.h>
#include <math.h>
#include <stdio.h>
unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */

int sched_class = EXPDISTSCHED;
void setschedclass(int sched){
	sched_class = sched;
}
int getschedclass(){
	return sched_class;
}

int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	if (sched_class == EXPDISTSCHED){
		int element_index;
		double rn = expdev(0.1);/* random number generator */
		if ((optr=&proctab[currpid])->pstate == PRCURR){
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}/* handling old pointer */
		if(q[rdyhead].qnext==rdytail){
			currpid = NULLPROC;
			nptr = &proctab[currpid];
			nptr->pstate = PRCURR;
		#ifdef	RTCLOCK
			preempt = QUANTUM;		
		#endif
			ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
			return OK;
		}/* Handling null process */
		/* The queue has priority from lowest to highest */
		if(rn<q[q[rdyhead].qnext].qkey){
			element_index = q[rdyhead].qnext;
		}
		else if(rn>=q[q[rdytail].qprev].qkey){
			element_index = q[rdytail].qprev;
		}
		else{
			int index_queue = q[rdyhead].qnext;
			while(index_queue!=rdytail && q[index_queue].qkey<= rn){
				index_queue = q[index_queue].qnext;
			}
			element_index = index_queue;
		}/* Conditions to apply exp distribution  */
		dequeue(element_index);
		currpid = element_index;
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;
		#ifdef	RTCLOCK
			preempt = QUANTUM;
		#endif
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
		return OK;
	}/* Handling the new pointer for Exponential distributed Scheduler */

	if (sched_class== LINUXSCHED){
		register struct	pentry *prptr;
		int goodness_id;
		int goodness_zero = 0;
		int goodness_maximum = 0;
		int goodness_pb= NULLPROC;

		if ((optr=&proctab[currpid])->pstate == PRCURR){
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
			optr->counter= preempt;
			if (optr->counter<0){
				optr->counter=0;
			}
			if(optr->counter > 0){
				optr->goodness =optr->pprio + optr->counter;
			}
			else{
				optr->goodness = 0;
			}/* Handling old pointer and updating goodness value */
		}
		for(goodness_id = 0; goodness_id< NPROC;goodness_id++){
			prptr = &proctab[goodness_id];
			if ((prptr->pstate == PRCURR ||prptr->pstate == PRREADY)&& (prptr->goodness>0))
			{
				goodness_zero = 1;
				break;
			}
		}/* checking if any of the process still has goodness */
		if(goodness_zero == 0)
		{
			for(goodness_id = 0; goodness_id< NPROC;goodness_id++){
				prptr = &proctab[goodness_id];
				if(prptr->pstate == PRFREE){
					continue;
				}
				if(prptr->counter<=0){
					prptr->quantum = prptr->pprio;
				}
				else{
					prptr->quantum = prptr->pprio + (prptr->counter/2);
				}
				prptr->counter = prptr->quantum;
				prptr->goodness = prptr->pprio + prptr->counter;
			}
		}/* Quantum addition for different cases and updating counter , goodness*/
		for(goodness_id = 0; goodness_id< NPROC;goodness_id++){
			prptr = &proctab[goodness_id];
			if(!(prptr->pstate == PRCURR||prptr->pstate == PRREADY)){
				continue;
			}
			if(prptr->goodness>goodness_maximum){
				goodness_maximum = prptr->goodness;
				goodness_pb = goodness_id;
			}
		}/* Checking the goodness maximum */
		if (goodness_maximum==0||goodness_pb==NULLPROC){
			currpid = NULLPROC;
			nptr = &proctab[currpid];
			nptr->pstate = PRCURR;
		#ifdef	RTCLOCK
			preempt = QUANTUM;		
		#endif
			ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
			return OK;
		}/* Handling NULL Process*/
		dequeue(goodness_pb);
		currpid = goodness_pb;
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;
		#ifdef RTCLOCK
			preempt = nptr->counter;
		#endif
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
		return OK;
	}/* updating final new pointer*/
	/* no switch needed if current process priority higher than next*/

	if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   (lastkey(rdytail)<optr->pprio)) {
		return(OK);
	}
	
	/* force context switch */

	if (optr->pstate == PRCURR) {
		optr->pstate = PRREADY;
		insert(currpid,rdyhead,optr->pprio);
	}

	/* remove highest priority process at end of ready list */

	nptr = &proctab[ (currpid = getlast(rdytail)) ];
	nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = QUANTUM;		/* reset preemption counter	*/
#endif
	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}

