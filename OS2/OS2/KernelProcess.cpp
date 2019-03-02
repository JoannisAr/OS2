
#include "KernelProcess.h"
#include"KernelSystem.h"
#include"part.h"
#include<iostream>
KernelProcess::KernelProcess(ProcessId pid) {
	this->pid = pid;
	descriptors = nullptr;
	pmt = nullptr;


	}
KernelProcess::~KernelProcess(){
	unsigned *ptr = (unsigned *)pmt, nula=0;
	for (int i = 0; i < 256; i++) {
		if (*(ptr + i) != 0) mySystem->returnPmt((unsigned *)(*(ptr + i)));
		*(ptr + i) = nula;
	}
	mySystem->returnPmt(pmt);

	elemDesc* tek = descriptors;
	while (tek) {
		//ne mora jer se u desc nalaze samo oni koji su alocirali str ali za svaki slucaj :)
		if (tek->d->used && !tek->d->uninit) {
			if (tek->d->valid) mySystem->returnProcess(tek->d->adr);
			else mySystem->returnCluster(tek->d->clusterNo);
		}
		elemDesc* tmp = tek;
		tek = tek->next;
		delete tmp;
	}

	mySystem->removeFromMaps(pid);
}
ProcessId KernelProcess::getProcessId() const{

	return pid;
}
Status KernelProcess::createSegment(VirtualAddress adr, PageNum segmentSize, AccessType flags){
	mySystem->mtx.lock();
	int seg = br++;
	unsigned long maskPmt1 = 0x00ff0000,
		maskPmt2 = 0x0000fc00,
		maskOff = 0x000003ff;
	unsigned long pmt1 = (unsigned long)adr & maskPmt1,
		pmt2 = (unsigned long)adr & maskPmt2,
		off = (unsigned long)adr & maskOff;
	pmt1=pmt1 >> 16;
	pmt2=pmt2 >> 10;
	if (off != 0) { mySystem->mtx.unlock(); return Status::TRAP; }	//ako adr nije poravnata na pocetak str
	unsigned long num = segmentSize;
	unsigned *pmtStart = (unsigned *)pmt;
	//pmtStart = pmtStart + pmt1;

	if (!checkSpace(pmtStart+pmt1, num, pmt1, pmt2)) { mySystem->mtx.unlock(); return Status::TRAP; }

	unsigned  tmp = *(pmtStart+pmt1);
	unsigned * pmtSec = (unsigned *)tmp;
	int segStart = 1;

	while (num > 0) {
		
		if (pmt1 >= 256)return Status::TRAP;
		//1)- ULAZ U PMT2 NE POSTOJI
		if (pmtSec == nullptr) {
			//uzmi novu str iz pmtFRee 
			pmtSec = (unsigned *)(mySystem->takePmtFree());
			if (pmtSec == nullptr) { mySystem->mtx.unlock(); return Status::TRAP; }
			*(pmtStart+pmt1)= (unsigned)pmtSec;

			//popunjavanje deskriptorima
			for (int i = 0; i < 64; i++) {
				Descriptor* desc = (Descriptor*)(pmtSec + 4 * i);
				//std::cout << desc << " " << i << " pmtSec je " << pmtSec << std::endl;
				//std::cout<<"Sizeof *desc= " << sizeof(*desc) << std::endl<<std::endl;
				//popuni sa unused deskriptorom
				if (i < pmt2 || num <= 0) {
					desc->used = 0;
					desc->adr = 0;
				}
				else {
					desc->used = 1;
					desc->dirty = 1;
					desc->history = 0;
					desc->referenced = 0;
					desc->adr = 0;
					desc->numSeg = seg;
					desc->rwe = RweToInt(flags);
					if (segStart) { desc->segmStart = 1; segStart = 0; }
					else desc->segmStart = 0;
					desc->valid = 0;
					desc->uninit = 1;
					//addToDescriptors(desc);
					
					num--;
				}
			}
			pmt2 = 0;
			pmt1++;
			pmtSec= (unsigned*)(*(pmtStart + pmt1));
		}

		//2)- ULAZ U PMT2 VEC POSTOJI
		else {
			if (pmt2 >= 64) {
				pmt2 = 0;
				pmt1++;
				pmtSec = (unsigned*)(*(pmtStart + pmt1));
			}
			else {
				Descriptor* desc = (Descriptor*)(pmtSec + 4 * pmt2);
				desc->used = 1;
				desc->dirty = 1;
				desc->history = 0;
				desc->referenced = 0;
				desc->adr = 0;
				desc->numSeg = seg;
				desc->rwe = RweToInt(flags);
				if (segStart) { desc->segmStart = 1; segStart = 0; }
				else desc->segmStart = 0;
				desc->valid = 0;
				desc->uninit = 1;
				num--;
				pmt2++;
			}
		}
	
	}
	mySystem->mtx.unlock();
	return Status::OK;
}

int KernelProcess::RweToInt(AccessType type) {
	if (type == AccessType::READ)return 0;
	if (type == AccessType::WRITE)return 1;
	if (type == AccessType::READ_WRITE)return 2;
	if (type == AccessType::EXECUTE)return 3;
}

int KernelProcess::checkSpace(unsigned * pmtStart, unsigned long num, int pmt1, int pmt2) {
	while (num > 0) {
		if (pmt1 > 255)return 0;

		//pmt2 za taj ulaz nije alociran
		if (*pmtStart == 0) {
			//num -= 64;
			if (num < 64)num = 0;
			else num = num - 64;
			if (num <= 0)return 1;
		}

		//pmt2 alociran
		else {
			unsigned  tmp = *pmtStart;
			unsigned * pmtSec = (unsigned *)tmp; //ptr na pmt 2. nivoa a pmt2 je ulaz u pmt 2. nivoa
			
			while (pmt2 < 64 && num > 0) {
				Descriptor* d = (Descriptor*)(pmtSec + 4 * pmt2);
				if (d->used == 1)return 0;
				pmt2++;
				num--;
			}
		}
		pmt2 = 0;
		pmtStart++;
		pmt1++;
	}
	return 1;
}

void KernelProcess::addToDescriptors(Descriptor* desc) {
	elemDesc* elem = new elemDesc(desc);
	if (descriptors != nullptr)elem->next = descriptors;
	descriptors = elem;
}

Status KernelProcess::loadSegment(VirtualAddress adr, PageNum segmentSize, AccessType flags, void * content){
	mySystem->mtx.lock();
	int seg = br++;
	unsigned long maskPmt1 = 0x00ff0000,
		maskPmt2 = 0x0000fc00,
		maskOff = 0x000003ff;
	unsigned long pmt1 = (unsigned long)adr & maskPmt1,
		pmt2 = (unsigned long)adr & maskPmt2,
		off = (unsigned long)adr & maskOff;
	pmt1=pmt1 >> 16;
	pmt2=pmt2 >> 10;
	if (off != 0) { mySystem->mtx.unlock(); return Status::TRAP; }	//ako adr nije poravnata na pocetak str
	unsigned long num = segmentSize;
	unsigned *pmtStart = (unsigned *)pmt;	//pokazivac na celu pmt tog procesa
	//pmtStart = pmtStart + pmt1;

	if (!checkSpace(pmtStart+pmt1, num, pmt1, pmt2)) { mySystem->mtx.unlock(); return Status::TRAP; }

	unsigned  tmp = *(pmtStart+pmt1);
	unsigned * pmtSec = (unsigned *)tmp;
	int segStart = 1;
	while (num > 0) {
		if (pmt1 >= 256)return Status::TRAP;

		//1)- ULAZ U PMT2 NE POSTOJI
		if (pmtSec == nullptr) {
			//uzmi novu str iz pmtFRee 
			pmtSec = (unsigned *)(mySystem->takePmtFree());
			if (pmtSec == nullptr) { mySystem->mtx.unlock(); return Status::TRAP; }
			*(pmtStart+pmt1) = (unsigned)pmtSec;		//upisi novu adr pmt2 u njen ulaz u pmt1

			//popunjavanje deskriptorima
			for (int i = 0; i < 64; i++) {
				//std::cout << sizeof(Descriptor) << std::endl;
				
				Descriptor* desc = (Descriptor*)(pmtSec + 4 * i);
				//std::cout << desc<<" "<<i<<" pmtSec je "<<pmtSec<< std::endl;
				//std::cout << "Sizeof *desc= " << sizeof(*desc) << std::endl<<std::endl;
				//popuni sa unused deskriptorom
				if (i < pmt2 || num <= 0) {
					desc->used = 0;
				}
				else {
					desc->used = 1;
					desc->dirty = 1;
					desc->history = 0;
					desc->adr = 0;
					desc->numSeg = seg;
					desc->referenced = 0;
					desc->rwe = RweToInt(flags);
					if (segStart) { desc->segmStart = 1; segStart = 0; }
					else desc->segmStart = 0;
					desc->valid = 0;
					int t=initLoadSeg(desc, (unsigned *)content);	//pronalazak str i njeno inicializovanje
					if (!t) { mySystem->mtx.unlock(); return Status::TRAP; }
					unsigned* c = (unsigned*)content;
					c += 256;
					content = (void*)c;
					desc->uninit = 0;
					addToDescriptors(desc);
					if(num==1)std::cout << &desc << std::endl;
					num--;
				}
			}
			pmt2 = 0;
			pmt1++;
			pmtSec = (unsigned*)(*(pmtStart + pmt1));
		}

		//2)- ULAZ U PMT2 VEC POSTOJI
		else {
			if (pmt2 >= 64) {
				pmt2 = 0;
				pmt1++;
				pmtSec = (unsigned*)(*(pmtStart + pmt1));
			}
			else {
				Descriptor* desc = (Descriptor*)(pmtSec + 4 * pmt2);
				desc->used = 1;
				desc->dirty = 1;
				desc->history = 0;
				desc->referenced = 0;
				desc->adr = 0;
				desc->numSeg = seg;
				desc->rwe = RweToInt(flags);
				if (segStart) { desc->segmStart = 1; segStart = 0; }
				else desc->segmStart = 0;
				desc->valid = 0;
				int t = initLoadSeg(desc, (unsigned *)content);	//pronalazak str i njeno inicializovanje
				if (!t) { mySystem->mtx.unlock(); return Status::TRAP; }
				unsigned* c = (unsigned*)content;
				c += 256;
				content = (void*)c;
				desc->uninit = 0;
				num--;
				pmt2++;
			}
		}

	}
	mySystem->mtx.unlock();
	return Status::OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAdr){
	mySystem->mtx.lock();

	Descriptor* desc = getDescriptor(startAdr);
	if (!desc) { mySystem->mtx.unlock(); return Status::TRAP; }
	if (desc->segmStart == 0) { mySystem->mtx.unlock(); return Status::TRAP; }
	int seg = desc->numSeg, stop = 0;
	int pmt1, pmt2;
	getUlazi(startAdr, &pmt1, &pmt2);

	//vrati zauzete stranice i klastere
	while (!stop && desc->used == 1 && desc->numSeg == seg ) {
		if (desc->valid && desc->uninit==0) {
			mySystem->returnProcess(desc->adr);
		}
		if(!desc->valid && !desc->uninit){
			mySystem->returnCluster(desc->clusterNo);
		}
		desc->used = 0;
		desc->uninit = 0;
		desc->adr = 0;
		desc->history = 0;
		desc->numSeg = 0;
		desc->dirty = 0;
		desc->referenced = 0;
		desc->rwe = 0;
		desc->segmStart = 0;
		desc->valid = 0;

		pmt2++;
		if (pmt2 >= 64) {
			pmt2 = 0;
			pmt1++;
		}
		if (pmt1 >= 256)stop = 1;
		else {
			desc = (Descriptor*)*((unsigned *)*((unsigned *)pmt + pmt1));
		}
	}

	//obrisi deskriptore tog segmenta iz liste descriptors (lista onih sto imaju alocirane str)
	elemDesc* tek = descriptors, *prev=nullptr;
	while (tek != nullptr) {
		if (tek->d->numSeg == seg) {
			if (prev) {
				prev->next = tek->next;
				elemDesc* tmp = tek;
				tek = tek->next;
				delete tmp;
			}
			else {
				elemDesc* tmp = tek;
				tek = tek->next;
				descriptors = tek;
				delete tmp;
			}
		}
		else {
			prev = tek;
			tek = tek->next;
		}	
	}
}

Status KernelProcess::pageFault(VirtualAddress address){
	mySystem->mtx.lock();
	Descriptor* newDesc = getDescriptor(address);
	if (!newDesc) { mySystem->mtx.unlock(); return Status::TRAP; }

	unsigned *adr = findFreePage();
	if (adr == nullptr) {
		mySystem->mtx.unlock(); return Status::TRAP; 
	}

	if (newDesc->uninit == 0) {	//sacuvana je na disku jer je imala neki sadrzaj
		int flag = mySystem->partition->readCluster(newDesc->clusterNo, (char*)adr);
		if (!flag) { mySystem->mtx.unlock(); return Status::TRAP; }
	}
	newDesc->adr = adr;
	newDesc->dirty = 0;
	newDesc->uninit = 0;
	newDesc->valid = 1;

	mySystem->mtx.unlock();
	return Status::OK;
}
PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress adr){
	mySystem->mtx.lock();
	Descriptor* desc = getDescriptor(adr);
	if (desc == nullptr || desc->uninit || !desc->used || !desc->valid || !desc->adr) { 

		return nullptr; 

	}
	mySystem->mtx.unlock();
	return desc->adr;
}
	void KernelProcess::setPmt(PhysicalAddress pmt) {
		this->pmt = pmt;
	}

	Descriptor* KernelProcess::replaceWith() {

		Descriptor* desc;
		elemDesc* tek = descriptors;
		unsigned  history;
		unsigned  zeros = 0, max = 0;
		desc = tek->d;
		while (tek != nullptr) {
			zeros = 0;
			history = tek->d->history;
			unsigned  mask = 1;
			if (tek->d->dirty == 0)return tek->d;
			
			while (mask & history == 0 && zeros<32) {
				zeros++;
				mask = mask << 1;
			}
			if (zeros > max) { max = zeros; desc = tek->d; }

		}
		return desc;
	}

	Descriptor* KernelProcess::getDescriptor(VirtualAddress adr) {
		unsigned * ptr = (unsigned *)pmt;
		unsigned long maskPmt1 = 0x00ff0000,
			maskPmt2 = 0x0000fc00,
			maskOff = 0x000003ff;
		unsigned long pmt1 = (unsigned long)adr & maskPmt1,
			pmt2 = (unsigned long)adr & maskPmt2,
			off = (unsigned long)adr & maskOff;
		pmt1 = pmt1 >> 16;
		pmt2 = pmt2 >> 10;

		unsigned  tmp = *(ptr + pmt1);
		unsigned * pmtSec = (unsigned *)tmp;
		//POTENCIJALNO MOZE BITI PROBELM ZBOG 0 ILi NULLPTR!!!
		if (pmtSec == nullptr)return nullptr;

		Descriptor* d = (Descriptor*)(pmtSec + 4 * pmt2);
		return d;
	}

	//trazi onoliko free stranica kolko segment zahteva
	int KernelProcess::initLoadSeg(Descriptor * desc, unsigned * content){
		unsigned *adr = findFreePage();
		if (adr == nullptr)return 0;
		for (int i = 0; i < 256; i++) {
			*adr = *content;
			adr += i;
			content += i;
		}
		desc->adr = adr;
		desc->valid = 1;
		return 1;
	}

	unsigned * KernelProcess::findFreePage() {

		unsigned *adr = (unsigned *)mySystem->getProcessFree();
		//NEMA SLOBODNIH STR U OM I MORA SWAP
		if (adr == nullptr) {
			Descriptor* oldDesc = replaceWith();
			if (!oldDesc)
				return nullptr;

			//snima se ako je dirty
			if (oldDesc->dirty == 1) {
				int wrCluster = mySystem->getFreeCluster();
				if (wrCluster == -1)return nullptr;

				int flag = mySystem->partition->writeCluster(wrCluster, (const char*)oldDesc->adr);
				if (!flag) {
					mySystem->returnCluster(wrCluster);
					return nullptr;
				}
				oldDesc->clusterNo = wrCluster;
			}

			oldDesc->valid = 0;
			oldDesc->dirty = 0;
			adr = (unsigned *)oldDesc->adr;
		}

		return adr;
	}

	void KernelProcess::getUlazi(VirtualAddress adr, int* ulazPmt1, int* ulazPmt2){

		unsigned * ptr = (unsigned *)pmt;
		unsigned long maskPmt1 = 0x00ff0000,
			maskPmt2 = 0x0000fc00,
			maskOff = 0x000003ff;
		unsigned long pmt1 = (unsigned long)adr & maskPmt1,
			pmt2 = (unsigned long)adr & maskPmt2,
			off = (unsigned long)adr & maskOff;
		pmt1 = pmt1 >> 16;
		pmt2 = pmt2 >> 10;
		*ulazPmt1 = pmt1;
		*ulazPmt2 = pmt2;
	}

	

