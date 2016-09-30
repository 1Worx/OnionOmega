#include "gpio.h"


volatile unsigned long int *regAddress;
int debugLevel = 0, verbosityLevel = 0;	
// Register access
int _SetupAddress(unsigned long int blockBaseAddr, unsigned long int blockSize)
{
	int  m_mfd;

	//if (debugLevel == 0)
	{
		if ((m_mfd = open("/dev/mem", O_RDWR)) < 0)
		{
			printf("Cant open  /dev/mem");
			return EXIT_FAILURE;	// maybe return -1
		}

		regAddress = (unsigned long*)mmap(NULL,
			blockSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			m_mfd,
			blockBaseAddr
			);
		close(m_mfd);
		if (verbosityLevel > 0)
			printf("regAddress %d\n", regAddress);
		if (regAddress == MAP_FAILED)
		{
			printf("Cant find register address");
			return EXIT_FAILURE;	// maybe return -2
		}
	}

	return EXIT_SUCCESS;	// regAddress is now populated
}

void _WriteReg(unsigned long int registerOffset, unsigned long int value)
{
	if (verbosityLevel > 0)	printf("Writing register 0x%08lx with data 0x%08lx \n", (regAddress + registerOffset), value);

	*(regAddress + registerOffset) = value;
}

unsigned long int _ReadReg(unsigned long int registerOffset)
{
	unsigned long int 	value = 0x0;

	// read the value 
	value = *(regAddress + registerOffset);

	if (verbosityLevel > 0)
	{
		printf("Read register 0x%08lx, data: 0x%08lx \n", (regAddress + registerOffset), value);
	}
	return(value);
}

// change the value of a single bit
void _SetBit(unsigned long int *regVal, int bitNum, int value)
{
	if (value == 1) {
		*regVal |= (1 << bitNum);
	}
	else {
		*regVal &= ~(1 << bitNum);
	}

	// try this out
	// regVal ^= (-value ^ regVal) & (1 << bitNum);
}

// find the value of a single bit
int _GetBit(unsigned long int regVal, int bitNum)
{
	int value;

	// isolate the specific bit
	value = ((regVal >> bitNum) & 0x1);

	return (value);
}



#define REGISTER_BLOCK_ADDR			0x18040000
#define REGISTER_BLOCK_SIZE			0x30

#define REGISTER_OE_OFFSET			0
#define REGISTER_IN_OFFSET			1
#define REGISTER_OUT_OFFSET			2
#define REGISTER_SET_OFFSET			3
#define REGISTER_CLEAR_OFFSET		4



int 	pinNumber;
void initFastGpio(void)
{
	// setup the memory address space
	_SetupAddress(REGISTER_BLOCK_ADDR, REGISTER_BLOCK_SIZE);
}


// public functions
int SetDirection(int pinNum, int bOutput)
{
	unsigned long int regVal;

	// read the current input and output settings
	regVal = _ReadReg(REGISTER_OE_OFFSET);
	if (verbosityLevel > 0) printf("Direction setting read: 0x%08lx\n", regVal);

	// set the OE for this pin
	_SetBit(&regVal, pinNum, bOutput);
	if (verbosityLevel > 0) printf("Direction setting write: 0x%08lx\n", regVal);

	// write the new register value
	_WriteReg(REGISTER_OE_OFFSET, regVal);

	return (EXIT_SUCCESS);
}

int GetDirection(int pinNum, int *bOutput)
{
	unsigned long int regVal;

	// read the current input and output settings
	regVal = _ReadReg(REGISTER_OE_OFFSET);
	if (verbosityLevel > 0) printf("Direction setting read: 0x%08lx\n", regVal);

	// find the OE for this pin
	*bOutput = _GetBit(regVal, pinNum);


	return (EXIT_SUCCESS);
}

int Set(int pinNum, int value)
{
	unsigned long int 	regAddr;
	unsigned long int 	regVal;

	if (value == 0) {
		// write to the clear register
		regAddr = REGISTER_CLEAR_OFFSET;
	}
	else {
		// write to the set register
		regAddr = REGISTER_SET_OFFSET;
	}

	// put the desired pin value into the register 
	regVal = (0x1 << pinNum);

	// write to the register
	_WriteReg(regAddr, regVal);


	return EXIT_SUCCESS;
}

int Read(int pinNum, int *value)
{
	unsigned long int 	regVal;

	// read the current value of all pins
	regVal = _ReadReg(REGISTER_IN_OFFSET);

	// find the value of the specified pin
	*value = _GetBit(regVal, pinNum);


	return EXIT_SUCCESS;
}


unsigned int gpio_readAll()
{
	return _ReadReg(REGISTER_IN_OFFSET);
}

void gpio_command(int cmd, int pin, int *value)
{
	//int value = v;
	// object operations	
	int status = 0;
	char valString[255];
	int  dir = 0;
	switch (cmd) {
	case GPIO_CMD_SET:
		SetDirection(pin, 1); // set to output
		twSleepMsec(10);
		Set(pin, *value);

		strcpy(valString, (*value == 1 ? "1" : "0"));
		break;

	case GPIO_CMD_READ:
		SetDirection(pin, 0); // set to input
		twSleepMsec(10);
		Read(pin, value);
		strcpy(valString, (*value == 1 ? "1" : "0"));
		break;

	case GPIO_CMD_SET_DIRECTION:
		SetDirection(pin, dir); // set pin direction
		twSleepMsec(10);
		strcpy(valString, (dir == 1 ? "output" : "input"));
		break;

	case GPIO_CMD_GET_DIRECTION:
		GetDirection(pin, &dir); // find pin direction
		strcpy(valString, (dir == 1 ? "output" : "input"));
		break;

	default:
		status = EXIT_FAILURE;
		break;
	}

	if (status != EXIT_FAILURE) {
		printf("pin %d val %s\n", pin, valString);
	}
}