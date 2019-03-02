/*
#include <iostream>
#include <unordered_map>
using namespace std;
*/
/*


struct Descriptor {
	unsigned char segmStart : 1;	//govori da li je ta aresa pocetak segmenta
	unsigned char valid : 1;
	unsigned char rwe : 2;
	unsigned char referenced : 1;
	unsigned char onDisc : 1;
	unsigned char onDisc2 : 1;
	unsigned char onDisc3 : 2;
	unsigned char onDisc4 : 3;
	unsigned long history;
	unsigned clusterNo;

	void* adr;
};
*/
/*int main(int argc, char* argv[]) {
	/*Descriptor d;
	int a;
	d.history = 898989898;
	d.clusterNo = 6;


	cout << "ovo je vel descr  "<<sizeof(d) << endl;
	cout << "unsigned long" << sizeof(unsigned long) << endl;
	cout << "unsigned " << sizeof(unsigned) << endl;
	cout << "void*" << sizeof(void*) << endl;
	cin >> a;

			//////////////////////////////////

	
	unsigned long adr= 330767;
	unsigned long maskPmt1 = 0x00ff0000,
		maskPmt2 = 0x0000fc00,
		maskOff = 0x000003ff;
	unsigned long pmt1 = (unsigned long)adr & maskPmt1,
		pmt2 = (unsigned long)adr & maskPmt2,
		off = (unsigned long)adr & maskOff;
	pmt1=pmt1 >> 16;
	pmt2=pmt2 >> 10;
	cout << "pmt1: " << pmt1 << endl;
	cout << "pmt2: " << pmt2 << endl;
	cout << "off: " << off << endl;
	cin >> adr;
	int a, *ptr;
	int b = 0;
	int*ptr2 = (int*)b;
	Descriptor* d = nullptr;
	cout << ptr2;
	cin >> a;
	
	int a;
	std::unordered_map<int, char> mapica;
	mapica.insert(std::pair<int, char>(1, 'j'));
	cout << mapica[1]  << endl;
	cin >> a;
	return 0;
}*/