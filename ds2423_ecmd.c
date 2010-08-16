/*
 * Support for the counters of DS2423
 * ECMD interface
 * Copyright (C) 2009 Meinhard Schneider <meini@meini.org>
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "protocols/ecmd/ecmd-base.h"
#include "protocols/syslog/syslog.h"

#include "ds2423.h"
#include "onewire.h"

typedef struct {
union{
	uint32_t raw;
	uint16_t split[2];
};	

}   uint32_split;


int8_t parse_ow_rom(char *cmd, struct ow_rom_code_t *rom);

int8_t noinline ow_ecmd_parse_ds2423rom_arg(char **cmd, struct ow_rom_code_t *rom){
int8_t ret;

	while(**cmd == ' ')
		(*cmd)++;

	if(strlen(*cmd) >= 16)
	{
		/* called with rom code */

		ret = parse_ow_rom(*cmd, rom);

		/* check for parse error */
		if(ret < 0)
		{
			return ECMD_ERR_PARSE_ERROR;
		}

		/* move pointer behind rom code */
		*cmd += 16;
	}
	else
	{
		/* no rom code, use skip command */
		rom->raw = 0;
	}

	while(**cmd == ' ')
		(*cmd)++;

	return 0;
}



int16_t parse_cmd_onewire_ds2423_get_counter(char *cmd, char *output, uint16_t len){

	struct ow_rom_code_t rom;
	int8_t ret;
	uint8_t ecmd_return_len = 0;
	uint32_t result;
	int8_t counter;
	
	
	
	ret = ow_ecmd_parse_ds2423rom_arg( &cmd, &rom);
	
	if(ret != 0)
		return ECMD_ERR_READ_ERROR;	
	
	for(uint8_t i=0;i<8 ; i++){
	  ret=snprintf_P(output,  len, PSTR("%hhu "), &rom.bytewise[i]);
	  ecmd_return_len+=ret;
	  output+=ret;
	}
	if (rom.raw == 0)
		ret = ow_skip_rom();
	else {
	/* check for known family code */
		if (!ow_ds2423_sensor(&rom))
			return ECMD_ERR_PARSE_ERROR;
			
		ret = ow_match_rom(&rom);
		}

	if (ret < 0)
		return ECMD_ERR_READ_ERROR;	
	
	if(*cmd == '\0')
	/* get all counters */
		counter=-1;
	
	if(sscanf_P(cmd, PSTR("%d"), &counter) != 1)
		return ECMD_ERR_PARSE_ERROR;
	
	
	if(counter >= 0){
		uint8_t sreg = SREG;
		cli();
		ret= ow_ds2423_get_single_counter(counter,&result);
		SREG = sreg;
		if(ret > 0){  
		ecmd_return_len =snprintf_P(output, len, PSTR("%c: %u"), (unsigned char) ret+65,result );
		};
	} 
	/* use negative countervalues as range indicator for counters from (-1)*counter to last counter */
	else if(counter > -15){
	 for(uint8_t i = -counter-1; i< OW_DS2423_PAGE_COUNT;i++){
		uint8_t sreg = SREG;
		cli();
		ret=ow_ds2423_get_single_counter(counter,&result);
		SREG = sreg;
		if(ret > 0){
			ret=snprintf_P(output, len, PSTR("%c: %u"), (unsigned char) ret+65,result );
			ecmd_return_len+=ret;
			output+=ret;
			}
			else break;
	};
	}
	/* error unknown counter */
	else return ECMD_ERR_READ_ERROR;

	
	return ECMD_FINAL(ecmd_return_len); 
	
}

int16_t parse_cmd_onewire_ds2423_hi(char *cmd, char *output, uint16_t len){
  return  ECMD_FINAL(snprintf_P(output, len, PSTR("Hi")) );
  
}

int16_t parse_cmd_onewire_ds2423_loop(char *cmd, char *output, uint16_t len){
	uint8_t ecmd_ret_len =0;
	uint8_t ret =0;
	uint32_t a;
	uint32_split c;
	uint16_t b;
	debug_printf("len: %u \n",len);


	if(sscanf_P(cmd, PSTR("%d %10"PRIu32""), &b,&a) == 1)
			a=0xFFFFFFFF;
	debug_printf("%s\n",cmd);
	
	for(uint8_t i=0;i<b && i<2;i++){
		ret=snprintf_P(output,len,PSTR("%u : %10"PRIu32"\n"),b-i,a);
		debug_printf("%u : %10"PRIu32"\n",b-i,a);
		output += ret;
		ecmd_ret_len += ret;
		a--;
  	}
	if(b>2){
		b-=2;
		snprintf_P(cmd,len,PSTR("%d %10"PRIu32""),b,a);
		return ECMD_AGAIN(ecmd_ret_len);
	};
	
	/*
	for(; b>0; b--){
		if(b%2)
			ret=snprintf_P(output,len,PSTR("1"));
		else
			ret=snprintf_P(output,len,PSTR("0"));
	output+=ret;
	ecmd_ret_len+=ret;
	};
*/


return  ECMD_FINAL(ecmd_ret_len);
  
}



/*
	-- Ethersex META --
	block([[Dallas_1-wire_Bus]])
	ecmd_feature(onewire_ds2423_get_counter, "1w ds2423 counter", [DEVICE], `get value of given counter')
	ecmd_feature(onewire_ds2423_hi, "1w ds2423 hi", [DEVICE], `Say hi')
	ecmd_feature(onewire_ds2423_loop, "1w ds2423 loop", [DEVICE], `loop with ECMD_FINAL(ecmd_ret_len)')
*/
