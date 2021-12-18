#include "sheduler.h"
#include "../../pool.h"
#include "../../klist.h"
#include <memory/paging.h>
#include <HAL/PIT.h>
#include <video/SimpleTextIo.h>
#include <memory/MemoryOperations.h>
#include <memory/physical_allocator.h>
#include <HAL/X64/pcr.h>
#include <HAL/APIC/APIC.h>

KLINKED_LIST PspThreadList;
KLINKED_LIST PspProcessList;

KSPIN_LOCK DebugFramebufferSpinlock;
UINT64* DebugFramebufferPosition;
SIZE_T DebugFramebufferLen = 16;

VOID AsmLoop();

__declspec(noreturn) VOID PspTestAsmUser();
__declspec(noreturn) VOID PspTestAsmUserEnd();
__declspec(noreturn) VOID PspTestAsmUser2();

const CHAR cPspThrePoolTag[4] = "Thre";
const CHAR cPspProcPoolTag[4] = "Proc";

VOID HalpUpdateThreadKernelStack(PVOID kernelStack);

PVOID PspCreateAddressSpace()
{
	ULONG_PTR physicalPML4 = (ULONG_PTR) InternalAllocatePhysicalPage();
	PVOID virtualPML4 = PagingAllocatePageWithPhysicalAddress(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END, PAGE_PRESENT | PAGE_WRITE, (PVOID) physicalPML4);

	((UINT64*) virtualPML4)[PML4EntryForRecursivePaging] = physicalPML4 | PAGE_PRESENT | PAGE_WRITE;
	((UINT64*) virtualPML4)[KERNEL_DESIRED_PML4_ENTRY] = ((UINT64*) GetCR3())[KERNEL_DESIRED_PML4_ENTRY];

	return (PVOID)physicalPML4;
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
	PEPROCESS process1, process2;
	PETHREAD thread, thread2;
	NTSTATUS status;
	ULONG_PTR kernelStack, userStack;
	ULONG_PTR kernelStack2, userStack2;
	PVOID code;
	PKTASK_STATE_USER taskState, taskState2;

	PrintT("%i(%i)\n",__LINE__,(UINT64)ApicGetCurrentLapicId());

	status = PspCreateProcessInternal(&process1);
	if (status)
	{
		return status;
	}

	PrintT("%i(%i)\n",__LINE__,(UINT64)ApicGetCurrentLapicId());

	status = PspCreateThreadInternal(&thread, process1);
	if (status)
	{
		PspDestroyProcessInternal(process1);
		return status;
	}

	PrintT("%i(%i)\n",__LINE__,(UINT64)ApicGetCurrentLapicId());

	status = PspCreateProcessInternal(&process2);
	if (status)
	{
		PspDestoryThreadInternal(thread);
		PspDestroyProcessInternal(process1);
		return status;
	}

	PrintT("%i(%i)\n",__LINE__,(UINT64)ApicGetCurrentLapicId());

	status = PspCreateThreadInternal(&thread2, process2);
	if (status)
	{
		PspDestoryThreadInternal(thread);
		PspDestroyProcessInternal(process1);
		PspDestroyProcessInternal(process2);
		return status;
	}

	PrintT("%i(%i)\n",__LINE__,(UINT64)ApicGetCurrentLapicId());

	DebugFramebufferPosition = gFramebuffer + gPixelsPerScanline * (DebugFramebufferLen + 2);
	// ChangeAddressSpace(process->Pcb.AddressSpacePointer);

	kernelStack = (ULONG_PTR) PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	kernelStack = (ULONG_PTR) (kernelStack + PAGE_SIZE_SMALL - sizeof(*taskState));

	kernelStack2 = (ULONG_PTR) PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	kernelStack2 = (ULONG_PTR) (kernelStack2 + PAGE_SIZE_SMALL - sizeof(*taskState2));

	SetCR3((UINT64) process1->Pcb.AddressSpacePhysicalPointer);
	code = PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END);
	MemCopy(code, PspTestAsmUser, ((ULONG_PTR) PspTestAsmUserEnd) - ((ULONG_PTR) PspTestAsmUser));
	userStack = (ULONG_PTR) ((ULONG_PTR) PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END) + PAGE_SIZE_SMALL);

	SetCR3((UINT64) process2->Pcb.AddressSpacePhysicalPointer);
	code = PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END);
	MemCopy(code, PspTestAsmUser, ((ULONG_PTR) PspTestAsmUserEnd) - ((ULONG_PTR) PspTestAsmUser));
	userStack2 = (ULONG_PTR) ((ULONG_PTR) PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END) + PAGE_SIZE_SMALL);

	thread->Tcb.KernelStackPointer = (PVOID)kernelStack;
	taskState = (PKTASK_STATE_USER)kernelStack;
	taskState->Rip = (UINT64) code;
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

	thread2->Tcb.KernelStackPointer = (PVOID)kernelStack2;
	taskState2 = (PKTASK_STATE_USER)kernelStack2;
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
	
	HalpUpdateThreadKernelStack((PVOID)((ULONG_PTR) kernelStack + sizeof(*taskState)));

	KeGetPcr()->Prcb->CurrentThread = &thread->Tcb;
	KeGetPcr()->Prcb->NextThread = &thread2->Tcb;
	
	PrintT("\n%i %X %X %X %X\n", (UINT64)ApicGetCurrentLapicId(),thread, &thread->Tcb, thread2, &thread2->Tcb);
	
	SetCR3(process1->Pcb.AddressSpacePhysicalPointer);
	PspSwitchContextTo64(thread->Tcb.KernelStackPointer);
}

KPROCESS PspCreatePcb()
{
	KPROCESS result;

	result.AffinityMask = 0xFFFFFFFFFFFFFFFFULL;
	KeInitializeSpinLock(&result.ProcessLock);
	result.ThreadList.Next = (PKLINKED_LIST)NULL;

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

	PrintT("Post allocation\n");

	if (result == (PEPROCESS)NULL)
		return STATUS_NO_MEMORY;

	*output = result;
	result->ThreadList.Next = (PKLINKED_LIST)NULL;
	result->Pcb = PspCreatePcb();
	result->Pcb.AddressSpacePhysicalPointer = PspCreateAddressSpace();

	return STATUS_SUCCESS;
}

NTSTATUS PspCreateThreadInternal(PETHREAD* output, PEPROCESS parent)
{ 
	PETHREAD result = ExAllocatePoolZero(NonPagedPool, sizeof(ETHREAD), POOL_TAG_FROM_STR(cPspThrePoolTag));

	if (result == (PETHREAD)NULL)
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

KSPIN_LOCK PrintLock;

ULONG_PTR PspScheduleThread(ULONG_PTR stack)
{
	PKPCR pcr = KeGetPcr();
	PKTHREAD temp = pcr->Prcb->NextThread;
	UINT32 i, j;
	UINT32* ProcessorSpecificFramebuffer;
	UINT8 ProcessorId;
	KIRQL Irql;

	KeAcquireSpinLock(&PrintLock, &Irql);

	PrintT("Doing switch\n");

	KeReleaseSpinLock(&PrintLock, Irql);

	pcr->Prcb->NextThread = pcr->Prcb->CurrentThread;
	pcr->Prcb->CurrentThread = temp;

	ProcessorId = ApicGetCurrentLapicId();
	ProcessorSpecificFramebuffer = DebugFramebufferPosition + (DebugFramebufferLen + 2) * gPixelsPerScanline * ProcessorId;

	for (i = 0; i < DebugFramebufferLen; i++)
	{
		for (j = 0; j < gPixelsPerScanline; j++)
		{
			ProcessorSpecificFramebuffer[j] = pcr->Prcb->CurrentThread;
		}

		ProcessorSpecificFramebuffer += gPixelsPerScanline;
	}

	if (pcr->Prcb->CurrentThread->Process != pcr->Prcb->NextThread->Process)
	{
		SetCR3((UINT64)pcr->Prcb->CurrentThread->Process->AddressSpacePhysicalPointer);
	}

	return (ULONG_PTR)pcr->Prcb->CurrentThread->KernelStackPointer;
}