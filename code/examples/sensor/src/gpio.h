#pragma once

#ifndef GPIO_INCLUDED
#define GPIO_INCLUDED

#include <sys/types.h>
#include <sys/mman.h>
//#include "mman.h""
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

typedef enum e_GpioCmd {
	GPIO_CMD_SET = 0,
	GPIO_CMD_READ,
	GPIO_CMD_SET_DIRECTION,
	GPIO_CMD_GET_DIRECTION,
	GPIO_CMD_PWM,
	NUM_GPIO_CMD
} gpioCmd;


		int _SetupAddress(unsigned long int blockBaseAddr, unsigned long int blockSize);
		void _WriteReg(unsigned long int registerOffset, unsigned long int value);
		unsigned long int _ReadReg(unsigned long int registerOffset);
		void _SetBit(unsigned long int *regVal, int bitNum, int value);
		int _GetBit(unsigned long int regVal, int bitNum);
		void initFastGpio(void);
		int SetDirection(int pinNum, int bOutput);
		int GetDirection(int pinNum, int *bOutput);
		int Set(int pinNum, int value);
		int Read(int pinNum, int *value);
		unsigned int gpio_readAll();
		void gpio_command(int cmd, int pin, int *value);

#endif 