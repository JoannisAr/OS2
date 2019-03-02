#pragma once

struct Descriptor {
	unsigned history;
	unsigned * adr;
	unsigned clusterNo;

	unsigned char used : 1;	//ako tuu ne postoji desc nego je samo inicializovano
	unsigned char segmStart : 1;	//govori da li je ta aresa pocetak segmenta
	unsigned char valid : 1;
	unsigned char rwe : 2;
	unsigned char referenced : 1;	//da li je referencirana u periodu izmedju 2 periodicJob-a
	unsigned char dirty : 1;
	unsigned char uninit : 1;	//neinicijalizovan. Nastao prilikom createSegment a ne load
	unsigned char numSeg : 1;

	
};

struct elemDesc {
	Descriptor* d;
	elemDesc*next;
	elemDesc(Descriptor* dd) {
		d = dd;
		next = nullptr;
	}
};
