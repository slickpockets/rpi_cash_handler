
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ssp_helpers.h"
#include "port_linux.h"
#include "SSPComs.h"
#include <unistd.h>
#include "hiredis.h"

#define PORT_TO_USE "/dev/ttyUSB0"
#define OPEN_PORT  	0
#define MAX_INPUT_LEN	50U
#define MAX_INPUT_NUM	50U
#define STOP_FILE_NAME	"receipt"

#undef DEBUG_MODE

char amount[MAX_INPUT_NUM][MAX_INPUT_LEN];
char currency[MAX_INPUT_NUM][MAX_INPUT_LEN];
char route[MAX_INPUT_NUM][MAX_INPUT_LEN];
unsigned char inputNum;
redisContext *context;
unsigned char validDb;
char lastNote[5];

void do_route(SSP_COMMAND *sspC, unsigned char index)
{
	char cmd[100];
	char tempBuf[50];
	redisReply *reply;

	if (validDb)
	{	

		if (strncmp(route[index], "storage", 7) == 0)
		{
			int amountVal = (int)(strtod(amount[index], NULL)*100);
			// send the route command
			if (ssp6_set_route(sspC, amountVal, currency[index], 0x00) != SSP_RESPONSE_OK)
			{
				printf("ERROR: Route to storage failed\n");
			}
			else
			{

				redisCommand(context, "SET Storage:%s", amountVal);
				redisGetReply(context,&reply);
				freeReplyObject(reply);
			}
		}
		else if (strncmp(route[index],"cashbox", 7) == 0)
		{
			int amountVal = (int)(strtod(amount[index], NULL)*100);
			// send the route command
			if (ssp6_set_route(sspC, amountVal, currency[index], 0x01) != SSP_RESPONSE_OK)
			{
				printf("ERROR: Route to cashbox failed\n");
			}
			else
			{
				redisCommand(context, "SET Cashbox:%d", amountVal);
				redisGetReply(context,&reply);
				freeReplyObject(reply);
			}
		}
		else
		{
			printf("Wrong input parameters!\n");
		}
	}
	else
	{
		/* Do nothing */
	}

}

// pase the validators response to the poll command. the SSP_POLL_DATA6 structure has an
// array of structures which contain values and country codes
void parse_poll(SSP_COMMAND *sspC, SSP_POLL_DATA6 * poll)
{
	int i;
	char command[100];
	char tempBuf[50];
	redisReply *reply;

	if (validDb)
	{
		for (i = 0; i < poll->event_count; ++i)
		{
			printf("retrievd value is: %lu\n", poll->events[i].data1);
			if (poll->events[i].data1 == 1)
			{
				strcpy(lastNote, "5");
			}
			else if (poll->events[i].data1 == 2)
			{
				strcpy(lastNote, "10");
			}
			else if (poll->events[i].data1 == 3)
			{
				strcpy(lastNote, "20");
			}
			else if (poll->events[i].data1 == 4)
			{
				strcpy(lastNote, "50");
			}
			else if (poll->events[i].data1 == 5)
			{
				strcpy(lastNote, "100");
			}
			else
			{
				/* Do nothing */
			}
			switch(poll->events[i].event)
			{
			case SSP_POLL_RESET:
				printf("Unit Reset\n");
				// Make sure we are using ssp version 6
				if (ssp6_host_protocol(sspC, 0x06) != SSP_RESPONSE_OK)
			    {
			        printf("Host Protocol Failed\n");
			        return;
			    }
				break;
			case SSP_POLL_READ:
				// the 'read' event contains 1 data value, which if >0 means a note has been validated and is in escrow
				if (poll->events[i].data1 > 0){
					printf("Note Read %ld %s\n", poll->events[i].data1, poll->events[i].cc);
				}
				break;
			case SSP_POLL_CREDIT:
				// The note which was in escrow has been accepted
		    	printf("Credit %ld %s\n", poll->events[i].data1, poll->events[i].cc);
				/*****************************************************************************************************************/
				strcpy(command, "INCRBY total ");
				strcat(command, lastNote);
				redisAppendCommand(context, command);
				printf("%s\n", command);
				/*****************************************************************************************************************/
				redisGetReply(context,&reply);
				freeReplyObject(reply);
				/*****************************************************************************************************************/
				break;
			case SSP_POLL_INCOMPLETE_PAYOUT:
				// the validator shutdown during a payout, this event is reporting that some value remains to payout
		    	printf("Incomplete payout %ld of %ld %s\n", poll->events[i].data1, poll->events[i].data2, poll->events[i].cc);
				break;
	        case SSP_POLL_INCOMPLETE_FLOAT:
	   			// the validator shutdown during a float, this event is reporting that some value remains to float
		    	printf("Incomplete float %ld of %ld %s\n", poll->events[i].data1, poll->events[i].data2, poll->events[i].cc);
				break;
			case SSP_POLL_REJECTING:
				break;
			case SSP_POLL_REJECTED:
				// The note was rejected
				printf("Note Rejected\n");
				break;
			case SSP_POLL_STACKING:
				break;
			case SSP_POLL_STORED:
				// The note has been stored in the payout unit
				printf("Stored\n");
				/*****************************************************************************************************************/
				strcpy(command, "LPUSH stored ");
				strcat(command, lastNote);
				redisAppendCommand(context, command);
				printf("%s\n", command);
				/*****************************************************************************************************************/
				strcpy(command, "INCRBY stored:total ");
				strcat(command, lastNote);
				redisAppendCommand(context, command);
				printf("%s\n", command);
				/*****************************************************************************************************************/
				redisGetReply(context, &reply);
				freeReplyObject(reply);
				redisGetReply(context, &reply);
				freeReplyObject(reply);
				/*****************************************************************************************************************/
				break;
			case SSP_POLL_STACKED:
				// The note has been stacked in the cashbox
				printf("Stacked\n");
				/*****************************************************************************************************************/
				strcpy(command, "LPUSH stacked ");
				strcat(command, lastNote);
				redisAppendCommand(context, command);
				printf("%s\n", command);
				/*****************************************************************************************************************/
				strcpy(command, "INCRBY stacked:total ");
				strcat(command, lastNote);
				redisAppendCommand(context, command);
				printf("%s\n", command);
				/*****************************************************************************************************************/
				redisGetReply(context, &reply);
				freeReplyObject(reply);
				redisGetReply(context, &reply);
				freeReplyObject(reply);
				/*****************************************************************************************************************/
				break;
			case SSP_POLL_SAFE_JAM:
				printf("Safe Jam\n");
				break;
			case SSP_POLL_UNSAFE_JAM:
				printf("Unsafe Jam\n");
				break;
			case SSP_POLL_DISABLED:
				// The validator has been disabled
				printf("DISABLED\n");
				break;
			case SSP_POLL_FRAUD_ATTEMPT:
				// The validator has detected a fraud attempt
		    	printf("Fraud Attempt %ld %s\n", poll->events[i].data1, poll->events[i].cc);
				break;
			case SSP_POLL_STACKER_FULL:
				// The cashbox is full
				printf("Stacker Full\n");
				break;
	        case SSP_POLL_CASH_BOX_REMOVED:
	        	// The cashbox has been removed
	            printf("Cashbox Removed\n");
	            break;
	        case SSP_POLL_CASH_BOX_REPLACED:
	        	// The cashbox has been replaced
	            printf("Cashbox Replaced\n");
	            break;
	        case SSP_POLL_CLEARED_FROM_FRONT:
	        	// A note was in the notepath at startup and has been cleared from the front of the validator
	            printf("Cleared from front\n");
	            break;
	        case SSP_POLL_CLEARED_INTO_CASHBOX:
	            // A note was in the notepath at startup and has been cleared into the cashbox
	            printf("Cleared Into Cashbox\n");
	            break;
	        case SSP_POLL_CALIBRATION_FAIL:
	        	// the hopper calibration has failed. An extra byte is available with an error code.
	            printf("Calibration fail: ");

	            switch(poll->events[i].data1) {
	                case NO_FAILUE:
	                    printf ("No failure\n");
	                    break;
	                case SENSOR_FLAP:
	                    printf ("Optical sensor flap\n");
	                    break;
	                case SENSOR_EXIT:
	                    printf ("Optical sensor exit\n");
	                    break;
	                case SENSOR_COIL1:
	                    printf ("Coil sensor 1\n");
	                    break;
	                case SENSOR_COIL2:
	                    printf ("Coil sensor 2\n");
	                    break;
	                case NOT_INITIALISED:
	                    printf ("Unit not initialised\n");
	                    break;
	                case CHECKSUM_ERROR:
	                    printf ("Data checksum error\n");
	                    break;
	                case COMMAND_RECAL:
	                    printf ("Recalibration by command required\n");
	                    ssp6_run_calibration(sspC);
	                    break;
	            }
	            break;
			}
		}
	}
	else
	{
		/* Do nothing */
	}
}

void do_poll(SSP_COMMAND *sspC)
{
	SSP_POLL_DATA6 poll;
	SSP_RESPONSE_ENUM rsp_status;
	//poll the unit
	if ((rsp_status = ssp6_poll(sspC, &poll)) != SSP_RESPONSE_OK)
	{
		if (rsp_status == SSP_RESPONSE_TIMEOUT) {
			// If the poll timed out, then give up
			printf("SSP Poll Timeout\n");
			return;
		} else {
			if (rsp_status == 0xFA) {
				// The validator has responded with key not set, so we should try to negotiate one
				if (ssp6_setup_encryption(sspC,(unsigned long long)0x123456701234567LL) != SSP_RESPONSE_OK)
					printf("Encryption Failed\n");
				else
					printf("Encryption Setup\n");
			} else {
				printf ("SSP Poll Error: 0x%x\n", rsp_status);
			}
		}
	}
	parse_poll(sspC, &poll);
}

void run_validator(SSP_COMMAND *sspC)
{
    SSP6_SETUP_REQUEST_DATA setup_req;
    unsigned int i=0;
    FILE * fileHandler;
    
    //check validator is present
	if (ssp6_sync(sspC) != SSP_RESPONSE_OK)
	{
	    printf("NO VALIDATOR FOUND\n");
	    return;
	}
	printf ("Validator Found\n");

    //try to setup encryption using the default key
	if (ssp6_setup_encryption(sspC,(unsigned long long)0x123456701234567LL) != SSP_RESPONSE_OK)
        printf("Encryption Failed\n");
    else
        printf("Encryption Setup\n");


	// Make sure we are using ssp version 6
	if (ssp6_host_protocol(sspC, 0x06) != SSP_RESPONSE_OK)
    {
        printf("Host Protocol Failed\n");
        return;
    }

	// Collect some information about the validator
	if (ssp6_setup_request(sspC, &setup_req) != SSP_RESPONSE_OK) {
        printf("Setup Request Failed\n");
        return;
    }
    printf("Firmware: %s\n", setup_req.FirmwareVersion);
    printf("Channels:\n");
    for (i=0; i<setup_req.NumberOfChannels; i++){
        printf("channel %d: %d %s\n", i+1, setup_req.ChannelData[i].value, setup_req.ChannelData[i].cc);
    }

    //enable the validator
	if (ssp6_enable(sspC) != SSP_RESPONSE_OK)
	{
	    printf("Enable Failed\n");
        return;
	}
	
    if (setup_req.UnitType == 0x03) {
        // SMART Hopper requires different inhibit commands
        for (i=0; i<setup_req.NumberOfChannels; i++){
        	ssp6_set_coinmech_inhibits(sspC, setup_req.ChannelData[i].value, setup_req.ChannelData[i].cc, ENABLED);
        }
    } else {
    	if (setup_req.UnitType == 0x06 || setup_req.UnitType == 0x07) {
			//enable the payout unit
    		if (ssp6_enable_payout(sspC, setup_req.UnitType) != SSP_RESPONSE_OK)
			{
			    printf("Enable Failed\n");
    		    return;
			}
		}
    
        // set the inhibits (enable all note acceptance)
	    if (ssp6_set_inhibits(sspC,0xFF,0xFF) != SSP_RESPONSE_OK)
	    {
	        printf("Inhibits Failed\n");
            return;
	    }
	}
	do_poll(sspC);
	for (i = 0; i < (int)inputNum; i++)
	{
		do_route(sspC, (unsigned char)i);
	}
	while ((fileHandler = fopen(STOP_FILE_NAME,"rb")) == NULL)
	{
		do_poll(sspC);
	}
	fclose(fileHandler);
}

int main(int argc, char ** argv)
{
	SSP_COMMAND sspC;
	unsigned char charIdx;
	unsigned char inputIdx;
	unsigned char offset;

	inputNum = (argc - 1);
	if ( ((inputNum % 3) != 0) || (inputNum > MAX_INPUT_NUM) || (inputNum == 0) )
	{
		printf("Wrong input parameters!\n");
	}
	else
	{
		inputNum /= 3;
		for (inputIdx = 0; inputIdx < inputNum; inputIdx++)
		{
			offset = inputIdx * 3;
			for (charIdx = 0; charIdx < MAX_INPUT_LEN; charIdx++)
			{
				route[inputIdx][charIdx] = argv[offset+1][charIdx];
				if (argv[offset+1][charIdx] == '\0')
				{
					break;
				}
			}
			for (charIdx = 0; charIdx < MAX_INPUT_LEN; charIdx++)
			{
				amount[inputIdx][charIdx] = argv[offset+2][charIdx];
				if (argv[offset+2][charIdx] == '\0')
				{
					break;
				}
			}
			for (charIdx = 0; charIdx < MAX_INPUT_LEN; charIdx++)
			{
				if ( (argv[offset+3][charIdx] >= 97) && (argv[offset+3][charIdx] <= 122) )
				{
					currency[inputIdx][charIdx] = argv[offset+3][charIdx] - 32;
				}
				else
				{
					currency[inputIdx][charIdx] = argv[offset+3][charIdx];
				}
				if (argv[offset+3][charIdx] == '\0')
				{
					break;
				}
			}
		}
#ifdef DEBUG_MODE
		for (inputIdx = 0; inputIdx < inputNum; inputIdx++)
		{
			printf("%s\n", route[inputIdx]);
			printf("%s\n", amount[inputIdx]);
			printf("%s\n", currency[inputIdx]);
		}
#else /* DEBUG_MODE */
		sspC.SSPAddress = (int)(strtod(PORT_TO_USE, NULL));

		init_lib();

		// Setup the SSP_COMMAND structure for the validator at ssp_address
		sspC.Timeout = 1000;
		sspC.EncryptionStatus = NO_ENCRYPTION;
		sspC.RetryLevel = 3;
		sspC.BaudRate = 9600;

		//open the com port
		if (open_ssp_port(PORT_TO_USE) == 0)
		{
			printf("Port Error\n");
			return 1;
		}
		context = redisConnect("127.0.0.1", 6379);
		if (context != NULL && context->err)
		{
		    printf("Error: %s\n", context->errstr);
		    validDb = 0;
		    // handle error
		}
		else
		{
			validDb = 1;
		}

		//run the validator
		run_validator(&sspC);

		// close the com port
		close_ssp_port();
#endif /* DEBUG_MODE */
	}

	return 0;
}
