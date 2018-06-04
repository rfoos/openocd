/*
 * Copyright (C) 2017-2018 by Rick Foos
 * rfoos@solengtech.com
 *
 * Copyright (C) 2016-2018 by Eta Compute, Inc.
 * www.etacompute.com
 *
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/**
 * @file
 * Flash Write for OCD.
 * This is an SRAM wrapper routine to call Bootrom helper functions.
 *
 */

#include <string.h>
#include <stdint.h>

#if OCD
#include "etacorem3_flash_common.h"
#else
#include "eta_chip.h"
<<<<<<< HEAD
#include "etacorem3_flash_common.h"
=======
>>>>>>> master-ocdecm
#endif

/** Flash helper function for write. */
BootROM_flash_program_T BootROM_flash_program;
#ifndef OCD
SET_MAGIC_NUMBERS;
#endif

<<<<<<< HEAD
#if OCD
/**
 * Write up to a sector to flash.
 * A non-zero value in R0 contains address of parameter block.
=======
/**
 * Write up to a sector to flash.
 * A non-zero value in R0 contains address of parameter block.
 * R0 is 0 if built as a standalone executable.
>>>>>>> master-ocdecm
 *
 * The purpose of sram_param_start is to capture the parameter in R0 and
 * not the typical argc, argv of main.
 */
int main(uint32_t sram_param_start)
{
	eta_write_interface *flash_interface;
<<<<<<< HEAD

=======
	/*
	 * This can also be built into a standalone executable with startup code.
	 * The startup code calls =main(0,NULL), and sram_param_start is 0.
	 * When sram_param_start is 0, the default SRAM_PARAM_START address is used.
	 */
>>>>>>> master-ocdecm
	if (sram_param_start == 0)
		flash_interface = (eta_write_interface *) SRAM_PARAM_START;
	else
		flash_interface = (eta_write_interface *) sram_param_start;
<<<<<<< HEAD
#else
/**
 * Write up to a sector to flash.
 * Standalone executable with startup code.
 * Use SRAM_PARAM_START to locate parameter block.
 */
int main(void)
{
	eta_write_interface *flash_interface;

	flash_interface = (eta_write_interface *) SRAM_PARAM_START;
#endif
=======
>>>>>>> master-ocdecm

	uint32_t flash_address = flash_interface->flash_address;
	uint32_t flash_length = flash_interface->flash_length;
	uint32_t flash_address_max = flash_address + flash_length;
	uint32_t *sram_buffer = (uint32_t *) flash_interface->sram_buffer;
<<<<<<< HEAD
	/* Allow a default SRAM buffer. */
	if (sram_buffer == NULL)
		sram_buffer = (uint32_t *) SRAM_BUFFER_START;
    /* fpga, silicon different behavior. */
	uint32_t bootrom_version = flash_interface->bootrom_version;

	/* Breakpoint is -2, if something goes wrong in call. */
    /* Don't use negative number returns, set retval as progress. */

    /* Invalid length not caught elsewhere. */
=======
	uint32_t bootrom_version = flash_interface->bootrom_version;

>>>>>>> master-ocdecm
	if (flash_length == 0) {
		flash_interface->retval = 0;
		goto parameter_error;
	}

<<<<<<< HEAD
    /* Before flash starts. */
=======
	/* allow a default value. */
	if (sram_buffer == NULL)
		sram_buffer = (uint32_t *) SRAM_BUFFER_START;

>>>>>>> master-ocdecm
	if (flash_address < ETA_COMMON_FLASH_BASE) {
		flash_interface->retval = 1;
		goto parameter_error;
	}
<<<<<<< HEAD

    /* After flash ends. */
	if ((flash_address >= ETA_COMMON_FLASH_MAX) && \
        (flash_address_max > ETA_COMMON_FLASH_MAX))
    {
		flash_interface->retval = 2;
=======
	/* Breakpoint is -2, don't use negative number returns. */
	if (flash_address >= ETA_COMMON_FLASH_MAX) {
		flash_interface->retval = 2;
		flash_interface->flash_address;
		goto parameter_error;
	}
	if (flash_address_max > ETA_COMMON_FLASH_MAX) {
		flash_interface->retval = 3;
>>>>>>> master-ocdecm
		goto parameter_error;
	}

	/* Set our Helper function entry point from interface. */
	if (flash_interface->bootrom_entry_point) {
		BootROM_flash_program = \
			(BootROM_flash_program_T) flash_interface->bootrom_entry_point;
	} else {
		flash_interface->retval = 4;
		goto parameter_error;
	}

<<<<<<< HEAD
	/* Board and FPGA BootrROMs use 64 bit counts for length. */
	uint32_t count = (flash_length >> 3);

	if (flash_interface->options == 1) {
        /* DWord count. */
		const uint32_t block_size = 64;
        /* Bytes to increment Flash Address. */
		const uint32_t increment_size = 512;

=======
	/*
	 * Board and FPGA BootrROMs use 64 bit counts for length.
	 */
	uint32_t count = (flash_length >> 3);

	if (flash_interface->options == 1) {

		const uint32_t block_size = 64;	/* DWord count. */
		const uint32_t increment_size = 512;	/* Bytes to increment Flash Address. */
>>>>>>> master-ocdecm
		/*
		 * Due to a bug in this version of the helper, we have to program
		 * the whole page in blocks of 512 bytes.
		 */
		uint32_t num_extra = count%block_size;
		uint32_t num_block = count/block_size + (num_extra ? 1 : 0);

<<<<<<< HEAD
		uint32_t tmp_adr = flash_address;
		uint32_t tmp_src = (uintptr_t) sram_buffer;
=======

		uint32_t tmp_adr = flash_address;
		uint32_t *tmp_src = sram_buffer;
>>>>>>> master-ocdecm

		for (int I = 0; I < num_block; I++) {

			if ((num_extra != 0) && \
				(I == (num_block - 1))) {
				flash_interface->retval = 5;
<<<<<<< HEAD
                /* The last 32 bits of buffer ending in a 52 byte string don't work. */
                if ((flash_length%52) == 0) {
                    /* Last 32 bits are not addressable */
                    int i=4;
                    /* Extend buffer 4 bytes. */
                    char *adr=(char *)(tmp_adr+52);
                    char *src=(char *)(tmp_src+52);
                    /* Put flash copy at new end of buffer. */
                    while (i--)
                        *src++ = *adr++;
                    /* write 4 more bytes, in 64 bit address. */
                    num_extra++;
                }
				ETA_CSP_FLASH_PROGRAM(tmp_adr, (uint32_t *) tmp_src,
					((bootrom_version == 0) ? num_extra * 2 : num_extra));
			} else {
				flash_interface->retval = 6;
				ETA_CSP_FLASH_PROGRAM(tmp_adr, (uint32_t *) tmp_src,
					((bootrom_version == 0) ? block_size * 2 : block_size));
			}
=======
				ETA_CSP_FLASH_PROGRAM(tmp_adr, tmp_src,
					((bootrom_version == 0) ? num_extra * 2 : num_extra));
			} else {
				flash_interface->retval = 6;
				ETA_CSP_FLASH_PROGRAM(tmp_adr, tmp_src,
					((bootrom_version == 0) ? block_size * 2 : block_size));
			}

>>>>>>> master-ocdecm
			tmp_adr += increment_size;	/* Always bytes. */
			tmp_src += 128;	/* Address Increment. */
		}
	} else
		ETA_CSP_FLASH_PROGRAM(flash_address, sram_buffer, count);

	/* Can't get an RC from bootrom, guess it worked. */
	flash_interface->retval = 0;

parameter_error:
#if OCD
	asm ("    BKPT      #0");
#else
	return flash_interface->retval;
#endif
}
