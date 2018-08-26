/*****************************************************************************************************************/
strcpy(command, "LPUSH ");
strcat(command, poll->events[i].cc);
strcat(command, " ");
(void)sprintf(tempBuf, "%lu", poll->events[i].data1);
strcat(command, tempBuf);
redisAppendCommand(context, command);
printf("%s\n", command);
/*****************************************************************************************************************/
strcpy(command, "INCRBY ");
strcat(command, poll->events[i].cc);
strcat(command, ":total ");
strcat(command, amountStr);
redisAppendCommand(context, command);
printf("%s\n", command);
