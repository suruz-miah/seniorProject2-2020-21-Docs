/* Array_Recorder.c
*    
*/

// C Library headers
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

// Robot Control Library headers
#include <rc/uart.h>
#include <rc/gpio.h>

// Custom headers
#define DEBUG_XBEECOM 0
#define XBEE_ARRAY_DEBUG 1
#include "CoreLib.h"
#include "ATCom.h"
#include "XBeeArray.h"

int main(int argc, char* argv[]){
	printf("\tStarting Multiplexer XBee Array Test...\n");

	// Initalize state variables
	struct XBeeArray_Settings array = {
		5,1,   // Uart buses Top(1) and Side(5) 
		3,1,  // GPIO 0 (Chip 1 Pin 25)
		3,2,  // GPIO 1 (Chip 1 Pin 17)
		0x1111 // Target Remote XBee's 16-bit address
	};

	// Initalize storage variables
	int result;
	unsigned char strengths[5];
	FILE* fp;

	// Read in peramiters
	ASSERT(argc >= 3, "Not enugh input peramiters provided.\n");
	double ideal_angle = atof(argv[1]);
	double ideal_distance = atof(argv[2]);

	// Initalize XBee Reflector Array
	printf("\tInitalizeing XBee Reflector Array...\n");
	result = XBeeArray_Init(array);
	ASSERT(result == 0, "\tERROR: Failed to Initalize XBee Array.\n");

	// Get Stenght Values from the XBee Reflector Array
	printf("\tGetting Strength values from XBee Reflector Array...\n");
	result = XBeeArray_GetStrengths(array, strengths);
	ASSERT(result == 0, "\tERROR: Failed to Get Signal Strength values from the XBee Array.\n");
	printf("\tTop Strength: %02X\n", strengths[0]);
	fprintHexBuffer(strengths+1, 4, "\tSide Strengths: ", "\n");

	// Record output
	fp = fopen("Strengths.csv", "a");
	ASSERT(fp != NULL, "\tERROR: Failed to open file.\n");
	
	if(ftell(fp) == 0) {
		result = fprintf(fp, "top,side1,side2,side3,side4,angle,distance\n");
		ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");
	}

	for(int i=0;i<5;i++) {
		result = fprintf(fp, "%d,", strengths[i]);
		ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");
	}
	result = fprintf(fp, "%1.1f,%1.1f\n",ideal_angle,ideal_distance);
	ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");

	result = fclose(fp);
	ASSERT(result == 0, "\tERROR: Faield to close file.\n");

	// Close XBee Reflector Array
	printf("\tCloseing XBee Reflector Array...\n");
	result = XBeeArray_Close(array);
	ASSERT(result == 0, "\tERROR: Failed to Close XBee Array.\n");

	return result;
}