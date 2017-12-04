/*
 * Copyright (C) 2017 by Rick Foos
 * rfoos@solengtech.com
 *
 * Copyright (C) 2017 by ETA Compute, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdbool.h>

#include "imp.h"
#include "helper/binarybuffer.h"
#include "target/algorithm.h"
#include "target/arm_opcodes.h"
#include "target/armv7m.h"
#include "target/cortex_m.h"

/**
 * @file
 * Flash programming support for ETA ECM3xx devices.
 *
 */

#define MAGIC_ADDR_M3ETA    (0x0001FFF0)
#define MAGIC_ADDR_SUBZ     (0x1001FFF0)

#if 0
/**
 * Load magic numbers into sram and bootrom will use sram vector table.
 */
static const uint32_t magic_numbers[] = {
	0xc001c0de,
	0xc001c0de,
	0xdeadbeef,
	0xc369a517,
};
#endif

#define SRAM_START_M3ETA    (0x00010000)
#define SRAM_LENGTH_M3ETA   (0x00010000)
#define SRAM_START_SUBZ     (0x10000000)
#define SRAM_LENGTH_SUBZ    (0x00020000)
#define FLASH_START_SUBZ    (0x01000000)
#define FLASH_LENGTH_SUBZ   (0x00100000)

#define SRAM_PARM_M3ETA     (0x00001000)
#define SRAM_PARM_SUBZ      (0x10001000)

#define ETA_COMMON_SRAM_MAX_SUBZ  (0x10020000)
#define ETA_COMMON_SRAM_BASE_SUBZ (0x10010000)
#define ETA_COMMON_SRAM_SIZE_SUBZ \
	(ETA_COMMON_SRAM_MAX_SUBZ - ETA_COMMON_SRAM_BASE_SUBZ)

#define ETA_COMMON_SRAM_MAX_M3ETA  (0x00020000)
#define ETA_COMMON_SRAM_BASE_M3ETA (0x00010000)
#define ETA_COMMON_SRAM_SIZE_M3ETA \
	(ETA_COMMON_SRAM_MAX_M3ETA - ETA_COMMON_SRAM_BASE_M3ETA)

#define ETA_COMMON_FLASH_MAX_SUBZ  0x01080000
#define ETA_COMMON_FLASH_BASE_SUBZ 0x01000000
#define ETA_COMMON_FLASH_SIZE_SUBZ \
	(ETA_COMMON_FLASH_MAX_SUBZ - ETA_COMMON_FLASH_BASE_SUBZ)
#define ETA_COMMON_FLASH_PAGE_SIZE_SUBZ 4096

/**
@verbatim
Conceptual 64-bit Peripheral ID
        PID3                    PID2                    PID1                    PID0
7 |  |  |  |  |  |  |0 |7 |  |  |  |  |  |  | 0|7 |  |  |  |  |  |  | 0|7 |  |  |  |  |  |  | 0|
31|  |  |28|27|  |  |24|23|  |  |20|19|18|  |  |  |  |  |12|11|  |  |  |  |  |  |  |  |  |  | 0|
  RevAnd      Customer   Revision    ^      JEP106 ID Code           PartNumber
                                     |Uses JEP 106 ID
ARM Debug Interface Architecture Specification ADIv5.0 to ADIv5.2
@endverbatim
*/

/* Jedec (Debug) Registers to ID silicon/bootrom version. */

#define REG_JEDEC_PID0             (0xF0000FE0)
#define REG_JEDEC_PID1             (0xF0000FE4)
#define REG_JEDEC_PID2             (0xF0000FE8)
#define REG_JEDEC_PID3             (0xF0000FEC)

typedef union {
	uint8_t pids[4];
	uint32_t jedec;
} jedec_pid_container;


/**
 * SRAM parameters to call helper functions.
 */
typedef struct {
	uint32_t ui32FlashAddress;	/**<  */
	uint32_t ui32Length;	/**<  */
	uint32_t ui32RC;/**<  */
} eta_flash_interface_t;

/**
 * Chip versions supported by this driver.
 */
typedef enum {
	etacore_m3eta,
	etacore_subzero,
} etacorem3_variant;

/**
 * ETA flash bank info from probe.
 */
typedef struct etacorem3_flash_bank_t {
	etacorem3_variant variant;

	/* flash geometry */
	uint32_t num_pages;	/**< Number of flash pages.  */
	uint32_t pagesize;	/**< Flash Page Size  */

	/* chip info */
	uint32_t jedec;

	bool probed;	/**< Flash bank has been probed. */
} *etacorem3_flash_bank_p;

/*
 * Jump table for subz.
 */
#define BootROM_FlashWSHelper   (0x0000009d)
#define BootROM_ui32LoadHelper  (0x000000e5)
#define BootROM_ui32StoreHelper (0x000000fd)
#define BootROM_flash_ref_cell_erase    (0x00000285)
#define BootROM_flash_erase             (0x00000385)
#define BootROM_flash_program   (0x000004C9)

/*
 * Utilities
 */

/**
 * Read Jedec PID 0-3.
 * @param bank
 * @returns success is 32 bit pids.
 * @returns failure is 0
 */
static int get_jedec_pid03(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct etacorem3_flash_bank_t *etacorem3_info = bank->driver_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (etacorem3_info->probed == false) {
		LOG_ERROR("Target not probed");
		return ERROR_FLASH_BANK_NOT_PROBED;
	}

	jedec_pid_container pid03;
	for (int i= 0,addr= REG_JEDEC_PID0; i < 4; i++,addr+= 4) {
		int retval = target_read_u8(bank->target, addr, &pid03.pids[i]);
		if (retval != ERROR_OK) {
			LOG_ERROR("JEDEC PID%i not readable", i);
			return 0;
		}
	}
	return pid03.jedec;
}


/*
 * openocd exec commands
 */

#define TARGET_HALTED_AND_PROBED(target, info) { \
		if (target->state != TARGET_HALTED) { \
			LOG_ERROR("Target not halted"); \
			return ERROR_TARGET_NOT_HALTED; \
		} \
		if (info->probed == false) { \
			LOG_ERROR("Target not probed"); \
			return ERROR_FLASH_BANK_NOT_PROBED; }}

/**
 * Mass erase flash bank.
 * @param bank Pointer to the flash bank descriptor.
 * @return retval
 *
 */
static int etacorem3_mass_erase(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct etacorem3_flash_bank_t *etacorem3_info = bank->driver_priv;
	int retval = ERROR_OK;

	TARGET_HALTED_AND_PROBED(target, etacorem3_info);

	/*
	 * Erase the main array.
	 */
	LOG_INFO("Mass erase on bank %d.", bank->bank_number);

	return retval;
}

/**
 * Erase pages in flash.
 *
 * name: etacorem3_erase
 * @param bank Pointer to the flash bank descriptor.
 * @param first
 * @param last
 * @return retval
 *
 */
static int etacorem3_erase(struct flash_bank *bank, int first, int last)
{
	struct etacorem3_flash_bank_t *etacorem3_info = bank->driver_priv;
	/* struct target *target = bank->target; */
	uint32_t retval = ERROR_OK;

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (etacorem3_info->probed == false) {
		LOG_ERROR("Target not probed");
		return ERROR_FLASH_BANK_NOT_PROBED;
	}

	/*
	 * Check pages.
	 * Fix num_pages for the device.
	 */
	if ((first < 0) || (last < first) || (last >= (int)etacorem3_info->num_pages))
		return ERROR_FLASH_SECTOR_INVALID;

	/*
	 * Just Mass Erase if all pages are given.
	 * TODO: Fix num_pages for the device
	 */
	if ((first == 0) && (last == ((int)etacorem3_info->num_pages-1)))
		return etacorem3_mass_erase(bank);

	LOG_INFO("%d pages erased!", 1+(last-first));

	return retval;
}

/**
 * Write pages to flash from buffer.
 * @param bank
 * @param buffer
 * @param offset
 * @param count
 * @returns
 *
 *
 */
static int etacorem3_write(struct flash_bank *bank,
	const uint8_t *buffer, uint32_t offset, uint32_t count)
{
	/* struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv; */
	struct target *target = bank->target;
	uint32_t address = bank->base + offset;
	uint32_t buffer_pointer = 0x10001000;
	uint32_t maxbuffer;
	uint32_t thisrun_count;
	int retval = ERROR_OK;

	if (((count%4) != 0) || ((offset%4) != 0)) {
		LOG_ERROR("write block must be multiple of 4 bytes in offset & length");
		return ERROR_FAIL;
	}

	/*
	 * Max buffer size for this device.
	 * Hard code one 8k page for now.
	 */
	maxbuffer = 0x2000;

	LOG_INFO("Flashing main array");

	while (count > 0) {
		if (count > maxbuffer)
			thisrun_count = maxbuffer;
		else
			thisrun_count = count;

		/*
		 * Write Buffer.
		 */
		retval = target_write_buffer(target, buffer_pointer, thisrun_count, buffer);

		if (retval != ERROR_OK) {
			LOG_ERROR("error writing buffer to target.");
			break;
		}

		LOG_DEBUG("address = 0x%08x", address);

		/* write a sector. */
		if (retval != ERROR_OK)
			break;
		buffer += thisrun_count;
		address += thisrun_count;
		count -= thisrun_count;
	}


	LOG_INFO("Main array flashed");

	return retval;
}

/**
 * Can't protect/unprotect pages on etacorem3.
 * @param bank
 * @param set
 * @param first
 * @param last
 * @returns
 *
 *
 */
static int etacorem3_protect(struct flash_bank *bank, int set, int first, int last)
{
	/* can't protect/unprotect on the etacorem3 */
	return ERROR_OK;
}

/**
 * Sectors are always unprotected.
 * @param bank
 * @returns
 *
 *
 */
static int etacorem3_protect_check(struct flash_bank *bank)
{
	/* sectors are always unprotected	*/
	return ERROR_OK;
}
/**
 * Probe flash part, and build sector list.
 * name: etacorem3_probe
 * @param bank Pointer to the flash bank descriptor.
 * @return on success: ERROR_OK
 *
 */
static int etacorem3_probe(struct flash_bank *bank)
{
	struct etacorem3_flash_bank_t *etacorem3_info = bank->driver_priv;

	if (!etacorem3_info->probed)
		LOG_INFO("Probing part.");

	/* etacorem3_build_sector_list(bank); */
	etacorem3_info->probed = true;
	/* Load extra info. */
	etacorem3_info->jedec = get_jedec_pid03(bank);
	return ERROR_OK;
}

/**
 * Check for erased sectors.
 * @param bank Pointer to the flash bank descriptor.
 * @return
 *
 */
static int etacorem3_erase_check(struct flash_bank *bank)
{
	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* Etamcore3 hardware doesn't support erase detection. */

	return ERROR_OK;
}

/**
 * Display ETA chip info from probe.
 * @param bank
 * @param buf Buffer to be printed.
 * @param buf_size Size of buffer.
 * @returns success
 */
static int get_etacorem3_info(struct flash_bank *bank, char *buf, int buf_size)
{
	/* struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv; */

	snprintf(buf, buf_size, "etacorem3 flash driver");

	return ERROR_OK;
}

/*
 * openocd command interface
 */

/*
 * flash_bank etacorem3 <base> <size> 0 0 <target#>
 */

/**
 * Initialize etacorem3 bank info.
 * @returns success
 *
 */
FLASH_BANK_COMMAND_HANDLER(etacorem3_flash_bank_command)
{
	struct etacorem3_flash_bank_t *etacorem3_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	etacorem3_info = calloc(1, sizeof(*etacorem3_info));
	etacorem3_info->probed = false;

	bank->driver_priv = etacorem3_info;

	return ERROR_OK;
}

/**
 * Handle external mass erase command.
 * @returns
 *
 *
 */
COMMAND_HANDLER(etacorem3_handle_mass_erase_command)
{

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	uint32_t retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (ERROR_OK != retval)
		return retval;

	if (etacorem3_mass_erase(bank) == ERROR_OK) {
		/* set all sectors as erased */
		for (int i = 0; i < bank->num_sectors; i++)
			bank->sectors[i].is_erased = 1;

		command_print(CMD_CTX, "etacorem3 mass erase complete");
	} else
		command_print(CMD_CTX, "etacorem3 mass erase failed");

	return ERROR_OK;
}

/**
 * Handle external page erase command.
 * @returns
 *
 */
COMMAND_HANDLER(etacorem3_handle_page_erase_command)
{
	struct flash_bank *bank;
	uint32_t first, last;
	uint32_t retval;

	if (CMD_ARGC < 3)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], first);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], last);

	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (ERROR_OK != retval)
		return retval;

	if (etacorem3_erase(bank, first, last) == ERROR_OK)
		command_print(CMD_CTX, "etacorem3 page erase complete");
	else
		command_print(CMD_CTX, "etacorem3 page erase failed");

	return ERROR_OK;
}

static const struct command_registration etacorem3_exec_command_handlers[] = {
	{
		.name = "mass_erase",
		.usage = "<bank>",
		.handler = etacorem3_handle_mass_erase_command,
		.mode = COMMAND_EXEC,
		.help = "Erase entire device",
	},
	{
		.name = "page_erase",
		.usage = "<bank> <first> <last>",
		.handler = etacorem3_handle_page_erase_command,
		.mode = COMMAND_EXEC,
		.help = "Erase device pages",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration etacorem3_command_handlers[] = {
	{
		.name = "etacorem3",
		.mode = COMMAND_EXEC,
		.help = "etacorem3 flash command group",
		.usage = "Support for ETA Compute parts.",
		.chain = etacorem3_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

struct flash_driver etacorem3_flash = {
	.name = "etacorem3",
	.commands = etacorem3_command_handlers,
	.flash_bank_command = etacorem3_flash_bank_command,
	.erase = etacorem3_erase,
	.protect = etacorem3_protect,
	.write = etacorem3_write,
	.read = default_flash_read,
	.probe = etacorem3_probe,
	.auto_probe = etacorem3_probe,
	.erase_check = etacorem3_erase_check,
	.protect_check = etacorem3_protect_check,
	.info = get_etacorem3_info,
};
