/* P2 Thread Library*/
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

typedef struct Semaphore
{
	int semID;
	int value;
}MySemaphore;

typedef struct ThreadNodes
{
	int threadID;
	int parentThreadID;
	struct ThreadNodes *next;
	ucontext_t context;
}MyThread;

typedef struct BlockedNodes
{
	int threadID;
	int parentThreadID;
	int blockingThreadID;
	struct BlockedNodes *next;
	ucontext_t context;
}BlockedThread;

typedef struct SemBlockedNodes
{
	int threadID;
	int parentThreadID;
	int blockingThreadID;
	struct SemBlockedNodes *next;
	ucontext_t context;
}SemaphoreBlockedThread;

static int threadCount = 0, semCount = 0, curThreadID = 0, curThreadParentID = 0, blkThreadCount = 0, semBlkThreadCount = 0;
static ucontext_t mainInitContext;

static MyThread *front, *rear;
static BlockedThread *blkFront, *blkRear;
static SemaphoreBlockedThread *sBFront, *sBRear;

int isChildExist(int parentThreadID)
{

	//child existing in thread queue?
    MyThread *tempFront=front;
    while(tempFront!=NULL)
    {
    	if(tempFront->parentThreadID==parentThreadID)
    		return 1;
    	tempFront=tempFront->next;
    }

    //child existing in BlockedThread queue?
    BlockedThread *blkTempFront=blkFront;
    while(blkTempFront!=NULL)
    {
    	if(blkTempFront->parentThreadID==parentThreadID)
    		return 1;
    	blkTempFront=blkTempFront->next;
    }

    //child existing in SemaphoreBlockedThread queue?
    SemaphoreBlockedThread *sBTempFront=sBFront;
    while(sBTempFront!=NULL)
    {
    	if(sBTempFront->parentThreadID==parentThreadID)
    		return 1;
    	sBTempFront=sBTempFront->next;
    }
	
    return 0;
}

void insertInQueue(ucontext_t context, int threadID, int parentThreadID)
{
	//printf("insert ke andar");
	// inserting thread into queue
	MyThread *temp = (MyThread *)malloc(sizeof(MyThread));
	temp->next = NULL;
	temp->context = context;
	temp->threadID = threadID;
	temp->parentThreadID = parentThreadID;
	
	if(front == NULL || rear == NULL)
	{
		front = temp;
		rear = temp;
	}
	else
	{
		rear->next = temp;
		rear = temp;
	}
}

MyThread removeFromQueue()
{
	//printf("pop ke andar");
	//popping thread from queue
	if(front == NULL)
		return;
	else
	{
		MyThread *tempFront = front;
		front = front->next;
		if(front == NULL)
			rear = NULL;
		return *tempFront;
	}
}

BlockedThread removeFromBQueue(int threadID)
{
	BlockedThread *tempFront;
	if(blkFront == NULL)
	{
		return ;
	}
	else if(blkFront->threadID == threadID)
	{
		tempFront = blkFront;
		blkFront = blkFront->next;
		blkThreadCount--;
	}
	else
	{
		BlockedThread *prev;
		tempFront = blkFront;
		while(tempFront != NULL)
		{
			if(tempFront->threadID == threadID)
			{
				prev->next = tempFront->next;
				break;
			}
			prev = tempFront;
			tempFront = tempFront->next;
		}
	}
	return *tempFront;
}

SemaphoreBlockedThread removeFromSBQueue(int tid)
{
	SemaphoreBlockedThread *tempFront;
	if(sBFront == NULL)
	{
		return ;
	}
	else if(sBFront->threadID == tid)
	{
		tempFront = sBFront;
		sBFront = sBFront->next;
		semBlkThreadCount--;
	}
	else
	{
		semBlkThreadCount--;
		SemaphoreBlockedThread *prev;
		tempFront = sBFront;
		while(tempFront != NULL)
		{
			if(tempFront->threadID == tid)
			{
				prev->next = tempFront->next;
				break;
			}
			prev = tempFront;
			tempFront = tempFront->next;
		}
	}
	return *tempFront;
}


//-------------------------- THREAD PROCEDURES ---------------------------//



/*
	MyThread MyThreadCreate (void(*start_funct)(void *), void *args)
	This routine creates a new MyThread.
	The parameter start_func is the function in which the new thread starts executing.
	The parameter args is passed to the start function. This routine does not pre-empt the invoking thread.
	In others words the parent (invoking) thread will continue to run; the child thread will sit in the ready queue.
*/
MyThread* MyThreadCreate(void(*start_funct)(void *), void *args)
{
	//printf("thread create ke andar");
	ucontext_t mContext;
	getcontext(&mContext);
	mContext.uc_stack.ss_sp=malloc(8192);
	mContext.uc_stack.ss_size=8192;
	mContext.uc_link=NULL;
	makecontext(&mContext,(void(*)(void))start_funct, 1,args);
	insertInQueue(mContext,++threadCount, curThreadID);
	return rear;
}



/*
	void MyThreadYield(void)
	Suspends execution of invoking thread and yield to another thread.
	The invoking thread remains ready to execute—it is not blocked.
	Thus, if there is no other ready thread, the invoking thread will continue to execute.
*/
void MyThreadYield(void)
{
	//printf("thread yield ke andar");
	if(front == NULL)
		return;

	ucontext_t mContext;
	MyThread swapContext;
	getcontext(&mContext);
	insertInQueue(mContext, curThreadID, curThreadParentID);
	swapContext = removeFromQueue();
	//printf("check 2 inside yield");
	curThreadID = swapContext.threadID;
	curThreadParentID = swapContext.parentThreadID;
	swapcontext(&(rear->context), &swapContext.context);
}


/*
	int MyThreadJoin(MyThread thread)
	Joins the invoking function with the specified child thread.
	If the child has already terminated, do not block.
	Note: A child may have terminated without the parent having joined with it.
	Returns 0 on success (after any necessary blocking). It returns -1 on failure.
	Failure occurs if specified thread is not an immediate child of invoking thread.
*/
int MyThreadJoin(MyThread* childThread)
{
	//printf("thread join ke andar");
	if(childThread == NULL)
		return 0;

	MyThread thread = *childThread;
	//printf("threadID=%d  parentID=%d  child=%d\n",thread.parentThreadID,curThreadId,thread.threadID);
	if(thread.parentThreadID != curThreadID)
		return -1;

	
	//Inside Join
	int tid = thread.threadID;
	int cTerminated = 1;
	//thread existing in thread queue?
    MyThread *tempFront=front;
    while(tempFront!=NULL)
    {
    	if(tempFront->threadID==tid)
    	{
    		cTerminated = 0;
    		break;
    	}
    	tempFront=tempFront->next;
    }

    //thread existing in BlockedThread queue?
    BlockedThread *blkTempFront=blkFront;
    while(blkTempFront!=NULL)
    {
    	if(blkTempFront->threadID==tid)
    	{
    		cTerminated = 0;
    		break;
    	}
    	blkTempFront=blkTempFront->next;
    }

    //thread existing in SemaphoreBlockedThread queue?
    SemaphoreBlockedThread *sBTempFront=sBFront;
    while(sBTempFront!=NULL)
    {
    	if(sBTempFront->threadID==tid)
    	{
    		cTerminated = 0;
    		break;
    	}
    	sBTempFront=sBTempFront->next;
    }

	if(cTerminated == 0)
	{
		ucontext_t mContext;
		MyThread swapContext;
		getcontext(&mContext);
		
		blkThreadCount++;
		BlockedThread *temp = (BlockedThread *)malloc(sizeof(BlockedThread));
		temp->context = mContext;
		temp->threadID = curThreadID;
		temp->parentThreadID = curThreadParentID;
		temp->blockingThreadID = thread.threadID;
		temp->next = NULL;
		if(blkFront == NULL || blkRear == NULL)
		{
			blkFront = temp;
			blkRear = temp;
		}
		else
		{
			blkRear->next = temp;
			blkRear = temp;
		}

		//printf("removal ke pehle");
		swapContext = removeFromQueue();
		curThreadID = swapContext.threadID;
		curThreadParentID = swapContext.parentThreadID;
		swapcontext(&(blkRear->context), &swapContext.context);
	}
	return 0;

}



/*
	void MyThreadJoinAll(void)
	Waits until all children have terminated. Returns immediately if there are no active children.
*/
void MyThreadJoinAll(void)
{
	//printf("join all ke andar");
	if(isChildExist(curThreadID))
	{
		ucontext_t mContext;
		MyThread swapContext;
		getcontext(&mContext);

		blkThreadCount++;
		BlockedThread *temp = (BlockedThread *)malloc(sizeof(BlockedThread));
		temp->context = mContext;
		temp->threadID = curThreadID;
		temp->next = NULL;
		temp->parentThreadID = curThreadParentID;
		temp->blockingThreadID = 0;
		if(blkFront == NULL || blkRear == NULL)
		{
			blkFront = temp;
			blkRear = temp;
		}
		else
		{
			blkRear->next = temp;
			blkRear = temp;
		}
		
		swapContext = removeFromQueue();
		curThreadID = swapContext.threadID;
		curThreadParentID = swapContext.parentThreadID;
		swapcontext(&(blkRear->context), &swapContext.context);
	}
}



/*
	void MyThreadExit(void)
	Terminates the invoking thread.
	Note: all MyThreads are required to invoke this function.
	Do not allow functions to “fall out” of the start function.
*/
void MyThreadExit(void)
{
	//printf("exit ke andar");
	//printf("End Context ThreadId= %d  ThreadCount=%d  BThreadCount=%d\n",curThreadID,threadCount,blkThreadCount);
    //If any other thread blocked on this exiting thread then unblock it
	if(blkThreadCount > 0 || blkFront != NULL)
	{
		BlockedThread *blkTempFront = blkFront;
		while(blkTempFront != NULL)
		{
			if(blkTempFront->blockingThreadID == curThreadID)
			{
				BlockedThread tempBlk = removeFromBQueue(blkTempFront->threadID);	
				//printf("c2 inside exit");
				insertInQueue(tempBlk.context, tempBlk.threadID, tempBlk.parentThreadID);
				break;
			}
			blkTempFront = blkTempFront->next;
		}
	}
	
	int blocked = 0;
	BlockedThread *blkTempFront=blkFront;
    while(blkTempFront!=NULL)
    {
    	if(blkTempFront->threadID == curThreadParentID)
    	{
    		blocked = 1;
    		break;
    	}
    	blkTempFront=blkTempFront->next;
    }

    
	if(blocked == 1 && !isChildExist(curThreadParentID))
	{
		BlockedThread tempBlk = removeFromBQueue(curThreadParentID);
		//printf("c3 inside exit");
		insertInQueue(tempBlk.context, tempBlk.threadID, tempBlk.parentThreadID);

	}

	ucontext_t mainContext;
	MyThread swapContext;
	if(front == NULL)
	{
		ucontext_t tempCC;
		swapcontext(&tempCC, &mainInitContext);
	}
	else
	{
		swapContext = removeFromQueue();
		curThreadID = swapContext.threadID;
		curThreadParentID = swapContext.parentThreadID;
		swapcontext(&mainContext, &swapContext.context);
	}
}


/* 
	Now starting with Semaphore routines. Semaphore operations of
	Init, Signal, Wait, Destroy 
*/

/*
	MySemaphore MySemaphoreInit(int initialValue)
	Create a semaphore. Set the initial value to initialValue, which must be non-negative.
	A positive initial value has the same effect as invoking MySemaphoreSignal the same number of times.
	On error it returns NULL.
*/

MySemaphore* MySemaphoreInit(int initialValue)
{
	if(initialValue < 0)
		return NULL;
	MySemaphore *sem = (MySemaphore *)malloc(sizeof(MySemaphore));
	sem->semID = ++semCount;
	sem->value = initialValue;
	return sem;
}




/*
	void MySemaphoreSignal(MySemaphore sem)
	Signal semaphore sem. The invoking thread is not pre-empted.
*/
void MySemaphoreSignal(MySemaphore *sem)
{
	//printf("semsignal ke andar");
	if(sem == NULL)
		return;
	if(sem->value++ < 0)
	{
		SemaphoreBlockedThread *sBTempFront = sBFront;
		while(sBTempFront != NULL)
		{
			if(sBTempFront->blockingThreadID == sem->semID)
			{
				SemaphoreBlockedThread tempBlocked=removeFromSBQueue(sBTempFront->threadID);
				//printf("c2 inside semsignal");
				insertInQueue(tempBlocked.context, tempBlocked.threadID, tempBlocked.parentThreadID);
				break;
			}
			sBTempFront = sBTempFront->next;
		}
	}
}



/*
	void MySemaphoreWait(MySemaphore sem)
	Wait on semaphore sem.
*/
void MySemaphoreWait(MySemaphore *sem)
{
	if(sem == NULL)
		return;
	sem->value = sem->value - 1;
	if(sem->value < 0)
	{
		ucontext_t mContext;
		MyThread swapContext;
		getcontext(&mContext);

		semBlkThreadCount++;
		SemaphoreBlockedThread *temp = (SemaphoreBlockedThread *)malloc(sizeof(SemaphoreBlockedThread));
		temp->context = mContext;
		temp->threadID = curThreadID;
		temp->parentThreadID = curThreadParentID;
		temp->blockingThreadID = sem->semID;
		temp->next = NULL;
		if(sBFront == NULL || sBRear == NULL)
		{
			sBFront = temp;
			sBRear = temp;
		}
		else
		{
			sBRear->next = temp;
			sBRear = temp;
		}

		swapContext = removeFromQueue();
		curThreadID = swapContext.threadID;
		curThreadParentID = swapContext.parentThreadID;
		swapcontext(&(sBRear->context), &swapContext.context);
	}
}

/*
	int MySemaphoreDestroy(MySemaphore sem)
	Destroy semaphore sem. Do not destroy semaphore if any threads are blocked on the queue.
	Return 0 on success, -1 on failure.
*/

int MySemaphoreDestroy(MySemaphore *sem)
{
	if(sem == NULL)
		return 0;
	SemaphoreBlockedThread *sBTempFront = sBFront;
	while(sBTempFront != NULL)
	{
		if(sBTempFront->blockingThreadID == sem->semID)
			return -1;
		sBTempFront = sBTempFront->next;
	}

	free(sem);
	return 0;
}

/*
	Unix Process Routine
	void MyThreadInit (void(*start_funct)(void *), void *args)
	This routine is called before any other MyThread call. It is invoked only by the Unix process.
	It is similar to invoking MyThreadCreate immediately followed by MyThreadJoinAll.
	The MyThread created is the oldest ancestor of all MyThreads—it is the “main” MyThread.
	This routine can only be invoked once.
	It returns when there are no threads available to run (i.e., the thread ready queue is empty.
*/


void MyThreadInit(void(*start_funct)(void *), void *args)
{
	MyThread swapContext;
	ucontext_t mContext;
	getcontext(&mContext);

	mContext.uc_stack.ss_sp = malloc(8192);
	mContext.uc_stack.ss_size =  8192;
	mContext.uc_link = NULL;
	
	makecontext(&mContext, (void(*)(void))start_funct, 1, args);
	insertInQueue(mContext, ++threadCount, curThreadID);
	
	swapContext = removeFromQueue();
	curThreadID = swapContext.threadID;
	curThreadParentID = swapContext.parentThreadID;
	
	swapcontext(&mainInitContext, &swapContext.context);
}

