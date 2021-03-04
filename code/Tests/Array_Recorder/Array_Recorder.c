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
#include <signal.h>
#include <math.h>

// Custom headers
#define DEBUG_XBEECOM 0
#define XBEE_ARRAY_DEBUG 1
#include "XBeeArray.h"
#include "step.h"

#define STEPS_PER_MEASUREMENT 5
#define DEG_PER_MEASUREMENT (int)(STEPS_PER_MEASUREMENT*DEG_PER_STEP)
#define NUM_MEASUREMENTS 90/DEG_PER_MEASUREMENT

// interrupt handler to catch ctrl-c
static void __signal_handler(__attribute__ ((unused)) int dummy) {
        rc_set_state(EXITING);
        return;
}

// Prototypes
int ProcessArguments(int argc, char* argv[], double* angle, double* distance, double* height, bool* useSweep);
int AddRecored(FILE* fp, struct XBeeArray_Settings array, double angle, double distance, double height);
int StripFloat(char* str, float* value);

// Main
int main(int argc, char* argv[]){
	printf("\tStarting Array Value Recorder...\n");

	// set signal handler so the loop can exit cleanly
    signal(SIGINT, __signal_handler);

	// Set up PID file for this program
	if(rc_kill_existing_process(2.0)<-2) return -1;
    rc_make_pid_file();

	// Initalize state variables
	bool useSweep = false;
	double init_angle = 0; //atof(argv[1]);
	double init_distance = 1; //atof(argv[2]);
	double init_height = 0;
	struct XBeeArray_Settings array = {
		5,1,   // Uart buses Top(1) and Side(5) 
		3,1,  // GPIO 0 (Chip 1 Pin 25)
		3,2,  // GPIO 1 (Chip 1 Pin 17)
		0x1111 // Target Remote XBee's 16-bit address
	};

	// Initalize storage variables
	int result;
	StepperMotor sm;
	FILE* fp;

	// Read in peramiters
	result = ProcessArguments(argc, argv, &init_angle, &init_distance, &init_height, &useSweep);
	MAIN_ASSERT(result != -1, "\tERROR: Failed to Read in Arguments.\n");

	// Initalize XBee Reflector Array
	printf("\tInitalizeing XBee Reflector Array...\n");
	result = XBeeArray_Init(array);
	MAIN_ASSERT(result == 0, "\tERROR: Failed to Initialize XBee Array.\n");

    // Initialize Stepper Motor
    printf("\tInitializing Stepper Motor...\n");
    MAIN_ASSERT(stepper_init(&sm, 3, 4) != -1, "\tERROR: Failed to initialize Stepper Motor.\n");

	// Open output file in Append mode
	fp = fopen("Strengths.csv", "a");
	MAIN_ASSERT(fp != NULL, "\tERROR: Failed to open file.\n");
	
	// Main Program loop
	rc_set_state(RUNNING);
	if(useSweep){
		// Speep with angles with steper motor array
		int segment = 0;
		int direction = 1;
		float angleOffset = 0;
		while(rc_get_state() != EXITING && segment < (NUM_MEASUREMENTS*2)-1) {
			// Add recored
			double curAngle = fmod((init_angle + (double)angleOffset), 360.0);
			result = AddRecored(fp, array, curAngle, init_distance, init_height);
			MAIN_ASSERT(result != -1, " ");

			// Move the stepper motor
			direction = (segment < NUM_MEASUREMENTS) ? 1: -1;
        	result = step(&sm, direction, 5);
			MAIN_ASSERT(result != -1, " ")
			angleOffset += (DEG_PER_MEASUREMENT * direction);
		}
	} else {
		// Add record
		result = AddRecored(fp, array, init_angle, init_distance, init_height);
		MAIN_ASSERT(result != -1, " ");
	}

	// Close output file
	result = fclose(fp);
	MAIN_ASSERT(result == 0, "\tERROR: Faield to close file.\n");

	// Close XBee Reflector Array
	printf("\tCloseing XBee Reflector Array...\n");
	result = XBeeArray_Close(array);
	MAIN_ASSERT(result == 0, "\tERROR: Failed to Close XBee Array.\n");

	// Close the stepper motor
    printf("\tClosing Stepper Motor...\n");
    MAIN_ASSERT(stepper_cleanup() != -1, "\tERROR: Failed to close Stepper Motor\n");

    rc_remove_pid_file();    // remove pid file LAST

	return result;
}

int AddRecored(FILE* fp, struct XBeeArray_Settings array, double angle, double distance, double height) {
	// Initalize storage variables
	int result;
	unsigned char strengths[5];
	
	// Get Stenght Values from the XBee Reflector Array
	result = XBeeArray_GetStrengths(array, strengths);
	ASSERT(result == 0, "\tERROR: Failed to Get Signal Strength values from the XBee Array.\n");
	
	// Write out header if the file is empty
	if(ftell(fp) == 0) {
		result = fprintf(fp, "angle,distance,height,top,side1,side2,side3,side4\n");
		ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");
	}

	// Write out the ange, distance, and height values to the file
	result = fprintf(fp, "%1.1f,%1.1f,%1.1f\n",angle,distance,height);
	ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");

	// Write out strength values to the file
	for(int i=0;i<5;i++) {
		result = fprintf(fp, ",%d", strengths[i]);
		ASSERT(result >= 0, "\tERROR: Failed to write to the file\n");
	}

	return 0;
}

int ProcessArguments(int argc, char* argv[], double* angle, double* distance, double* height, bool* useSweep) {
	// Check for Help menue argument
	if(argc > 1 && !strcmp(argv[1],"-help")){
		printf("\t--- [ Help ] ---\n");
		printf("\t>    Set inital angle : -a [int]\n");
		printf("\t> Set inital distance : -d [int]\n");
		printf("\t>   Set inital height : -h [int]\n");
		printf("\t>           Use sweep : -s\n");
		return 0;
	}
	
	// Check for value set operators
	int result = 0;
	int argId = 1;
	while(argId < argc) {
		// Check if current main argumetn is in valid format "-[Char]"
		char* mainArg = argv[argId++];
		bool mainFailed = !((mainArg[0] == '-') && (mainArg[2] == '\0'));
		if(!mainFailed) {
			switch (mainArg[1]) {
			case 'a': // Chip Select Argument
				if(argId < argc) {
					float posAngle;
					result = StripFloat(argv[argId++], &posAngle);
				 	if(result > 0) {
					 	*angle = posAngle;
						break;
					}
				}
				ASSERT(false,"ERROR: -a(Set Inital angle) should be in format of \"-a [int]\".\n")
			case 'd': // Distance Set argument
				if(argId < argc) {
					float posDistance;
					result = StripFloat(argv[argId++], &posDistance);
				 	if(result > 0) {
					 	*distance = posDistance;
						break;
					}
				}
				ASSERT(false,"ERROR: -d(Set inital distance) should be in format of \"-d [int]\".\n")
			case 'h': // Distance Set argument
				if(argId < argc) {
					float posHeight;
					result = StripFloat(argv[argId++], &posHeight);
				 	if(result > 0) {
					 	*height = posHeight;
						break;
					}
				}
				ASSERT(false,"ERROR: -h(Set inital height) should be in format of \"-h [int]\".\n")
			case 's': // Distance Set argument
				*useSweep = true;
			default: // Invalid argument
				mainFailed = true; break;
			}
		}
		ASSERT(!mainFailed, "ERROR: \"%s\" is not a valid argument. Use \"%s -help\" for list of valid arguments.\n", mainArg, argv[0]);
	}

	return 0;
}

int StripFloat(char* str, float* value) {
	int len;
	int result = sscanf(str, "%f %n", value, &len);
	if(result==1 && !str[len]){
		return len;
	}
	return -1;
}
