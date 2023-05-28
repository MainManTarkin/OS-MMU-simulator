#include <iostream>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>

#define failCode 1

#define endOFFile 9
#define undefinedCommand 8
#define maxPhysicalPages 8
#define totalSupportedPages 32

#define DUMP_MMUCommand 0
#define DUMP_PTCommand 1
#define ReadCommand 2
#define WriteCommand 3

using namespace std;

/*	Machine Architecture
	Pages are 2048 bytes long 		--- 11 bits
	VA Space is 32 pages			---  5 bits
	Virtual Addresses are therefore --- 16 bits
	Physical memory can fit         ---  8 pages
	PFN in bits						---  3 bits
*/

const uint32_t PAGE_BITS = 11;
const uint32_t PAGE_SIZE = (1 << PAGE_BITS);
const uint32_t PFN_BITS = 3;
const uint32_t VPN_BITS = 5;
const uint32_t VRT_PAGES = (1 << VPN_BITS);
const uint32_t PHYS_PAGES = (1 << PFN_BITS);
const uint32_t PHYS_SIZE = PHYS_PAGES * PAGE_SIZE;

const uint32_t VA_SIZE_BITS = VPN_BITS + PAGE_BITS;
const uint32_t VA_SIZE = 1 << (VA_SIZE_BITS);

struct PTE
{
	uint32_t dirty : 1;
	uint32_t referenced : 1; // UNUSED
	uint32_t present : 1;
	uint32_t valid : 1;
	uint32_t rw : 1; // UNUSED
	uint32_t pfn : PFN_BITS;
};

struct _VA
{
	uint16_t offset : PAGE_BITS;
	uint16_t vpn : VPN_BITS;
};

struct VRT_Address
{
	union
	{
		uint16_t value;
		_VA virtual_address;
	};
};

struct PMME
{
	PMME() = default;
	unsigned int in_use : 1;
	uint32_t VPN;
	uint32_t PFN;
};

// End of Arch

struct fileCommand
{
	fileCommand() = default;
	string fileInputLocation;
};

struct userInput
{

	unsigned char commdType;
	u_int16_t addrNumber;
};



class TLB
{
private:
	vector<PMME> buffer;

	bool cacheError = false;

	size_t bufferIterator;

public:
	TLB();

	void readBuffer(size_t tranPage);

	size_t getBufferSize();

	uint32_t fetchPageBuffer(uint32_t pageNumberInput);

	void writeToBuffer(size_t frameNumberInput, uint32_t newPageNumberInput);

	bool getCacheError();
};

TLB::TLB()
{

	buffer = vector<PMME>(maxPhysicalPages);

	bufferIterator = 0;
}

void TLB::readBuffer(size_t tranPage)
{
	const char *cacheStat[] = {" FREE ", " USED "};
	const int setWidthThree = 3;
	const int setWidthFour = 4;

	if (tranPage < buffer.size())
	{

		cout << '[' << setw(setWidthThree) << tranPage << ']';
		cout << cacheStat[buffer[tranPage].in_use] << "VPN:" << setw(setWidthFour);
		cout << buffer[tranPage].VPN << endl;
	}
}

size_t TLB::getBufferSize()
{

	return buffer.size();
}

bool TLB::getCacheError()
{

	bool returnBool = cacheError;

	cacheError = false;

	return returnBool;
}

uint32_t TLB::fetchPageBuffer(uint32_t pageNumberInput)
{

	uint32_t returnFrameNumber = 0;

	bool hitOrMiss = false;

	for (size_t i = 0; i < buffer.size(); i++)
	{//look for page with given page number if found output pfn otherwise miss

		if (buffer[i].VPN == pageNumberInput && buffer[i].in_use)
		{

			returnFrameNumber = buffer[i].PFN;
			hitOrMiss = true;
			break;
		}
	}

	if (!hitOrMiss)
	{

		cacheError = true;
	}

	return returnFrameNumber;
}

void TLB::writeToBuffer(size_t frameNumberInput, uint32_t newPageNumberInput)
{
	const int in_useBitSet = 1;

	for (size_t i = 0; i < buffer.size(); i++)
	{//look for page with given PFN first if not move on

		if (buffer[i].PFN == frameNumberInput && buffer[i].in_use)
		{

			buffer[i].VPN = newPageNumberInput;
			return;
		}
	}
	//just move to discrete location determined by the bufferIterartor
	buffer[bufferIterator].PFN = frameNumberInput;

	buffer[bufferIterator].VPN = newPageNumberInput;

	buffer[bufferIterator].in_use = in_useBitSet;

	bufferIterator++;

	if (bufferIterator >= maxPhysicalPages)
	{

		bufferIterator = 0;
	}
}

class MMU
{
private:
	vector<int32_t> physicalMem;

	vector<PTE> pageTable;

	TLB tlBuffer;

	size_t physicalMemIterator;

	bool pagingFlag = false;

	uint32_t swapPage(VRT_Address pageNumberInput, bool write);

	uint32_t translateVA(VRT_Address pageNumberInput, bool write);

public:
	MMU();

	void readPT();

	void readMMU();

	void readVA(u_int16_t addressInput);

	void writeVA(u_int16_t addressInput);
};

MMU::MMU()
{

	const int machArchWidth[] = {28, 27, 30, 29, 21, 30};

	pageTable = vector<PTE>(totalSupportedPages);

	physicalMem = vector<int32_t>(maxPhysicalPages);

	for (size_t i = 0; i < physicalMem.size(); i++)
	{
		physicalMem[i] = -1;
	}

	physicalMemIterator = 0;

	cout << "Machine Architecture:" << endl;
	cout << "Page Size (bits):" << setw(machArchWidth[0]) << PAGE_BITS << endl;
	cout << "Page Size (bytes):" << setw(machArchWidth[1]) << PAGE_SIZE << endl;
	cout << "VA Size (bits):" << setw(machArchWidth[2]) << VPN_BITS << endl;
	cout << "VA Size (bytes):" << setw(machArchWidth[3]) << VA_SIZE << endl;
	cout << "Physical Memory (bytes):" << setw(machArchWidth[4]) << PHYS_SIZE << endl;
	cout << "Physical Pages:" << setw(machArchWidth[5]) << PHYS_PAGES << endl;
}

void MMU::readPT()
{

	const char *pageStat[] = {" CLEAN ", " DIRTY "};
	const int setWidthThree = 3;
	const int setWidthFour = 4;

	bool foundPresentPage = false;

	cout << "PAGE TABLE:" << endl;

	for (size_t i = 0; i < pageTable.size(); i++)
	{//iterate through the page table only printing out ones that are present

		if (pageTable[i].present)
		{
			foundPresentPage = true;

			cout << '[' << setw(setWidthThree) << i << ']' << pageStat[pageTable[i].dirty] << "PRES IN PFN:" << setw(setWidthFour) << pageTable[i].pfn << endl;
		}
	}

	if (!foundPresentPage)
	{

		cout << "No present pages" << endl;
	}
}

void MMU::readMMU()
{

	cout << "MMU:" << endl;

	for (size_t i = 0; i < tlBuffer.getBufferSize(); i++)
	{

		tlBuffer.readBuffer(i);
	}
}

uint32_t MMU::swapPage(VRT_Address pageNumberInput, bool write)
{
	const char *dirtyOrWrite[] = {"", " DIRTY "};

	const int widthOfTwo = 2;

	const int presentBitSet = 1;

	//the page we are swaping out
	int32_t swappedPage = physicalMem[physicalMemIterator];

	//the page we want to place in
	uint32_t swappingInPage = pageNumberInput.virtual_address.vpn;

	//the current frame number we are looking for
	uint32_t frameNumber = physicalMemIterator;

	// change pagetable for currtenly present page if there is one
	if (swappedPage != -1)
	{

		cout << "VPN:" << setw(widthOfTwo) << swappedPage;
		cout << " SELECTED TO EJECT" << dirtyOrWrite[pageTable[swappedPage].dirty] << endl;

		if (pageTable[swappedPage].dirty)
		{//if it is dirty write that there is a write back

			cout << "VPN:" << setw(widthOfTwo) << swappedPage;
			cout << " WRITING BACK" << endl;
			pageTable[swappedPage].dirty = 0;
		}

		pageTable[swappedPage].present = 0;

		pageTable[swappedPage].pfn = 0;
	}

	// swap in new page
	physicalMem[physicalMemIterator] = swappingInPage;

	pageTable[swappingInPage].present = presentBitSet;

	pageTable[swappingInPage].pfn = physicalMemIterator;

	tlBuffer.writeToBuffer(physicalMemIterator, swappingInPage);

	// output text

	cout << "VPN:" << setw(widthOfTwo) << pageNumberInput.virtual_address.vpn;
	cout << " VA:" << setw(widthOfTwo) << pageNumberInput.virtual_address.offset;
	cout << " ASSIGNING TO PFN:" << setw(widthOfTwo) << physicalMemIterator << endl;

	cout << "VPN:" << setw(widthOfTwo) << pageNumberInput.virtual_address.vpn;
	cout << " VA:" << setw(widthOfTwo) << pageNumberInput.virtual_address.offset;
	cout << " SWAPPING IN TO PFN:" << setw(widthOfTwo) << physicalMemIterator;

	if (write)
	{

		cout << " NEWLY DIRTY" << endl;
	}
	else
	{

		cout << endl;
	}

	// increase (or reset physicalMemIterator)

	physicalMemIterator++;

	if (physicalMemIterator >= maxPhysicalPages)
	{
		physicalMemIterator = 0;
	}

	return frameNumber;
}

uint32_t MMU::translateVA(VRT_Address pageNumberInput, bool write)
{//bool write is used later in the swappage() for choosing text output 

	const int widthOfTwo = 2;

	int frameNumber = 0;

	if (pageNumberInput.virtual_address.vpn >= pageTable.size())
	{//failed vpn is larger then pageTable 
		pagingFlag = true;
		return 0;
	}

	if (pageTable[pageNumberInput.virtual_address.vpn].present)
	{//found page and is present return the frame number for succf translation

		return pageTable[pageNumberInput.virtual_address.vpn].pfn;
	}
	else
	{//page fualt it does not exist in physical mem do a swap and set paging flag

		cout << "VPN:" << setw(widthOfTwo) << pageNumberInput.virtual_address.vpn;
		cout << " VA: " << setw(widthOfTwo) << pageNumberInput.value;
		cout << " PAGE FAULT" << endl;

		frameNumber = swapPage(pageNumberInput, write);

		pagingFlag = true;

		return frameNumber;
	}
}

void MMU::readVA(u_int16_t addressInput)
{

	const int widthOfTwo = 2;

	VRT_Address addrVrt;

	uint32_t frameNumber = 0;

	bool readBackTranslation = true;

	cout << "Read " << addressInput << endl;

	addrVrt.value = addressInput;

	//check the tlb first if the page exist if it does not the error flag is set
	frameNumber = tlBuffer.fetchPageBuffer(addrVrt.virtual_address.vpn);

	if (!tlBuffer.getCacheError())
	{ // look in TLB if we had a hit: report translation

		cout << "VPN:" << setw(widthOfTwo) << addrVrt.virtual_address.vpn;
		cout << " VA:" << setw(widthOfTwo) << addrVrt.value;
		cout << " SUCCESSFUL TRANSLATION TO PFN:" << setw(widthOfTwo) << frameNumber << endl;
		return;
	}

	// if not in the TLB then translate address from the page table
	frameNumber = translateVA(addrVrt, false);

	if (pagingFlag)
	{//paging flag is set if a swap had to occur

		pagingFlag = false;
		readBackTranslation = false;
	}

	if (readBackTranslation)
	{//if no swap occured then print out a succeful of translation

		cout << "VPN:" << setw(widthOfTwo) << addrVrt.virtual_address.vpn;
		cout << " VA:" << setw(widthOfTwo) << addrVrt.virtual_address.offset;
		cout << " SUCCESSFUL TRANSLATION TO PFN:" << setw(widthOfTwo) << frameNumber << endl;
	}
}

void MMU::writeVA(u_int16_t addressInput)
{

	const int widthOfTwo = 2;

	const int dirtySet = 1;

	const char *dirtyOrWrite[] = {" NEWLY DIRTY ", " REPEAT WRITE "};

	VRT_Address addrVrt;

	uint32_t frameNumber = 0;

	bool readBackTranslation = true;

	cout << "Write " << addressInput << endl;

	addrVrt.value = addressInput;

	//check the tlb first if the page exist if it does not the error flag is set
	frameNumber = tlBuffer.fetchPageBuffer(addrVrt.virtual_address.vpn);

	if (!tlBuffer.getCacheError())
	{ // look in TLB if we had a hit: report translation

		cout << "VPN:" << setw(widthOfTwo) << addrVrt.virtual_address.vpn;
		cout << " VA:" << setw(widthOfTwo) << addrVrt.value;
		cout << " SUCCESSFUL TRANSLATION TO PFN:" << setw(widthOfTwo) << frameNumber;
		cout << dirtyOrWrite[pageTable[addrVrt.virtual_address.vpn].dirty] << endl;
		pageTable[addrVrt.virtual_address.vpn].dirty = dirtySet;
		return;
	}

	// if not in the TLB then translate address from the page table
	frameNumber = translateVA(addrVrt, true);

	if (pagingFlag)
	{//paging flag is set if a swap had to occur

		pagingFlag = false;
		readBackTranslation = false;
	}

	if (readBackTranslation)
	{//if no swap occured then print out a succeful of translation

		cout << "VPN:" << setw(widthOfTwo) << addrVrt.virtual_address.vpn;
		cout << " VA:" << setw(widthOfTwo) << addrVrt.virtual_address.offset;
		cout << " SUCCESSFUL TRANSLATION TO PFN:" << setw(widthOfTwo) << frameNumber;
		cout << dirtyOrWrite[pageTable[addrVrt.virtual_address.vpn].dirty] << endl;
		}
	pageTable[addrVrt.virtual_address.vpn].dirty = dirtySet;
}

int handleArgs(int argcInput, char **argvInput, fileCommand &fileCommandInput)
{
	const int failNeg = -1;
	int getOptReturn = 0;

	while ((getOptReturn = getopt(argcInput, argvInput, "f:")) != failNeg)
	{

		switch (getOptReturn)
		{
		case 'f':

			fileCommandInput.fileInputLocation = optarg;

			break;
		default:
			return failCode;
			break;
		}
	}

	return 0;
}

int translateJob(userInput &parseInputStruct, string textCommand)
{

	/*
		Opcode translation
	-------------------------
		DUMP_MMU = 0
		DUMP_PT = 1
		Read = 2
		Write = 3
	-------------------------
	*/

	const size_t addOne = 1;
	const char asciiForZero = 48;
	const char asciiForNine = 57;

	string commandPart;
	size_t stringPos[2] = {0, 0};
	//if the string is cut into mutiple section (Read 312) 
	//seperate them and place the opcode in commandPart and number(if there) in addrNumber
	if ((stringPos[0] = textCommand.find_first_of(' ')) != string::npos)
	{

		commandPart = textCommand.substr(0, stringPos[0]);

		for (stringPos[addOne] = stringPos[0] + addOne; stringPos[addOne] < textCommand.size(); stringPos[addOne]++)
		{

			if (textCommand[stringPos[addOne]] < asciiForZero || textCommand[stringPos[addOne]] > asciiForNine)
			{

				break;
			}
		}
	}
	else
	{

		commandPart = textCommand;
	}

	if (commandPart.compare("DUMP_MMU") == 0)
	{

		parseInputStruct.commdType = DUMP_MMUCommand;

		return 0;
	}
	else if (commandPart.compare("DUMP_PT") == 0)
	{

		parseInputStruct.commdType = DUMP_PTCommand;

		return 0;
	}
	else if (commandPart.compare("Read") == 0)
	{

		parseInputStruct.commdType = ReadCommand;
		parseInputStruct.addrNumber = stoull(textCommand.substr(stringPos[0] + addOne, stringPos[addOne]));

		return 0;
	}
	else if (commandPart.compare("Write") == 0)
	{

		parseInputStruct.commdType = WriteCommand;
		parseInputStruct.addrNumber = stoull(textCommand.substr(stringPos[0] + addOne, stringPos[addOne]));

		return 0;
	}

	return failCode;
}

void parseInput(userInput &parseInputStruct, ifstream &cmdFileInput)
{

	string textToParse;

	if (!cmdFileInput.eof())
	{

		getline(cmdFileInput, textToParse);

		if (translateJob(parseInputStruct, textToParse))
		{

			parseInputStruct.commdType = undefinedCommand;
		}
	}
	else
	{

		parseInputStruct.commdType = endOFFile;
	}
}

int pagingProgramLoop(ifstream &cmdFileInput)
{

	bool loopAlive = true;

	userInput userCommndFile;

	MMU programMMU;

	while (loopAlive)
	{
		//main program loop
		//at the beginning parse the next line of input from the ifstream
		//if the parsing did not fail
		//switch to the given fuction based on the opcode read from the stream

		parseInput(userCommndFile, cmdFileInput);

		switch (userCommndFile.commdType)
		{
		case DUMP_MMUCommand:

			programMMU.readMMU();

			break;
		case DUMP_PTCommand:

			programMMU.readPT();

			break;
		case ReadCommand:

			programMMU.readVA(userCommndFile.addrNumber);

			break;
		case WriteCommand:

			programMMU.writeVA(userCommndFile.addrNumber);

			break;
		case undefinedCommand:
			//a bad opcode or something else not recognized was given
			//so return and fail program
			return failCode;

			break;
		default:
			loopAlive = false;
			break;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{

	const char unaccetplbeArgcSize = 2;

	fileCommand fileStruct;

	ifstream cmdFile;

	//check argument counter if less then 2 then -f was never declred so end program
	if (argc < unaccetplbeArgcSize)
	{

		cerr << "Must specify file name with -f" << endl;
		return failCode;
	}

	if (handleArgs(argc, argv, fileStruct))
	{

		return failCode;
	}

	//cmdFile is the ifstream open the file given by the argument vector
	cmdFile.open(fileStruct.fileInputLocation, std::ifstream::in);

	if (!cmdFile.is_open())//check to see if it opened the given file if not output error and fail
	{

		cerr << "Failed to open: " << argv[2] << endl;
		perror("Cause: ");
		return failCode;
	}

	if (pagingProgramLoop(cmdFile))
	{

		cerr << "Problem in pagingProgramLoop()" << endl;
		return failCode;
	}

	cmdFile.close();

	return 0;
}