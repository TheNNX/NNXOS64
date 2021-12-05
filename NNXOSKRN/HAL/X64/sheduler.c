#include "sheduler.h"
#include "../../pool.h"
#include "../../klist.h"
#include <memory/paging.h>
#include <HAL/PIT.h>
#include <video/SimpleTextIo.h>
#include <memory/MemoryOperations.h>
#include <HAL/X64/pcr.h>

KLINKED_LIST PspThreadList;
KLINKED_LIST PspProcessList;

VOID AsmLoop();

__declspec(noreturn) VOID PspTestAsmUser();
__declspec(noreturn) VOID PspTestAsmUserEnd();
__declspec(noreturn) VOID PspTestAsmUser2();

const CHAR cPspThrePoolTag[4] = "Thre";
const CHAR cPspProcPoolTag[4] = "Proc";

PVOID PspCreateAddressSpace()
{
	return GetCR3();
}

VOID DebugFunction()
{
	while (true)
	{
		PitUniprocessorPollSleepMs(1000);
		PrintT("A");
	}
}

NTSTATUS PspDebugTest()
{
	BOOL kernelMode = FALSE;
	PEPROCESS process;
	PETHREAD thread, thread2;
	NTSTATUS status;
	UINT64* kernelStack, userStack;
	UINT64* kernelStack2, userStack2;
	PVOID code;
	PKTASK_STATE_USER taskState, taskState2;

	if (status = PspCreateProcessInternal(&process))
	{
		return status;
	}

	if (status = PspCreateThreadInternal(&thread, process))
	{
		PspDestroyProcessInternal(process);
		return status;
	}

	if (status = PspCreateThreadInternal(&thread2, process))
	{
		PspDestoryThreadInternal(thread);
		PspDestroyProcessInternal(process);
		return status;
	}

	process->Pcb.AddressSpacePointer = PspCreateAddressSpace();
	// ChangeAddressSpace(process->Pcb.AddressSpacePointer);

	kernelStack = PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	kernelStack = (UINT64*) (((ULONG_PTR) kernelStack) + PAGE_SIZE_SMALL - sizeof(*taskState));

	kernelStack2 = PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	kernelStack2 = (UINT64*) (((ULONG_PTR) kernelStack2) + PAGE_SIZE_SMALL - sizeof(*taskState2));

	userStack = (UINT64*) (((ULONG_PTR) PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END)) + PAGE_SIZE_SMALL);

	userStack2 = (UINT64*) (((ULONG_PTR) PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END)) + PAGE_SIZE_SMALL);

	code = PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END);
	MemCopy(code, PspTestAsmUser, ((ULONG_PTR) PspTestAsmUserEnd) - ((ULONG_PTR) PspTestAsmUser));

	thread->Tcb.KernelStackPointer = kernelStack;
	taskState = kernelStack;
	taskState->Rip = code;
	taskState->Cs = kernelMode ? 0x08 : 0x1B;
	taskState->Ds = kernelMode ? 0x10 : 0x23;
	taskState->Es = kernelMode ? 0x10 : 0x23;
	taskState->Fs = kernelMode ? 0x10 : 0x23;
	taskState->Gs = kernelMode ? 0x10 : 0x23;
	taskState->Rflags = 0x00000286;
	taskState->Rax = 0xDEADBEEF;
	taskState->Rbx = 0xBBBBBBBB;
	taskState->Rcx = 0xCCCCCCCC;
	taskState->Rsp = userStack;
	taskState->Ss = kernelMode ? 0x10 : 0x23;

	thread2->Tcb.KernelStackPointer = kernelStack2;
	taskState2 = kernelStack2;
	taskState2->Rip = (ULONG_PTR)code + (ULONG_PTR)PspTestAsmUser2 - (ULONG_PTR)PspTestAsmUser;
	taskState2->Cs = kernelMode ? 0x08 : 0x1B;
	taskState2->Ds = kernelMode ? 0x10 : 0x23;
	taskState2->Es = kernelMode ? 0x10 : 0x23;
	taskState2->Fs = kernelMode ? 0x10 : 0x23;
	taskState2->Gs = kernelMode ? 0x10 : 0x23;
	taskState2->Rflags = 0x00000286;
	taskState2->Rax = 0xDEADBEEF;
	taskState2->Rax = 0xDEADBEEF;
	taskState2->Rbx = 0xBBBBBBBB;
	taskState2->Rcx = 0xCCCCCCCC;
	taskState2->Rsp = userStack2;
	taskState2->Ss = kernelMode ? 0x10 : 0x23;

	HalpUpdateThreadKernelStack(((ULONG_PTR) kernelStack) + sizeof(*taskState));
	KeGetPcr()->Prcb->CurrentThread = thread;
	KeGetPcr()->Prcb->NextThread = thread2;

	// AsmLoop();

	PspSwitchContextTo64(kernelStack);
}

KPROCESS PspCreatePcb()
{
	KPROCESS result;

	result.AffinityMask = 0xFFFFFFFFFFFFFFFFULL;
	KeInitializeSpinLock(&result.ProcessLock);
	result.ThreadList.Next = NULL;

	return result;
}

KTHREAD PspCreateTcb(PKPROCESS process)
{
	KTHREAD result;

	result.Process = process;
	KeInitializeSpinLock(&result.ThreadLock);

	return result;
}

NTSTATUS PspCreateProcessInternal(PEPROCESS* output)
{ 
	PEPROCESS result = ExAllocatePoolZero(NonPagedPool, sizeof(EPROCESS), POOL_TAG_FROM_STR(cPspProcPoolTag));

	if (result == NULL)
		return STATUS_NO_MEMORY;

	*output = result;
	result->ThreadList.Next = NULL;
	result->Pcb = PspCreatePcb();

	return STATUS_SUCCESS;
}

NTSTATUS PspCreateThreadInternal(PETHREAD* output, PEPROCESS parent)
{ 
	PETHREAD result = ExAllocatePoolZero(NonPagedPool, sizeof(ETHREAD), POOL_TAG_FROM_STR(cPspThrePoolTag));

	if (result == NULL)
		return STATUS_NO_MEMORY;

	*output = result;

	result->Process = parent;
	parent->Pcb.NumberOfThreads++;
	AppendList(&parent->ThreadList, result);
	result->Tcb = PspCreateTcb(&parent->Pcb);
	AppendList(&parent->Pcb.ThreadList, &result->Tcb);
	return STATUS_SUCCESS;
}

NTSTATUS PspDestroyProcessInternal(PEPROCESS process)
{
	ClearListAndDestroyValues(&process->ThreadList, PspDestoryThreadInternal);
	ClearList(&process->Pcb.ThreadList);
	ExFreePool(process);

	return STATUS_SUCCESS;
}

NTSTATUS PspDestoryThreadInternal(PETHREAD thread)
{
	if(FindInList(&thread->Process->ThreadList, thread))
	   RemoveFromList(&thread->Process->ThreadList, thread);
	RemoveFromList(&thread->Process->Pcb.ThreadList, &thread->Tcb);
	ExFreePool(thread);

	return STATUS_SUCCESS;
}


ULONG_PTR PspScheduleThread(ULONG_PTR stack)
{
	PKPCR pcr = KeGetPcr();
	PKTHREAD temp = pcr->Prcb->NextThread;
	pcr->Prcb->NextThread = pcr->Prcb->CurrentThread;
	pcr->Prcb->CurrentThread = temp;
	PrintT("\no%X (i%X) n%X\n", pcr->Prcb->CurrentThread->KernelStackPointer, pcr->Prcb->NextThread->KernelStackPointer, pcr->Prcb->NextThread->KernelStackPointer);
	return (ULONG_PTR)pcr->Prcb->CurrentThread->KernelStackPointer;
}