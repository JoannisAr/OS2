
#include"part.h"

#include "KernelSystem.h"
#include "PmtDeclarations.h"
#include <iostream>


	KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition) {
		mtx.lock();
		processFree = processVMSpace;
		pmtFree = pmtSpace;
		freeCluster = nullptr;
		numOfClusters = partition->getNumOfClusters();
		this->partition = partition;
		//std::cout << sizeof(unsigned) << std::endl;
		//std::cout << sizeof(unsigned*) << std::endl;
		this->initialize(processVMSpace, processVMSpaceSize);
		this->initialize(pmtSpace, pmtSpaceSize);
		this->initList(freeCluster);
		mtx.unlock();
	}

	KernelSystem::~KernelSystem(){
		mtx.lock();
		int i = 0;
		for (auto it = processes.begin(i); it != processes.end(i); ++it) {
			KernelProcess* p = (KernelProcess*)it->second;
			delete p;
		}
		elem* tek = freeCluster;
		while (tek) {
			elem* tmp = tek;
			tek = tek->next;
			delete tmp;
		}
		mtx.unlock();
	}

	Process* KernelSystem::createProcess() {
		mtx.lock();
		ProcessId pid = (ProcessId)id++;
		Process* p = new Process(pid);
		p->pProcess->mySystem = this;
		PhysicalAddress pmt = takePmtFree();
		p->setPmt(pmt);
		
		/*if (pmtFree != nullptr) {
			unsigned next = *((unsigned*)pmtFree);
			if (next == 0)pmtFree = nullptr;
			else pmtFree = (unsigned*)next;*/
			//anuliraj prvo polje novododeljene str za pmt
			unsigned init = 0;
			*((unsigned*)pmt) = init;
		//}
		//dodaj u listu svih procesa
		processPmt.insert(std::pair<ProcessId, PhysicalAddress>(pid, pmt));
		processes.insert(std::pair<ProcessId, KernelProcess*>(pid, p->pProcess));

		mtx.unlock();
		return p;
	}

	PhysicalAddress KernelSystem::takePmtFree() {
		resourcesMtx.lock();
		unsigned*ptr = (unsigned*)pmtFree;
		if (ptr == nullptr) { resourcesMtx.unlock(); return nullptr; }
		unsigned*next = (unsigned*)(*(ptr));
		pmtFree = next;
		*ptr = 0;
		std::cout << "Uzimam pmt sa adr " << ptr << std::endl;
		resourcesMtx.unlock();
		return ptr;
	}
	void KernelSystem::returnPmt(PhysicalAddress pmt) {
		resourcesMtx.lock();
		unsigned* ptr = (unsigned*)pmt, nulica=0;
		if (pmtFree == nullptr) {
			*ptr = nulica;
		}
		else *ptr = (unsigned)pmtFree;

		pmtFree = (unsigned*)pmt;
		resourcesMtx.unlock();
	}

	int KernelSystem::getFreeCluster(){
		resourcesMtx.lock();
		if (freeCluster == nullptr) { resourcesMtx.unlock(); return -1; }
		int val = freeCluster->val;
		freeCluster = freeCluster->next;
		resourcesMtx.unlock();
		return val;
	}

	void KernelSystem::returnCluster(int val){
		resourcesMtx.lock();
		elem* e = new elem(val);
		e->next = freeCluster;
		freeCluster = e;
		resourcesMtx.unlock();
	}

	PhysicalAddress KernelSystem::getProcessFree(){
		resourcesMtx.lock();
		unsigned*ptr = (unsigned*)processFree;
		if (ptr == nullptr) { resourcesMtx.unlock(); return nullptr; }

		unsigned* next = (unsigned*)(*(ptr));
		processFree = next;
		*ptr = 0;
		resourcesMtx.unlock();
		return ptr;
	}

	void KernelSystem::returnProcess(PhysicalAddress p){
		resourcesMtx.lock();
		unsigned* ptr = (unsigned*)p, nulica = 0;
		if (processFree == nullptr) {
			*ptr = nulica;
		}
		else *ptr = (unsigned)processFree;

		processFree = p;
		resourcesMtx.unlock();
	}


	void KernelSystem::initialize(PhysicalAddress a, PageNum num) {
		unsigned* next, *adr=(unsigned*)a;

		for (int p = 0; p < num; p++) {
			for (int j = 0; j < 256; j++) {
				*(adr +256*p + j) = 0;
			}
		}

		adr = (unsigned*)a;
		//std::cout << " --------------PMTFREE:---------------" << std::endl;
		for (int i = 0; i < num; i++) {
			if (i == num - 1) {
				next = 0;
			}
			else {
				unsigned x = 256;
				next = adr + x; //  1024/4
			}
			*adr = (unsigned)next;
			//std::cout << adr << " sadrzaj: "<<std::hex<<*adr<<std::endl;
			adr=next;
		}
	}

	void KernelSystem::initList(elem* first) {
		elem* tek = nullptr;
		for (int i = 0; i < numOfClusters; i++) {
			elem* e = new elem(i);
			if (first == nullptr)first = e;
			else tek->next = e;
			tek = e;
		}
	}
	Status KernelSystem::access(ProcessId pid, VirtualAddress adr, AccessType type) {
		mtx.lock();
		unsigned long* pmt = (unsigned long*)processPmt[pid];
		unsigned long maskPmt1 = 0x00ff0000,
			maskPmt2 = 0x0000fc00,
			maskOff = 0x000003ff;
		unsigned long pmt1 = (unsigned long)adr & maskPmt1,
			pmt2 = (unsigned long)adr & maskPmt2,
			off = (unsigned long)adr & maskOff;
		pmt1 = pmt1 >> 16;
		pmt2 = pmt2 >> 10;

		unsigned tmp = *(pmt + pmt1);
		unsigned* pmtSec = (unsigned *)tmp;
		//ako uopste ne postoji pmt 2. nivoa na tom ulazu
		if (pmtSec == nullptr) { mtx.unlock();  return Status::TRAP; }


		Descriptor* d = (Descriptor*)(pmtSec + 4 * pmt2);
		Descriptor desc = (Descriptor)(*d);
		if (desc.used == 0) { mtx.unlock(); return Status::TRAP; }
		if (desc.valid == 0 || desc.uninit == 1) { mtx.unlock(); return Status::PAGE_FAULT; }
		desc.referenced = 1;

		//provera prava pristupa
		if (type == AccessType::READ && (desc.rwe == 0 || desc.rwe == 2) ) { mtx.unlock(); return Status::OK; }
		if (type == AccessType::WRITE && (desc.rwe == 1 || desc.rwe==2 )) {
			desc.dirty = 1; mtx.unlock(); return Status::OK;
		}
		if (type == AccessType::READ_WRITE && (desc.rwe == 2)) { desc.dirty = 1; mtx.unlock(); return Status::OK; }
		if (type == AccessType::EXECUTE && desc.rwe == 3) { mtx.unlock(); return Status::OK; }
		mtx.unlock();
		return Status::TRAP;


	}

	Time KernelSystem::periodicJob() {
		mtx.lock();
		int i=0;
		//kroz sve procese u sistemu
		for (auto it = processes.begin(i); it != processes.end(i); ++it) {
			KernelProcess* p = (KernelProcess*)it->second;
			elemDesc* d = p->descriptors;
			//kroz sve postojece deskriptore tog procesa
			while (d != nullptr) {
				d->d->history=d->d->history << 1;
				unsigned ref = 0;
				if (d->d->referenced == 1)ref++;
				d->d->history | ref;
				d->d->referenced = 0;
				d = d->next;
			}
		}
		mtx.unlock();
		return 100000;
	}

	void KernelSystem::removeFromMaps(unsigned id) {
		processes.erase(id);
		processPmt.erase(id);
	}

	
