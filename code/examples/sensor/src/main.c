/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Test application
 */

#include <stdio.h>
#include <string.h>
#include <linux/reboot.h>
#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"
#include "gpio.h"
#include "dht_read.h"

/* Name of our thing */
char thingName[100];
/* Server Details */
char ip[20];
char appkey[40];
int port = 80;

int delay = 1000;
int polling = 0;
unsigned int previousValues=0;
int oldVal = -1;
unsigned int mask = 0;

/**********************/

struct {
	int32_t pins[27];
} properties;

//gpio 12 input only, gpio 8 unstable https://community.onion.io/topic/578/issue-with-gpio-12/14
//13,15,16,17 are un-settable on onion 15,16,17 used for reset switch
char pinNumbers[] = { 0,1,6,7,8,12,13,14,18,19,20,21,23,26 };

/*****************
Helper Functions
*****************/
void sendPropertyUpdate() {
	/* This can be done with all at once via a service, or individually */

	char tmp[10];
	//one at a time
	/*	twPrimitive * value = 0;
	int i = 0;
	for (i = 1; i < (sizeof(pinNumbers)); i++)
	{
		sprintf(tmp, "pin%d", i);
		value = twPrimitive_CreateFromInteger(properties.pins[i]);
		twApi_WriteProperty(TW_THING, thingName, tmp, value, -1, 0);
		twPrimitive_Delete(value);
	}
	*/

	// Create the property list 
	propertyList * proplist = twApi_CreatePropertyList("pin0", twPrimitive_CreateFromInteger(properties.pins[0]), 0);
	if (!proplist) {
		TW_LOG(TW_ERROR, "sendPropertyUpdate: Error allocating property list");
		return;
	}
	int i = 0;
	for (i = 1; i < (sizeof(pinNumbers)); i++)
	{
		sprintf(tmp, "pin%d", pinNumbers[i]);
		twApi_AddPropertyToList(proplist, tmp, twPrimitive_CreateFromInteger(properties.pins[pinNumbers[i]]), 0);
	}

	twApi_PushProperties(TW_THING, thingName, proplist, -1, FALSE);
	twApi_DeletePropertyList(proplist);
	
}

void shutdownTask(DATETIME now, void * params) {
	TW_LOG(TW_FORCE,"shutdownTask - Shutdown service called.  SYSTEM IS SHUTTING DOWN");
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	twApi_Delete();
	twLogger_Delete();
	exit(0);	
}

void rebootTask(DATETIME now, void * params) {
	TW_LOG(TW_FORCE, "rebootTask - reboot service called.  SYSTEM IS RESTARTING");
	system("/sbin/reboot");
	
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	twApi_Delete();
	twLogger_Delete();
	exit(0);
}
/***************
Data Collection Task
****************/
/*
This function gets called at the rate defined in the task creation.  The SDK has 
a simple cooperative multitasker, so the function cannot infinitely loop.
Use of a task like this is optional and not required in a multithreaded
environment where this functonality could be provided in a separate thread.
*/
#define DATA_COLLECTION_RATE_MSEC 2000
void dataCollectionTask(DATETIME now, void * params) {
	TW_LOG(TW_TRACE, "dataCollectionTask: Executing");

	int i = 0, j = 0;
	unsigned int values = gpio_readAll() & mask;

	if (previousValues != values)
	{
		
		for (i = 0; i < (sizeof(pinNumbers)); i++)
		{
			properties.pins[pinNumbers[i]] = (values >> pinNumbers[i]) & 1;
		}
		sendPropertyUpdate();
	}
	previousValues = values;
}


/*****************
Property Handler Callbacks 
******************/
enum msgCodeEnum propertyHandler(const char * entityName, const char * propertyName,  twInfoTable ** value, char isWrite, void * userdata) {

	TW_LOG(TW_TRACE,"propertyHandler - Function called for Entity %s, Property %s\n", entityName, propertyName);
	if (!propertyName) return TWX_NOT_FOUND; //propertyName = asterisk;

	int len = strlen(propertyName);
	int i = 0;
	if (propertyName[len - 2] == 'n')
		i = atoi(&(propertyName[len - 1]));
	else
		i = atoi(&(propertyName[len - 2]));

	if (i < 0)
	{
		return TWX_NOT_FOUND;
		TW_LOG(TW_ERROR, "propertyHandler - bad index for pin");
	}
	if (value) {
		if (isWrite && *value) {
			/* Property Writes */		
			int32_t  v;
			twInfoTable_GetInteger(*value, propertyName, 0, &v);
			TW_LOG(TW_INFO, "propertyHandler - set index %d to value %d",i,v);
			gpio_command(GPIO_CMD_SET, i, &v);
			properties.pins[i] = v;
			
		} else {
			/* Property Reads */
			gpio_command(GPIO_CMD_READ, i, &properties.pins[i]);
			*value = twInfoTable_CreateFromInteger(propertyName, properties.pins[i]);

		}
		return TWX_SUCCESS;
	} else {
		TW_LOG(TW_ERROR,"propertyHandler - NULL pointer for value");
		return TWX_BAD_REQUEST;
	}
}

/*****************
Service Callbacks 
******************/


/* Example of handling multiple services in a callback */
enum msgCodeEnum multiServiceHandler(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
	TW_LOG(TW_TRACE,"multiServiceHandler - Function called");
	if (!content) {
		TW_LOG(TW_ERROR,"multiServiceHandler - NULL content pointer");
		return TWX_BAD_REQUEST;
	}
	if (strcmp(entityName, thingName) == 0) {
		 if (strcmp(serviceName, "Shutdown") == 0) {
			/* Create a task to handle the shutdown so we can respond gracefully */
			twApi_CreateTask(1, shutdownTask);
		}
		 if (strcmp(serviceName, "reboot") == 0) {
			 twApi_CreateTask(1, rebootTask);
		 }
		return TWX_NOT_FOUND;	
	}
	return TWX_NOT_FOUND;	
}

/*******************************************/
/*         Bind Event Callback             */
/*******************************************/
void BindEventHandler(char * entityName, char isBound, void * userdata) {
	if (isBound) TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Bound", entityName);
	else TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Unbound", entityName);
}

/*******************************************/
/*    OnAuthenticated Event Callback       */
/*******************************************/
void AuthEventHandler(char * credType, char * credValue, void * userdata) {
	if (!credType || !credValue) return;
	TW_LOG(TW_FORCE,"AuthEventHandler: Authenticated using %s = %s.  Userdata = 0x%x", credType, credValue, userdata);
	/* Could do a delayed bind here */
	/* twApi_BindThing(thingName); */
}


enum msgCodeEnum setPin(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	int pin, value, res;
	TW_LOG(TW_TRACE, "setPin - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR, "setPin - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}

	twInfoTable_GetInteger(params, "pin", 0, &pin);
	twInfoTable_GetInteger(params, "value", 0, &value);
	gpio_command(GPIO_CMD_SET, pin,&value);

	return TWX_SUCCESS;
}

enum msgCodeEnum readPin(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	int pin, res;
	TW_LOG(TW_TRACE, "readPin - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR, "readPin - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}

	twInfoTable_GetInteger(params, "pin", 0, &pin);
	//set direction first?
	TW_LOG(TW_TRACE, "got params %d %d, running gpio command",pin);
	gpio_command(GPIO_CMD_READ, pin, &res);
	*content = twInfoTable_CreateFromInteger("result", res);
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}


enum msgCodeEnum readTemperature(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	int pin, res,type;
	float humidity, temperature;
	if (!params || !content) {
		TW_LOG(TW_ERROR, "readTemperature - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}

	twInfoTable_GetInteger(params, "type", 0, &type);
	twInfoTable_GetInteger(params, "pin", 0, &pin);
	int result = dht_read(type, pin, &humidity, &temperature);

	if ( result < 0)
		TW_LOG(TW_ERROR, "Read temperature error return result %d", result);

	*content = twInfoTable_CreateFromNumber("result", temperature);
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}

enum msgCodeEnum readHumidity(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	int pin, res, type;
	float humidity, temperature;
	if (!params || !content) {
		TW_LOG(TW_ERROR, "readHumidity - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}

	twInfoTable_GetInteger(params, "type", 0, &type);
	twInfoTable_GetInteger(params, "pin", 0, &pin);

	int result = dht_read(type, pin, &humidity, &temperature);
	if (result < 0)
		TW_LOG(TW_ERROR, "Read humidity error return result %d", result);

	*content = twInfoTable_CreateFromNumber("result", humidity);
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}

void readSettings()
{
	char buf[255];
	FILE *file;
	int lineNumber = 0;
	char portString[8];

	memset(properties.pins, 0, 27);
	delay = 1;
	polling = 0;

	file = fopen("config.txt", "r");
	if (file) {
		while (( fgets(buf, 254, file)) != NULL)
		{

			if (buf[0] == '#' || buf[0] == '\n') //skip comments and blanks
				continue;
			int len = strlen(buf) - 1;
			if (buf[len] == '\n') buf[len] = '\0';
			puts(buf);
			switch (lineNumber++)
			{
			case 0: strcpy(thingName, buf); break;
			case 1: strcpy(ip, buf); break;
			case 2: strcpy(portString, buf); break;
			case 3: strcpy(appkey, buf); break;
			//case 4: delay = atoi(buf); break;
			//case 5: polling = atoi(buf); break;

			}
		}
		char * pEnd;
		port = strtol(portString, &pEnd, 10);
		if (pEnd == &portString[0]) //pEnd points to start on conversion failure
		{
			TW_LOG(TW_ERROR, "Unable to read port from config, using 80");		
			port = 80;
		}

		if (ferror(file)) {
			/* deal with error */
			printf("file read error\n");
		}
		fclose(file);
	}
	else
	{
		printf("config.txt not found\n");
	}
}
/***************
Main Loop
****************/
/*
Solely used to instantiate and configure the API.
*/
int main(int argc, char** argv) {
	int i = 0;
	
	initFastGpio();

	readSettings();
	//if (argc > 1)
	//	strcpy(ip, argv[1]);

	for (i = 0; i < (sizeof(pinNumbers)); i++)
	{
		mask |= (1 << pinNumbers[i]);
	}

	twDataShape * ds = 0;
	int err = 0;
	char in = 0;

	DATETIME nextDataCollectionTime = 0;

	twLogger_SetLevel( TW_DEBUG);
	twLogger_SetIsVerbose(1);
	TW_LOG(TW_FORCE, "Starting up");


	/* Initialize the API */
	err = twApi_Initialize(ip, port, TW_URI, appkey, NULL, MESSAGE_CHUNK_SIZE, MESSAGE_CHUNK_SIZE, TRUE);
	if (err) {
		TW_LOG(TW_ERROR, "Error initializing the API");
		exit(err);
	}
	
	/* Set up for connecting through an HTTP proxy: Auth modes supported - Basic, Digest and None */
	/****
	twApi_SetProxyInfo("10.128.0.90", 3128, "proxyuser123", "thingworx");
	****/

	/* Allow self signed certs */
	//twApi_SetSelfSignedOk();
	//twApi_DisableCertValidation();
	

	/* API now owns that datashape pointer, so we can reuse it */
	ds = NULL;

	/* Register our services that don't have inputs */
	twApi_RegisterService(TW_THING, thingName, "Shutdown", NULL, NULL, TW_NOTHING, NULL, multiServiceHandler, NULL);
	/////////////////////////////////////


	/* Register our services that don't have inputs */
	twApi_RegisterService(TW_THING, thingName, "reboot", NULL, NULL, TW_NOTHING, NULL, multiServiceHandler, NULL);

	/////////////////////////////////////

	ds = NULL;
	ds = twDataShape_Create(twDataShapeEntry_Create("pin", NULL, TW_INTEGER));
	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("value", NULL, TW_INTEGER));

	twApi_RegisterService(TW_THING, thingName, "setPin", NULL, ds, TW_NOTHING, NULL, setPin, NULL);

	////////////////////////////////////

	ds = NULL;
	ds = twDataShape_Create(twDataShapeEntry_Create("pin", NULL, TW_INTEGER));
	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}

	twApi_RegisterService(TW_THING, thingName, "readPin", NULL, ds, TW_INTEGER, NULL, readPin, NULL);	

	////////////////////////////////////

	ds = NULL;
	ds = twDataShape_Create(twDataShapeEntry_Create("type", "DHT type: 11 or 22", TW_INTEGER));
	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("pin", "Pin sensor is attached to", TW_INTEGER));

	twApi_RegisterService(TW_THING, thingName, "readTemperature", "Service for DHT temperature sensors. Parameter 'type' is 11 or 22, parameter 'pin' is which pin to read from.", ds, TW_NUMBER, NULL, readTemperature, NULL);

	twApi_RegisterService(TW_THING, thingName, "readHumidity", "Service for DHT humidity sensors. Parameter 'type' is 11 or 22, parameter 'pin' is which pin to read from.", ds, TW_NUMBER, NULL, readHumidity, NULL);

	/* Register our properties */
	char pinName[10];
	i = 0;
	for ( i = 0; i < (sizeof(pinNumbers)); i++)
	{	
		sprintf(pinName, "pin%d", pinNumbers[i]);
		twApi_RegisterProperty(TW_THING, thingName, pinName, TW_INTEGER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	}

	
	/* Register an authentication event handler */
	twApi_RegisterOnAuthenticatedCallback(AuthEventHandler, NULL); /* Callbacks only when we have connected & authenticated */

	/* Register a bind event handler */
	twApi_RegisterBindEventCallback(thingName, BindEventHandler, NULL); /* Callbacks only when thingName is bound/unbound */
	/* twApi_RegisterBindEventCallback(NULL, BindEventHandler, NULL); *//* First NULL says "tell me about all things that are bound */
	
	/* Bind our thing - Can bind before connecting or do it when the onAuthenticated callback is made.  Either is acceptable */
	twApi_BindThing(thingName);

	/* Connect to server */
	err = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries);
	if (!err) {
		//printf("\n");
		while(1) {
	
			#ifndef ENABLE_TASKER
					DATETIME now = twGetSystemTime(TRUE);
					//twApi_TaskerFunction(now, NULL);
					//twMessageHandler_msgHandlerTask(now, NULL);
					//if (twTimeGreaterThan(now, nextDataCollectionTime)) {
					//dataCollectionTask(now, NULL);
					//	nextDataCollectionTime = twAddMilliseconds(now, delay); //delay
					//}
			#else
					DATETIME now = twGetSystemTime(TRUE);

					if (twTimeGreaterThan(now, nextDataCollectionTime)) {
		
						dataCollectionTask(now, NULL);
						nextDataCollectionTime = twAddMilliseconds(now, delay); //delay
					}	
					//in = getch();
					//if (in == 'q' ) break;
					//else printf("\n");
			#endif
			//theres also a 10msec delay in all setdirection calls, which is a call that follows and read or write to a pin
			twSleepMsec(5);
		}
	}
	else {
		TW_LOG(TW_ERROR, "main: Server connection failed after %d attempts.  Error Code: %d", twcfg.connect_retries, err);
	}
	twApi_UnbindThing(thingName);
	twSleepMsec(100); // give time to shut down
	/* 
	twApi_Delete also cleans up all singletons including 
	twFileManager, twTunnelManager and twLogger. 
	*/
	twApi_Delete(); 
	exit(0);
}
