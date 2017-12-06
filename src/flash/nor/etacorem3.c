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
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/armv7m.h>

#include "imp.h"
#include "helper/binarybuffer.h"
#include "target/algorithm.h"
#include "target/armv7m.h"
#include "target/cortex_m.h"
#include "target/arm_opcodes.h"

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

#define ETA_COMMON_SRAM_MAX_M3ETA  (0x00020000)
#define ETA_COMMON_SRAM_BASE_M3ETA (0x00010000)
#define ETA_COMMON_SRAM_SIZE_M3ETA \
	(ETA_COMMON_SRAM_MAX_M3ETA - ETA_COMMON_SRAM_BASE_M3ETA)
#define ETA_COMMON_FLASH_PAGE_SIZE_M3ETA (0)

#define ETA_COMMON_SRAM_MAX_SUBZ  (0x10020000)
#define ETA_COMMON_SRAM_BASE_SUBZ (0x10010000)
#define ETA_COMMON_SRAM_SIZE_SUBZ \
	(ETA_COMMON_SRAM_MAX_SUBZ - ETA_COMMON_SRAM_BASE_SUBZ)

#define ETA_COMMON_FLASH_MAX_SUBZ  0x01080000
#define ETA_COMMON_FLASH_BASE_SUBZ 0x01000000
#define ETA_COMMON_FLASH_SIZE_SUBZ \
	(ETA_COMMON_FLASH_MAX_SUBZ - ETA_COMMON_FLASH_BASE_SUBZ)
#define ETA_COMMON_FLASH_PAGE_SIZE_SUBZ (4096)
#define ETA_COMMON_FLASH_NUM_PAGES_SUBZ \
	(ETA_COMMON_FLASH_SIZE_SUBZ / ETA_COMMON_FLASH_PAGE_SIZE_SUBZ)

/**
@verbatim
Conceptual 64-bit Peripheral ID
         PID2                    PID1                    PID0
|7 |  |  |  |  |  |  | 0|7 |  |  |  |  |  |  | 0|7 |  |  |  |  |  |  | 0|
|23|  |  |20|19|18|  |  |  |  |  |12|11|  |  |  |  |  |  |  |  |  |  | 0|
   Revision   ^      JEP106 ID Code           PartNumber
                         |Uses JEP 106 ID
ARM Debug Interface Architecture Specification ADIv5.0 to ADIv5.2

SCS Coresite Identification
0xE000EFE0	Peripheral ID0	0x0000000C
0xE000EFE4	Peripheral ID1	0x000000B0
0xE000EFE8	Peripheral ID2	0x0000000B
0xE000EFEC	Peripheral ID3	0x00000000
0xE000EFF0	Component ID0	0x0000000D
0xE000EFF4	Component ID1	0x000000E0
0xE000EFF8	Component ID2	0x00000005
0xE000EFFC	Component ID3	0x000000B1

ROM Table
0xE00FFFD0	Peripheral ID4	0x00000000	
0xE00FFFE0	Peripheral ID0	0x00000000
0xE00FFFE4	Peripheral ID1	0x00000000
0xE00FFFE8	Peripheral ID2	0x00000000
0xE00FFFEC	Peripheral ID3	0x00000000
0xE00FFFF0	Component ID0	0x0000000D
0xE00FFFF4	Component ID1	0x00000010
0xE00FFFF8	Component ID2	0x00000005
0xE00FFFFC	Component ID3	0x000000B1
@endverbatim
*/

/* Jedec ROM (Debug) Registers to ID silicon/bootrom version. */

/** ROM Peripheral ID 0 for Cortex M3. */
#define REG_JEDEC_PID0             (0xE00FFFE0)
#define REG_JEDEC_PID1             (0xE00FFFE4)
#define REG_JEDEC_PID2             (0xE00FFFE8)
#define REG_JEDEC_PID3             (0xE00FFFEC)

typedef union {
	uint8_t pids[4];
	uint32_t jedec;
} jedec_pid_container;

#if 0
/**
 * SRAM parameters to call helper functions.
 */
typedef struct {
	uint32_t ui32FlashAddress;	/**<  */
	uint32_t ui32Length;	/**<  */
	uint32_t ui32RC; /**<  */
} eta_flash_interface_t;
#endif

/**
 * Chip versions supported by this driver.
 * Based on Cortex M3 ROM PID 0-3.
 */
typedef enum {
	etacore_m3eta, /* 0x11201691 */
	etacore_subzero, /* 0x09201791 */
} etacorem3_variant;

/**
 * ETA flash bank info from probe.
 */
typedef struct etacorem3_flash_bank {
	etacorem3_variant variant;

	/* flash geometry */
	uint32_t num_pages;	/**< Number of flash pages.  */
	uint32_t pagesize;	/**< Flash Page Size  */

	/* part specific info needed by driver. */
	const char *target_name;
	uint32_t magic_address;
	uint32_t sram_base;
	uint32_t sram_size;
	uint32_t flash_base;
	uint32_t flash_size;

	/* chip info */
	uint32_t jedec;

	uint32_t probed;	/**< Flash bank has been probed. */
} *Petacorem3_flash_bank;

/*
 * Jump table for subzero.
 */
#define BootROM_FlashWSHelper   (0x0000009d)
#define BootROM_ui32LoadHelper  (0x000000e5)
#define BootROM_ui32StoreHelper (0x000000fd)
#define BootROM_flash_ref_cell_erase    (0x00000285)
#define BootROM_flash_erase             (0x00000385)
#define BootROM_flash_program   (0x000004C9)

#define TARGET_HALTED(target) { \
		if (target->state != TARGET_HALTED) { \
			LOG_ERROR("Target not halted"); \
			return ERROR_TARGET_NOT_HALTED; \
		}}
#define TARGET_PROBED(info) { \
		if (info->probed == 0) { \
			LOG_ERROR("Target not probed"); \
			return ERROR_FLASH_BANK_NOT_PROBED; }}	
#define TARGET_HALTED_AND_PROBED(target, info) { \
		if (target->state != TARGET_HALTED) { \
			LOG_ERROR("Target not halted"); \
			return ERROR_TARGET_NOT_HALTED; \
		} \
		if (info->probed == 0) { \
			LOG_ERROR("Target not probed"); \
			return ERROR_FLASH_BANK_NOT_PROBED; }}

/*
 * Global storage for driver.
 */

/** Part names used by flash info command. */
static char *etacorePartnames[] = {
	"ETA M3",
	"ETA M3/DSP (Subzero)",
	"Unknown"
};

/*
 * Utilities
 */

/**
 * Read Jedec PID 0-3.
 * @param bank
 * @returns success is 32 bit pids.
 * @returns failure is 0
 * @note If not Halted, reads to ROM PIDs return invalid values.
 */
static int get_jedec_pid03(struct flash_bank *bank)
{
	struct target *target = bank->target;

	/* special error return code for this routine. */
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return 0;
	}

	LOG_DEBUG("Starting PIDS");
	jedec_pid_container pid03;
	uint32_t i,addr;
	for (i=0,addr= REG_JEDEC_PID0; i < 4; i++,addr+= 4) {
		uint32_t buf;
		int retval = target_read_u32(bank->target, addr, &buf);
		pid03.pids[i] = (uint8_t) (buf & 0x000000FF);
		LOG_DEBUG("buf[0x%08x]: 0x%08x", addr, buf);
		if (retval != ERROR_OK) {
			LOG_ERROR("JEDEC PID%d not readable %d", i, retval);
			return 0;
		}
	}
	
	return pid03.jedec;
}

/**
 * Set chip variant based on PID.
 * Default to Subzero.
 * @param bank 
 * @return success is ERROR_OK.
 * @return failure if not probed.
 */
static int set_variant(struct flash_bank *bank)
{
	struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv;
	int retval = ERROR_OK;

	TARGET_PROBED(etacorem3_info);

	/* This may have failed previously if not halted. */
	if (etacorem3_info->jedec == 0) {
		etacorem3_info->jedec = get_jedec_pid03(bank);
	}

	/* Add parts here. We need to know the bootrom version. */
	switch (etacorem3_info->jedec) {
		case 0x11201691:
			etacorem3_info->variant = etacore_m3eta;
			break;
		case 0x09201791:
			etacorem3_info->variant = etacore_subzero;
			break;
		default:
			etacorem3_info->variant = etacore_subzero;
			break;
		}
	return retval;
}
	

/*
 * OpenOCD exec commands.
 */

/**
 * Mass erase flash bank.
 * @param bank Pointer to the flash bank descriptor.
 * @return retval
 *
 */
static int etacorem3_mass_erase(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv;
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
 * @param bank Pointer to the flash bank descriptor.
 * @param first
 * @param last
 * @return retval
 *
 */
static int etacorem3_erase(struct flash_bank *bank, int first, int last)
{
	struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv;
	struct target *target = bank->target;
	uint32_t retval = ERROR_OK;

	TARGET_HALTED_AND_PROBED(target, etacorem3_info);

	/*
	 * Check for valid page range.
	 */
	if ((first < 0) || (last < first) || (last >= (int)etacorem3_info->num_pages))
		return ERROR_FLASH_SECTOR_INVALID;

	/*
	 * Mass Erase if all pages are given.
	 */
	if ((first == 0) && (last == ((int)etacorem3_info->num_pages-1)))
		return etacorem3_mass_erase(bank);

	

	LOG_DEBUG("%d pages erased!", 1+(last-first));

	return retval;
}

/**
 * Write pages to flash from buffer.
 * @param bank
 * @param buffer
 * @param offset
 * @param count
 * @returns success ERROR_OK
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
 */
static int etacorem3_protect_check(struct flash_bank *bank)
{
	/* sectors are always unprotected. */
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
	struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv;

	if (etacorem3_info->probed == 1)
		LOG_INFO("Probing part.");
	else
		LOG_DEBUG("Part already probed.");

	/* Load extra info. */
	etacorem3_info->jedec = get_jedec_pid03(bank);
	if (etacorem3_info->jedec == 0) {
		LOG_WARNING("Could not read PID");
	}
	LOG_DEBUG("ROM PID0-3: 0x%08x", etacorem3_info->jedec);

	/* sets default value for case statement. */
	set_variant(bank);
	switch (etacorem3_info->variant) {
		case etacore_m3eta:
			etacorem3_info->target_name = etacorePartnames[0];
			etacorem3_info->num_pages = 0;
			etacorem3_info->pagesize = 0;
			etacorem3_info->magic_address = MAGIC_ADDR_M3ETA;
			etacorem3_info->sram_base = ETA_COMMON_SRAM_BASE_M3ETA;
			etacorem3_info->sram_size = ETA_COMMON_SRAM_SIZE_M3ETA;
			etacorem3_info->flash_base = 0;
			etacorem3_info->flash_size = 0;
			break;
		case etacore_subzero:
			etacorem3_info->target_name = etacorePartnames[1];
			etacorem3_info->num_pages = ETA_COMMON_FLASH_NUM_PAGES_SUBZ;
			etacorem3_info->pagesize = ETA_COMMON_FLASH_PAGE_SIZE_SUBZ;
			etacorem3_info->magic_address = MAGIC_ADDR_SUBZ;
			etacorem3_info->sram_base = ETA_COMMON_SRAM_BASE_SUBZ;
			etacorem3_info->sram_size = ETA_COMMON_SRAM_SIZE_SUBZ;
			etacorem3_info->flash_base = ETA_COMMON_FLASH_BASE_SUBZ;
			etacorem3_info->flash_size = ETA_COMMON_FLASH_SIZE_SUBZ;
			break;
		/* default: leave everything initialized to 0. */
	}

	if (bank->sectors) {
		free(bank->sectors);
		bank->sectors = NULL;
	}

	/* provide this for the benefit of the NOR flash framework */
	bank->base = bank->bank_number * etacorem3_info->flash_size;
	bank->size = etacorem3_info->pagesize * etacorem3_info->num_pages;
	bank->num_sectors = etacorem3_info->num_pages;

	LOG_DEBUG("bank number: %d, base: 0x%08X, size: %d KB, num sectors: %d",
		bank->bank_number,
		bank->base,
		bank->size/1024,
		bank->num_sectors);

	bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
	for (int i = 0; i < bank->num_sectors; i++) {
		bank->sectors[i].offset = i * etacorem3_info->pagesize;
		bank->sectors[i].size = etacorem3_info->pagesize;
		bank->sectors[i].is_erased = -1;
		bank->sectors[i].is_protected = -1;
	}

	etacorem3_info->probed = 1;
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
	TARGET_HALTED(bank->target);
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
	struct etacorem3_flash_bank *etacorem3_info = bank->driver_priv;

	TARGET_PROBED(etacorem3_info);

	int printed = snprintf(buf,
			buf_size,
			"\nETA Compute %s Partnum %u.%u"
			"\n\tTotal Flash: %u KB, Sram: %u KB\n",
			etacorem3_info->target_name,
			(etacorem3_info->jedec & 0x7FF),
			((etacorem3_info->jedec & 0x00F00000)>>20),
			(etacorem3_info->flash_size/1024),
			(etacorem3_info->sram_size/1024)
			);

	if (printed < 0)
		return ERROR_BUF_TOO_SMALL;
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
	struct etacorem3_flash_bank *etacorem3_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	etacorem3_info = calloc(1, sizeof(*etacorem3_info));
	etacorem3_info->probed = 0;

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
/**
 * Register exec commands.
 * Use this for additional commands specific to this target.
 * (i.e. Commands for automation, validation, or production)
 */
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

/**
 * Register required commands by name and chain to exec commands.
 */
static const struct command_registration etacorem3_command_handlers[] = {
	{
		.name = "etacorem3",
		.mode = COMMAND_EXEC,
		.help = "etacorem3 flash command group",
		.usage = "Support for ETA Compute ecm35xx parts.",
		.chain = etacorem3_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

/**
 * Required OpenOCD flash driver commands.
 */
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
