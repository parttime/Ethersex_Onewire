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
 

#include <avr/io.h>
#include <stdlib.h>
#include <util/crc16.h>
#include <util/delay.h>

#include "config.h"
#include "core/eeprom.h"

#include "ds2423.h"
#include "onewire.h"


static void noinline ow_crc16_seed(uint8_t *b, uint8_t len, uint16_t *seed)
{
	uint8_t i,j;

	for (i = 0; i < len; ++i)
	{
		
		*seed ^= b[i];
		for (j = 0; j < 8; ++j)
		{
			if(*seed & 1)
				*seed = (*seed >> 1) ^ 0xA001;
			else
				*seed = (*seed >> 1);
		}
		
	}
}

/* check CRC16 seed after eating all data bytes an both CRC16 bytes */
/* return 0 on success (CRC16 good), -1 on failure (CRC16 bad) */
static int8_t noinline ow_crc16_check(uint16_t *seed)
{
	if(*seed == 0xB001)
	{
		/* good */
		
		return 0;
	}
	else
	{
		/* bad */
		
		return -1;
	}
}

/* calculates CRC16 after eating all data bytes */
static uint16_t ow_crc16_calc(uint16_t *seed)
{

	return ~(*seed);
}

/* input for ow_crc16_seed but bytewise */
static void ow_crc16_seed_bytewise(uint8_t b, uint16_t *seed)
{
	ow_crc16_seed(&b, 1, seed);
}



/* check sensor family code */
int8_t ow_ds2423_sensor(struct ow_rom_code_t* rom){

	if(rom->family == OW_FAMILY_DS2423)
	return 1;
	
return 0;
}



uint8_t ow_ds2423_get_single_counter(int8_t index, uint32_t* value){
	uint16_t seed_calc=0;
	uint16_t seed_rcv=0;
	uint8_t i;
	uint8_t* value_u8= (uint8_t*) value;
	//if(index >16)
	//	return -2;
		
	ow_write_byte(OW_DS2423_READ_MEM_COUNTER);
	/* write starting address of page corresponding to requested counter */
	ow_write_byte(  (index*OW_DS2423_PAGE_SIZE)     & 0xFF);
	ow_write_byte( ((index*OW_DS2423_PAGE_SIZE)>>8) & 0xFF);
	
	/* Read answer to READ_MEM_COUNTER */
	
	/* discard contents of complete page */
	for(i=0; i< 32;i++){
		ow_crc16_seed_bytewise(ow_read_byte(),&seed_calc);
	};
	
	/* get value of requested counter  */
	for(i=0; i< 4; i++){
		value_u8[i] = ow_read_byte();
		ow_crc16_seed_bytewise(value[i], &seed_calc);
	};
	
	/* skip 4 zero bytes */
	for(i=0; i< 4;i++){
		ow_crc16_seed_bytewise(ow_read_byte(),&seed_calc);
	};
	
	/* receive CRC */
	seed_rcv  = ow_read_byte();
	seed_rcv += ow_read_byte() << 8 ;

	/* check crc */
	if(ow_crc16_check(&seed_calc) && seed_calc==seed_rcv)
		return 1;

	else return -1;

}


int8_t ow_ds2423_get_counter(struct ow_rom_code_t* rom, int8_t counter, uint32_t* countervalue){

	/* check if sensor is not of correct type */
	if(ow_ds2423_sensor(rom))
		return -1;
		
	int8_t ret;

	if (rom == NULL)
		ret = ow_skip_rom();
	else {
	/* check for known family code */
		if (!ow_ds2423_sensor(rom))
			return -2;
			
		ret = ow_match_rom(rom);
		}

	if (ret < 0)
		return ret;	

	/* check if requested page exists */
	if(counter > OW_DS2423_PAGE_COUNT)
		return -1;
		
	if(ow_match_rom(rom) == -1)
		return -1;
	 
	 /* one particular counter was requested */
	if(counter >= 0){
		ow_ds2423_get_single_counter(counter, countervalue);
		return 1;
	}
	/* use negative countervalues as range indicator for counters from (-1)*counter to last counter */
	else if(counter > -15){
	  uint8_t i;
	  for(i = -counter-1; i< OW_DS2423_PAGE_COUNT;i++){
		if(ow_ds2423_get_single_counter(counter,&countervalue[i]) <= 0) return (i+counter);
	  };
	  return (i+counter);
	}
	/* error unknown counter */
	else return -1;
}

