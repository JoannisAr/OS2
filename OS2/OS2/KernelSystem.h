#pragma once
#include "vm_declarations.h"
#include "PmtDeclarations.h"
#include <unordered_map>
#include "Process.h"
#include "part.h"
#include<mutex>
//class Partition;
//class Process;
class KernelSystem {
private:
	friend class KernelProcess;
	struct elem {
		int val;
		elem* next;
		elem(int v) {
			val = v;
			next = nullptr;
		}

	};
	


	PhysicalAddress processFree;
	PhysicalAddress pmtFree;
	Partition* partition;
	elem* freeCluster = nullptr;
	int numOfClusters;
	unsigned id = 0;
	std::mutex mtx;
	std::mutex resourcesMtx;
	

public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition);
	~KernelSystem();
	Process* createProcess();
	Status access(ProcessId pid, VirtualAddress address, AccessType type);
	Time periodicJob();
	void initialize(PhysicalAddress a, PageNum num);
	void initList(elem* first);
	PhysicalAddress takePmtFree();
	void returnPmt(PhysicalAddress pmt);
	int getFreeCluster();
	void returnCluster(int val);
	PhysicalAddress getProcessFree();
	void returnProcess(PhysicalAddress p);
	void removeFromMaps(unsigned id);
	
private:
	friend class KernelProcess;
	std::unordered_map<ProcessId, PhysicalAddress> processPmt;
	std::unordered_map<ProcessId, KernelProcess*> processes;
	
	

	
};
