/*!
 *****************************************************************************
 @file:    UARTCmd.c
 @author:  $Author: nxu2 $
 @brief:   UART Command process
 @version: $Revision: 766 $
 @date:    $Date: 2017-08-21 14:09:35 +0100 (Mon, 21 Aug 2017) $
 -----------------------------------------------------------------------------

 Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

 This software is proprietary to Analog Devices, Inc. and its licensors.
 By using this software you agree to the terms of the associated
 Analog Devices Software License Agreement.

 *****************************************************************************/
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include "math.h"
#include "Impedance.h"
#define LINEBUFF_SIZE 128
#define CMDTABLE_SIZE 7

uint32_t help(uint32_t para1, uint32_t para2);
uint32_t command_get_cfg(char *param1_str, double para2);
uint32_t command_set_cfg(char *param1_str, double para2);
uint32_t IDN(uint32_t para1, uint32_t para2);
uint32_t EIS_start(uint32_t para1, uint32_t para2);
uint32_t EIS_stop(uint32_t para1, uint32_t para2);


struct __uartcmd_table
{
	void *pObj;
	const char *cmd_name;
	const char *pDesc;
} uart_cmd_table[CMDTABLE_SIZE] =
{
{ (void*) help, "help", "print supported commands" },
{ (void*) help, "?", "print supported commands" },
{ (void*) command_set_cfg, "setcfg", "set config" },
{ (void*) command_get_cfg, "getcfg", "returns config" },
{ (void*) IDN, "*IDN?", "returns IDN" },
{ (void*) EIS_start, "EISStart", "returns IDN" },
{ (void*) EIS_stop, "EISStop", "returns IDN" },
};

uint32_t help(uint32_t para1, uint32_t para2)
{
	int i = 0;
	printf("*****help menu*****\nbelow are supported commands:\r\n");
	for (; i < CMDTABLE_SIZE; i++)
	{
		if (uart_cmd_table[i].pObj)
			printf("%-8s --\t%s\r\n", uart_cmd_table[i].cmd_name,
					uart_cmd_table[i].pDesc);
	}
	printf("***table end***\r\n");
	return 0x87654321;
}

char line_buffer[LINEBUFF_SIZE];
uint32_t line_buffer_index = 0;
uint32_t token_count = 0;
void *pObjFound = 0;
uint32_t parameter1, parameter2;
char *param1_str = NULL;
double param2_float = 0;

void UARTCmd_RemoveSpaces(void)
{
	int i = 0;
	token_count = 0;
	char flag_found_token = 0;
	while (i < line_buffer_index)
	{
		if (line_buffer[i] == ' ')
			line_buffer[i] = '\0';
		else
			break;
		i++;
	}
	if (i == line_buffer_index)
		return; /* All spaces... */
	while (i < line_buffer_index)
	{
		if (line_buffer[i] == ' ')
		{
			line_buffer[i] = '\0';
			flag_found_token = 0;
		}
		else
		{
			if (flag_found_token == 0)
				token_count++;
			flag_found_token = 1;
		}
		i++;
	}
}

// uint8_t calc_checksum(const char *data) {
//     uint8_t chk = 0;
//     while (*data) {
//         chk ^= (uint8_t)(*data++);
//     }
//     return chk;
// }
void send_packet(const char *type, const char *data) {
    char buffer[256];
    char frame[300];

    snprintf(buffer, sizeof(buffer), "%s | %s", type, data);
    uint8_t chk = calc_checksum(buffer);

    snprintf(frame, sizeof(frame), "<%s | %02X>\r\n", buffer, chk);

    printf("%s", frame);   // or UART transmit
}

void UARTCmd_MatchCommand(void)
{
	char *pcmd;
	int i = 0;
	pObjFound = 0;
	while (i < line_buffer_index)
	{
		if (line_buffer[i] != '\0')
		{
			pcmd = &line_buffer[i];
			break;
		}
		i++;
	}
	for (i = 0; i < CMDTABLE_SIZE; i++)
	{
		if (strcmp(uart_cmd_table[i].cmd_name, pcmd) == 0)
		{
			pObjFound = uart_cmd_table[i].pObj;
			break;
		}
	}
}

/* Translate string 'p' to number, store results in 'Res', return error code */
static uint32_t Str2Num(char *s, uint32_t *Res)
{
	char *p;
	unsigned int base = 10;

	*Res = strtoul(s, &p, base);

	return 0;
}

void UARTCmd_TranslateParas(void)
{
	param2_float = 0;
	char *p = line_buffer;
	parameter1 = 0;
	parameter2 = 0;
	while (*p == '\0')
		p++; /* goto command */
	while (*p != '\0')
		p++; /* skip command. */
	while (*p == '\0')
		p++; /* goto first parameter */
	param1_str = p;
	if (Str2Num(p, &parameter1) != 0)
		parameter1 = 0;

	if (token_count == 2)
		return; /* Only one parameter */
	while (*p != '\0')
		p++; /* skip first command. */
	while (*p == '\0')
		p++; /* goto second parameter */
//	a  = strtod(p,NULL);
	param2_float = atof(p);
	param2_float =roundf(param2_float * 1000.0f) / 1000.0f;
	if(Str2Num(p, &parameter2) != 0)
		parameter2 = 0;

}

void UARTCmd_Process(char c)
{
	if (line_buffer_index >= LINEBUFF_SIZE - 1)
		line_buffer_index = 0; /* Error: buffer overflow */
	if ((c == '\r') || (c == '\n'))
	{
		line_buffer[line_buffer_index] = '\0';
		/* Start to process command */
		if (line_buffer_index == 0)
		{
			line_buffer_index = 0; /* Reset buffer */
			return; /* No command inputs, return */
		}
		/* Step1, remove space */
		UARTCmd_RemoveSpaces();
		if (token_count == 0)
		{
			line_buffer_index = 0; /* Reset buffer */
			return; /* No valid input */
		}
		/* Step2, match commands */
		UARTCmd_MatchCommand();
		if (pObjFound == 0)
		{
			line_buffer_index = 0; /* Reset buffer */
			return; /* Command not support */
		}
		if (token_count > 1) /* There is parameters */
		{
			UARTCmd_TranslateParas();
		}
		/* Step3, call function */
		if (param1_str != NULL){
			((uint32_t (*) (char *,float)) (pObjFound)) (param1_str, param2_float);
			param1_str = NULL;
		}

		else
			((uint32_t (*)(uint32_t, uint32_t)) (pObjFound))(parameter1,
					parameter2);
		line_buffer_index = 0; /* Reset buffer */
	}
	else
	{
		line_buffer[line_buffer_index++] = c;
	}
}
