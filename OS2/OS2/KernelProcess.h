#pragma once
//class System;
class KernelSystem;
#include "vm_declarations.h"
#include "PmtDeclarations.h"
//#include "KernelSystem.h"
class KernelProcess {
public:
	KernelProcess(ProcessId pid);
	~KernelProcess();
	ProcessId getProcessId() const;
	Status createSegment(VirtualAddress adr, PageNum segmentSize,
		AccessType flags);
	Status loadSegment(VirtualAddress adr, PageNum segmentSize,
		AccessType flags, void* content);
	Status deleteSegment(VirtualAddress startAdr);
	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress adr);
	int checkSpace(unsigned * pmtStart, unsigned long num, int pmt1, int pmt2);
	int RweToInt(AccessType type);
	Descriptor* replaceWith();
	void addToDescriptors(Descriptor* desc);
	Descriptor* getDescriptor(VirtualAddress adr);
	int initLoadSeg(Descriptor* desc, unsigned * content);
	unsigned * findFreePage();
	void getUlazi(VirtualAddress adr, int* ulazPmt1, int* ulazPmt2);

private:
	
	int br = 0;
	elemDesc* descriptors;
	PhysicalAddress pmt;
	ProcessId pid;
	KernelSystem* mySystem;
	friend class System;
	friend class KernelSystem;
	friend class Process;
	void setPmt(PhysicalAddress pmt);
};
