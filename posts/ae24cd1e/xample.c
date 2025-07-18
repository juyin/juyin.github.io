
/******************************************************************************
 **
 ** FILE NAME   :  vthreadExample.c
 **
 ** DESCRIPTION :  This file contains sample execution for virtual thread usage.
 **                The mechanism of compilation of this file is as follows:
 **                gcc -Wall -g -o vthreadExample vthreadExample.c
 **                Execution method is:
 **                ./vthreadExample
 **                There are no other header files other than standard header
 **                files which are included in this C file.
 **
 ** DATE            AUTHOR          REF         REASON
 ** ------          ---------       -----       --------
 ** 17-Dec-10       R. Ezhirpavai               Initial Version.
 **
 **
 **  Copyright (C) 2010 Aricent Inc . All Rights Reserved
*******************************************************************************/

/******************************************************************************
 ** The standard system header files included are mentioned in this section.
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include "my_trace.h"

/******************************************************************************
 ** The scalability values used for this program are mentioned in this section.
*******************************************************************************/
/* Following hash define controls the maximum commands which can be given in
 * sequence.
 */
#define SAMPLE_MAX_COMMANDS 200
/* The following defines the maximum number of vitual threads that can be
 * created simultaneously through this program. Actual number of virtual
 * threads that can be created would depend on the sequence list Info
 * defined later.
 */
#define MAX_SIMULTANEOUS_TRANS 2000
/* The following defines tha virtual thread's memory size where it would
 * store the stack sizes. In case of large number of information on stack
 * this would increase.
 */
#define MAX_VIRTUAL_THREAD_STACK_SIZE 8096

/******************************************************************************
 ** This section defines some structures and hash defined for processing needs.
*******************************************************************************/
/* Common variable types are defined here. */
typedef unsigned char eU8;
typedef unsigned short eU16;
typedef unsigned int eU32;
typedef signed char eS8;
typedef signed short eS16;
typedef signed int eS32;
typedef eU8 tSampleReturn;

#define SUCCESS 1
#define FAILURE 0

/* These define command types for working on virtual threads. */
#define C 1 /* Create */
#define D 2 /* Delete */
#define W 3 /* Wakeup */
#define K 4 /* Kill */

/* For execution of a command on a virtual thread the following structure
 * is filled. It indicates the command type like create, delete, wakeup
 * etc to be executed and the virtual thread id on which the execution has
 * to be done.
 */
typedef struct
{
    eU8 commandType;
    eU32 threadId;
} tSampleSeqInfo;

/* For execution of certain sequence of commands on different virtual
 * threads, the commands are stored in following structure.
 * Command sequence that has to be executed is also mentioned in this
 * program.
 */
typedef struct
{
    tSampleSeqInfo  command[SAMPLE_MAX_COMMANDS];
    eU32            count;
} tSampleSeqListInfo;

/* The function pointer for handling the call back from virtual thread
 * creation is defined as follows. The sample program shall mention a
 * function prototype of this type and pass to virtual thread creation
 * function, so that after virtual thread is created this function can
 * be called.
 */
typedef void (*tVThCallBackFP)(void);

/* Structure where the stack contexts are stored are mentioned in
 * this structure. This is not required per virtual thread, but used
 * one in a program required for swapping stacks.
 */
typedef struct
{
    ucontext_t          outerUctx;
    ucontext_t          tmpOuterUctx;
} tVthContext;

/* The stack for each virtual thread is stored in this structure. This
 * is one per virtual thread.
 */
typedef struct
{
    ucontext_t         uctx;
} tVThread;

/******************************************************************************
 ** The global context created is mentioned below. This is global so that after
 ** virtual thread is created or is woken up, then information that we want to
 ** pass to that thread can be taken from this.
*******************************************************************************/
/*-----------------------------------------------------------------------------
 * Parameter description of global context:
 * noOfVitualThreadCreated: No. of virtual threads created by program and which
 *                          are still alive are mentioned in this.
 * vThreadCtx             : The common stack context for whole program is stored
 *                          in this and is not different for each virtual thread.
 *                          After execution of a virtual thread, to come back to
 *                          common stack context, this is used.
 * vThInfo                : Stack context for each virtual thread is stored in
 *                          this, so that it can be accessed again after the
 *                          virtual thread is called again.
 * pMsgProc               : The actual command that has now been chosen from
 *                          sequence list is stored in this so that from within
 *                          virtual thread execution, it can read that.
------------------------------------------------------------------------------*/
typedef struct
{
    eU32           noOfVitualThreadCreated;
    tVthContext    vThreadCtx;
    tVThread       vThInfo[MAX_SIMULTANEOUS_TRANS];
    tSampleSeqInfo *pMsgProc;
} tGlbContext;

/******************************************************************************
 ** Function prototypes used in this program are mentioned in this section.
*******************************************************************************/
/*-----------------------------------------------------------------------------
 -- Macro Name: SAMPLE_PRINT
 -- Decription: Sends the output for traces. The current definition sends the
 --             output on console.
 -- Arguments :
 -- x         : Can be more than one arguments which are sent for sending the
 --             traces.
------------------------------------------------------------------------------*/
#define  SAMPLE_PRINT(x) printf x

/*-----------------------------------------------------------------------------
 -- Function Name: sampleInit
 -- Decription   : This function does the initialization for the sample program
 --                to initialize global variables, sequence lists, virtual
 --                thread contexts. This is called only once in at the
 --                beginning of the program.
 -- Arguments    :
 -- pSeqListInfo : Complete sequence list is passed which would later be used
 --                for setting the command sequence to be executed. This
 --                function shall initialize the sequence list.
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for initialization of the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn sampleInit(tSampleSeqListInfo *pSeqListInfo,
                                tVthContext *pVThreadCtx);
/*-----------------------------------------------------------------------------
 -- Function Name: sampleSetSeq
 -- Decription   : This function is called only once after program
 --                initialization. This is used to set the command sequence
 --                that user wants to execute. In case user wants to change
 --                sequence of execution of commands on different virtual
 --                thread, then this has to be modified.
 -- Arguments    :
 -- pSeqListInfo : The following function sets the sequence of commands that
 --                have to be executed for different virtual threads in this
 --                list.
------------------------------------------------------------------------------*/
static tSampleReturn sampleSetSeq(tSampleSeqListInfo *pSeqListInfo);
/*-----------------------------------------------------------------------------
 -- Function Name: sampleHandleMsgSeq
 -- Decription   : This function handles one by the one the command sequence
 --                set by the user to call the commands for execution of
 --                of functionality of different virtual threads.
 -- Arguments    :
 -- pSeqInfo     : The single command that has to be currently executed is
 --                mentioned in this argument for the command type and the
 --                virtual thread on which it has to be executed.
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn sampleHandleMsgSeq(tSampleSeqInfo *pSeqInfo,
                                        tVthContext *pVThreadCtx);
/*-----------------------------------------------------------------------------
 -- Function Name: sampleVThreadCallBack
 -- Decription   : Once virtual thread is created this call back function is
 --                called. This is passed as argument when virtual thread is
 --                to be created.
 -- Arguments    : None
 --                Data to be used for processing after virtual thread is
 --                created is stored in global context.
------------------------------------------------------------------------------*/
void sampleVThreadCallBack(void);

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadBaseInit
 -- Decription   : This function gets the outer stack context at the time of
 --                initialization.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadBaseInit(tVthContext *pVthctx);

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadCreate
 -- Decription   : This function spawns a new virtual thread.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
 --                This will be updated for the stack context in this function.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadCreate(tVthContext *pVthctx, tVThread *pVth,
                                   tVThCallBackFP funcPtr);
/*-----------------------------------------------------------------------------
 -- Function Name: vThreadDestroy
 -- Decription   : This function is called to destroy a virtual thread. It
 --                needs to be called for cleanup even at end of normal
 --                virtual thread execution completion.
 -- Arguments    :
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
 --                When thread is destroyed, the memory for the stack context
 --                is deleted.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadDestroy(tVThread *pVth);
/*-----------------------------------------------------------------------------
 -- Function Name: vThreadSleep
 -- Decription   : This function swaps the stack context from the current
 --                virtual thread to the stack context for the base program.
 --                This is used to Relinquish Control from virtual thread to
 --                base program, so that next command can be processed while
 --                this virtual thread still awaits info from base program.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadSleep(tVthContext *pVthctx, tVThread *pVth);
/*-----------------------------------------------------------------------------
 -- Function Name: vThreadWakeup
 -- Decription   : When a command comes for a virtual thread which is already
 --                sleeping, then this function is called to again swap the
 --                stack context from base program stack to the specific
 --                virtual thread stack context. This would eventually Wakeup
 --                the Virtual Thread
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadWakeup(tVthContext *pVthctx, tVThread *pVth);
/*-----------------------------------------------------------------------------
 -- Function Name: vThreadBaseUpdate
 -- Decription   : After every virtual thread processing is over, this function
 --                is called to Store the base Stack context again.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadBaseUpdate(tVthContext *pVthctx);

/******************************************************************************
 ** Global contexts for the program is defined in this section.
*******************************************************************************/
tGlbContext  glbCtx;


/******************************************************************************
 ** The function definitions are mentioned in this section.
*******************************************************************************/
/*-----------------------------------------------------------------------------
 -- Function Name: main
 -- Decription   : This is the entry point function of this program which calls
 --                other function to execute a sample program that would
 --                explain how to use virtual thread and how is the execution
 --                of virtual threads.
 -- Arguments    : None
------------------------------------------------------------------------------*/
int main(void)
{
    tSampleReturn retVal = SUCCESS;
    tSampleSeqListInfo   seqListInfo;
    tSampleSeqInfo       *pSeqInfo = NULL;
    eU32                 i = 0, j = 0;
    tVthContext          *pVThreadCtx = &(glbCtx.vThreadCtx);
	
	MY_TRACE(1, "test\n");

    if (FAILURE == sampleInit(&seqListInfo, pVThreadCtx))
    {
        retVal = FAILURE;
    }
    else if (FAILURE == sampleSetSeq(&seqListInfo))
    {
        retVal = FAILURE;
    }
    else if (0 != seqListInfo.command[0].commandType)
    {
    	MY_TRACE(1, "seqListInfo.command[0].commandType=%d", seqListInfo.command[0].commandType);
        for (i = 0; ((i < seqListInfo.count) && (FAILURE != retVal)); i++)
        {
			
			pSeqInfo = &(seqListInfo.command[i]);

            for (j = 0; j < 2; j++)
            {
            	MY_TRACE(1, "i=%d,j=%d", i,j);
                if (FAILURE == vThreadBaseUpdate(pVThreadCtx))
                {
                    MY_TRACE(1, "Base Updation for Virtualization is failure\n");
                    retVal = FAILURE;
                }

                if (FAILURE == sampleHandleMsgSeq(pSeqInfo, pVThreadCtx))
                {
                    MY_TRACE(1, "Message handling is failure\n");
                    retVal = FAILURE;
                }

				
				MY_TRACE(1, "test\n");

                break;
            }
        }
    }

    MY_TRACE(1, " Program Executed the sequence loop normally and is exiting,"\
             " with return value as %d \n", retVal);
    return 0;
}

/*-----------------------------------------------------------------------------
 -- Function Name: sampleInit
 -- Decription   : This function does the initialization for the sample program
 --                to initialize global variables, sequence lists, virtual
 --                thread contexts. This is called only once in at the
 --                beginning of the program.
 -- Arguments    :
 -- pSeqListInfo : Complete sequence list is passed which would later be used
 --                for setting the command sequence to be executed. This
 --                function shall initialize the sequence list.
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for initialization of the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn sampleInit(tSampleSeqListInfo *pSeqListInfo,
                                tVthContext *pVThreadCtx)
{
    tSampleReturn retVal = SUCCESS;
    eU32          i = 0;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);

    if (NULL == pSeqListInfo)
    {
        MY_TRACE(1, "Wrong input in Function %s\n", __FUNCTION__);
        retVal = FAILURE;
    }
    else
    {
        for (i = 0; i < SAMPLE_MAX_COMMANDS; i++)
        {
            pSeqListInfo->command[i].commandType = 0;
            pSeqListInfo->command[i].threadId = 0;
        }

        glbCtx.noOfVitualThreadCreated = 0;
        glbCtx.pMsgProc = NULL;

        for (i = 0; i < MAX_SIMULTANEOUS_TRANS; i++)
        {
            glbCtx.vThInfo[i].uctx.uc_stack.ss_sp = NULL;
            glbCtx.vThInfo[i].uctx.uc_stack.ss_size = 0;
            glbCtx.vThInfo[i].uctx.uc_link = NULL;
        }

        if (FAILURE == vThreadBaseInit(pVThreadCtx))
        {
            MY_TRACE(1, "Virtual Thread Initialization is failure\n");
            retVal = FAILURE;
        }
    }

    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: sampleSetSeq
 -- Decription   : This function is called only once after program
 --                initialization. This is used to set the command sequence
 --                that user wants to execute. In case user wants to change
 --                sequence of execution of commands on different virtual
 --                thread, then this has to be modified.
 -- Arguments    :
 -- pSeqListInfo : The following function sets the sequence of commands that
 --                have to be executed for different virtual threads in this
 --                list.
------------------------------------------------------------------------------*/
static tSampleReturn sampleSetSeq(tSampleSeqListInfo *pSeqListInfo)
{
    tSampleReturn retVal = SUCCESS;
    eU32          i = 0;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);

    if (NULL == pSeqListInfo)
    {
        MY_TRACE(1, "Wrong input in Function %s\n", __FUNCTION__);
        retVal = FAILURE;
    }
    else
    {
        /*C1, W1, C2, W2, W1, D2, W1, D1, C2, W2, C3, C1, W1, W3, W2, K3, D2, D1 */
        i = 0;
        /* Creation for Thread 1 */
        pSeqListInfo->command[i].commandType = C;
        pSeqListInfo->command[i].threadId = 1;
        i++;

        /* Wakeup for Thread 1 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 1;
        i++;
#if 0

        /* Creation for Thread 2 */
        pSeqListInfo->command[i].commandType = C;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Wakeup for Thread 2 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Wakeup for Thread 1 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 1;
        i++;

        /* Deletion for Thread 2 */
        pSeqListInfo->command[i].commandType = D;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Wakeup for Thread 1 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 1;
        i++;
#endif
        /* Deletion for Thread 1 */
        pSeqListInfo->command[i].commandType = D;
        pSeqListInfo->command[i].threadId = 1;
        i++;
		
#if 0
		/* Creation for Thread 2 */
        pSeqListInfo->command[i].commandType = C;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Wakeup for Thread 2 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Creation for Thread 3 */
        pSeqListInfo->command[i].commandType = C;
        pSeqListInfo->command[i].threadId = 3;
        i++;

        /* Creation for Thread 1 */
        pSeqListInfo->command[i].commandType = C;
        pSeqListInfo->command[i].threadId = 1;
        i++;

        /* Wakeup for Thread 1 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 1;
        i++;

        /* Wakeup for Thread 3 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 3;
        i++;

        /* Wakeup for Thread 2 */
        pSeqListInfo->command[i].commandType = W;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Kill for Thread 3 */
        pSeqListInfo->command[i].commandType = K;
        pSeqListInfo->command[i].threadId = 3;
        i++;

        /* Deletion for Thread 2 */
        pSeqListInfo->command[i].commandType = D;
        pSeqListInfo->command[i].threadId = 2;
        i++;

        /* Deletion for Thread 1 */
        pSeqListInfo->command[i].commandType = D;
        pSeqListInfo->command[i].threadId = 1;
        i++;
#endif
        pSeqListInfo->count = i;
        MY_TRACE(1, "Total No. of Commands Chosen = %d\n", pSeqListInfo->count);
    }

    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: sampleHandleMsgSeq
 -- Decription   : This function handles one by the one the command sequence
 --                set by the user to call the commands for execution of
 --                of functionality of different virtual threads.
 -- Arguments    :
 -- pSeqInfo     : The single command that has to be currently executed is
 --                mentioned in this argument for the command type and the
 --                virtual thread on which it has to be executed.
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn sampleHandleMsgSeq(tSampleSeqInfo *pSeqInfo,
                                        tVthContext *pVThreadCtx)
{
    tSampleReturn retVal = SUCCESS;
    eU32          i = 0;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);

    if (NULL == pSeqInfo)
    {
        MY_TRACE(1, "Wrong input in Function %s\n", __FUNCTION__);
        retVal = FAILURE;
    }
    else
    {
        switch (pSeqInfo->commandType)
        {
            case C:
            {
                MY_TRACE(1, "Executing Creation of Virtual Thread for"\
                         " Thread %d\n", pSeqInfo->threadId);
                glbCtx.pMsgProc = pSeqInfo;

                if (FAILURE == vThreadCreate(pVThreadCtx,
                                             &(glbCtx.vThInfo[pSeqInfo->threadId]),
                                             sampleVThreadCallBack))
                {
                    MY_TRACE(1, "Virtual Thread Creation is failure\n");
                    retVal = FAILURE;
                }

                MY_TRACE(1, "Virtual Thread Creation Function Ended, which is"\
                         " not normal case\n");
                break;
            }

            case D:
            {
                MY_TRACE(1, "Executing Deletion of Virtual Thread for"\
                         " Thread %d\n", pSeqInfo->threadId);
                glbCtx.pMsgProc = pSeqInfo;

                if (FAILURE == vThreadWakeup(pVThreadCtx,
                                             &(glbCtx.vThInfo[pSeqInfo->threadId])))
                {
                    MY_TRACE(1, "Virtual Thread Creation is failure\n");
                    retVal = FAILURE;
                }

                break;
            }

            case W:
            {
                MY_TRACE(1, "Executing Wakeup of Virtual Thread for"\
                         " Thread %d\n", pSeqInfo->threadId);
                glbCtx.pMsgProc = pSeqInfo;

                if (FAILURE == vThreadWakeup(pVThreadCtx,
                                             &(glbCtx.vThInfo[pSeqInfo->threadId])))
                {
                    MY_TRACE(1, "Virtual Thread Creation is failure\n");
                    retVal = FAILURE;
                }

                break;
            }

            case K:
            {
                MY_TRACE(1, "Executing Kill of Virtual Thread for"\
                         " Thread %d\n", pSeqInfo->threadId);

                if (FAILURE == vThreadDestroy(
                        &(glbCtx.vThInfo[pSeqInfo->threadId])))
                {
                    MY_TRACE(1, "Virtual Thread Destroying is failure\n");
                    retVal = FAILURE;
                }

                break;
            }

            default:
            {
                MY_TRACE(1, "Wrong command Type fed for Sequence count No. %d\n",
                         i);
                retVal = FAILURE;
                break;
            }
        }
    }

    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: sampleVThreadCallBack
 -- Decription   : Once virtual thread is created this call back function is
 --                called. This is passed as argument when virtual thread is
 --                to be created.
 -- Arguments    : None
 --                Data to be used for processing after virtual thread is
 --                created is stored in global context.
------------------------------------------------------------------------------*/
void sampleVThreadCallBack(void)
{
    tSampleReturn retVal = SUCCESS;
    tSampleSeqInfo  *pSeqInfo = NULL;
    eU8             *pData = NULL;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    pSeqInfo = glbCtx.pMsgProc;
    MY_TRACE(1, "Virtual Thread Created and processing for Thread %d\n",
             pSeqInfo->threadId);
    pData = malloc(50);

    if (NULL != pData)
    {
        pData[12] = 20;

        while (D != pSeqInfo->commandType)
        {
            /* Thread has completed one schedule and going to sleep */
            MY_TRACE(1, "Virtual Thread %d going to Sleep\n",
                     pSeqInfo->threadId);

            if (FAILURE == vThreadSleep(&(glbCtx.vThreadCtx),
                                        &(glbCtx.vThInfo[pSeqInfo->threadId])))
            {
                MY_TRACE(1, "Virtual Thread Sleep is failure\n");
                retVal = FAILURE;
            }

            /* Thread will come to this position only at Thread wakeup */
            pSeqInfo = glbCtx.pMsgProc;
            MY_TRACE(1, "Virtual Thread %d Woken Up with Command Type %d\n",
                     pSeqInfo->threadId, pSeqInfo->commandType);
            (pData[12])++;
        }

        MY_TRACE(1, "About to Destroy Thread %d at Thread completion with value of pData as %d\n",
                 pSeqInfo->threadId, pData[12]);
        free(pData);

        MY_TRACE(1, "test");
    }

    if (FAILURE == vThreadDestroy(&(glbCtx.vThInfo[pSeqInfo->threadId])))
    {
        MY_TRACE(1, "Virtual Thread Destroying is failure\n");
        retVal = FAILURE;
    }

    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadBaseInit
 -- Decription   : This function gets the outer stack context at the time of
 --                initialization.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadBaseInit(tVthContext *pVthctx)
{
    tSampleReturn retVal = SUCCESS;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    pVthctx->outerUctx.uc_link = 0;
    pVthctx->outerUctx.uc_stack.ss_size = MAX_VIRTUAL_THREAD_STACK_SIZE;
    pVthctx->tmpOuterUctx.uc_link = 0;
    pVthctx->tmpOuterUctx.uc_stack.ss_size = MAX_VIRTUAL_THREAD_STACK_SIZE;

	/*保存当前上下文*/
    getcontext(&(pVthctx->outerUctx));
    getcontext(&(pVthctx->tmpOuterUctx));
    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadCreate
 -- Decription   : This function spawns a new virtual thread.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
 --                This will be updated for the stack context in this function.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadCreate(tVthContext *pVthctx, tVThread *pVth,
                                   tVThCallBackFP funcPtr)
{
    tSampleReturn retVal = SUCCESS;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
	MY_TRACE(1, "glbCtx.noOfVitualThreadCreated=%d\n",glbCtx.noOfVitualThreadCreated);

    if (MAX_SIMULTANEOUS_TRANS > glbCtx.noOfVitualThreadCreated)
    {
        memset(&(pVth->uctx), 0 , sizeof(pVth->uctx));
        getcontext(&(pVth->uctx));

        pVth->uctx.uc_link = &(pVthctx->outerUctx);
        pVth->uctx.uc_stack.ss_sp = malloc(MAX_VIRTUAL_THREAD_STACK_SIZE);

        if (pVth->uctx.uc_stack.ss_sp == NULL)
        {
            MY_TRACE(1, "No memory for the thread pool");
            retVal = FAILURE;
        }
        else
        {
            glbCtx.noOfVitualThreadCreated++;
            pVth->uctx.uc_stack.ss_size = MAX_VIRTUAL_THREAD_STACK_SIZE;
            makecontext(&pVth->uctx, (void *)funcPtr, 0);
			MY_TRACE(1, "Tags");
            swapcontext(&pVthctx->tmpOuterUctx, &pVth->uctx);
        }
    }
    else
    {
        MY_TRACE(1, "Max No. of Virtual Threads Created = %d\n",
                 glbCtx.noOfVitualThreadCreated);
        retVal = FAILURE;
    }

    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadDestroy
 -- Decription   : This function is called to destroy a virtual thread. It
 --                needs to be called for cleanup even at end of normal
 --                virtual thread execution completion.
 -- Arguments    :
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
 --                When thread is destroyed, the memory for the stack context
 --                is deleted.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadDestroy(tVThread *pVth)
{
    tSampleReturn retVal = SUCCESS;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    glbCtx.noOfVitualThreadCreated--;
    MY_TRACE(1, "test");
    free (pVth->uctx.uc_stack.ss_sp);
   // MY_TRACE(1, "test");
    //pVth->uctx.uc_stack.ss_sp = NULL;
   // MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadSleep
 -- Decription   : This function swaps the stack context from the current
 --                virtual thread to the stack context for the base program.
 --                This is used to Relinquish Control from virtual thread to
 --                base program, so that next command can be processed while
 --                this virtual thread still awaits info from base program.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadSleep(tVthContext *pVthctx, tVThread *pVth)
{
    tSampleReturn retVal = SUCCESS;
    eS32 intRetVal = 0;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    intRetVal = swapcontext(&(pVth->uctx), &(pVthctx->outerUctx));
    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadWakeup
 -- Decription   : When a command comes for a virtual thread which is already
 --                sleeping, then this function is called to again swap the
 --                stack context from base program stack to the specific
 --                virtual thread stack context. This would eventually Wakeup
 --                the Virtual Thread
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
 -- pVth         : The specific virtual thread context containing info for
 --                stack comtext for that virtual thread is stored in this.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadWakeup(tVthContext *pVthctx, tVThread *pVth)
{
    tSampleReturn retVal = SUCCESS;
    eS32 intRetVal = 0;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    intRetVal = swapcontext(&(pVthctx->tmpOuterUctx), &(pVth->uctx));
    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}

/*-----------------------------------------------------------------------------
 -- Function Name: vThreadBaseUpdate
 -- Decription   : After every virtual thread processing is over, this function
 --                is called to Store the base Stack context again.
 -- Arguments    :
 -- pVThreadCtx  : The common thread context is passed to this function which
 --                is used for updating the the base stack contexts.
------------------------------------------------------------------------------*/
static tSampleReturn vThreadBaseUpdate(tVthContext *pVthctx)
{
    tSampleReturn retVal = SUCCESS;
    MY_TRACE(1, "Function %s Entered\n", __FUNCTION__);
    getcontext(&pVthctx->outerUctx);
    retVal = SUCCESS;
    MY_TRACE(1, "Function %s Returns with %d\n", __FUNCTION__, retVal);
    return retVal;
}
