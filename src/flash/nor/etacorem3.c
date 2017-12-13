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
#include "target/armv7m.h"
#include "target/cortex_m.h"
#include "target/arm_opcodes.h"

/**
 * @file
 * Flash programming support for ETA ECM3xx devices.
 *
 */

/* SRAM Address for magic numbers. */

#define MAGIC_ADDR_M3ETA    (0x0001FFF0)
#define MAGIC_ADDR_SUBZ     (0x1001FFF0)

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
#define ETA_COMMON_FLASH_PAGE_ADDR_BITS (12)
#define ETA_COMMON_FLASH_PAGE_ADDR_MASK (0xFFFFF000)

/* Fixed parameters to bootrom calls. */
#define BOOTROM_FLASH_TNVS_COUNT   (0x10)
#define BOOTROM_FLASH_TRE_COUNT    (0x28)
#define BOOTROM_FLASH_TNVH_COUNT   (0x300)
#define BOOTROM_FLASH_TRCV_COUNT   (0x30)
#define BOOTROM_FLASH_TERASE_COUNT (0x30)
#define BOOTROM_FLASH_TPGS_COUNT   (0x28)
#define BOOTROM_FLASH_TPROG_COUNT  (0x50)

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

/**
 * Chip versions supported by this driver.
 * Based on Cortex M3 ROM PID 0-3.
 */
typedef enum {
	etacore_m3eta = 0,	/* 0x11201691 */
	etacore_subzero,	/* 0x09201791 */
	etacore_unknown,
} etacorem3_variant;

/**
 * ETA flash bank info from probe.
 */
struct etacorem3_flash_bank {
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
	uint32_t bootrom_erase_entry;
	uint32_t bootrom_program_entry;

	uint32_t jedec;	/**< chip info from rom PID. */
	bool fpga;	/**< board or fpga from cfg file. */
	bool probed;	/**< Flash bank has been probed. */
};

/*
 * Jump table for subzero bootroms.
 */

#define BootROM_flash_erase_board      (0x00000385)
#define BootROM_flash_program_board    (0x000004C9)
#define BootROM_flash_erase_fpga       (0x000001fd)
#define BootROM_flash_program_fpga     (0x00000281)

/**
 * Load and entry of wrapper function.
 * @note Depends on -work-area-phys in target file. */
#define SRAM_ENTRY_POINT        (0x10000000)
/** Location wrapper functions look for parameters, and top of stack. */
#define SRAM_PARAM_START        (0x10001000)
/** Target buffer for write operations. */
#define SRAM_BUFFER_START               (0x10002000)
/** Wait for halt after each command. T*150ms */
#define WAITHALT_TIMEOUT    (20)

/** Last element of one dimensional array */
#define ARRAY_LAST(x) x[ARRAY_SIZE(x)-1]

#define TARGET_HALTED(target) { \
		if (target->state != TARGET_HALTED) { \
			LOG_ERROR("Target not halted"); \
			return ERROR_TARGET_NOT_HALTED; } }

#define TARGET_PROBED(info) { \
		if (!info->probed) { \
			LOG_ERROR("Target not probed"); \
			return ERROR_FLASH_BANK_NOT_PROBED; } }

#define TARGET_HALTED_AND_PROBED(target, info) { \
		if (target->state != TARGET_HALTED) { \
			LOG_ERROR("Target not halted"); \
			return ERROR_TARGET_NOT_HALTED; \
		} \
		if (!info->probed) { \
			LOG_ERROR("Target not probed"); \
			return ERROR_FLASH_BANK_NOT_PROBED; } }

#define CHECK_STATUS(rc, msg) { \
		if (rc != ERROR_OK) \
			LOG_ERROR("status(%" PRId32 "):%s\n", rc, msg); }

#define CHECK_STATUS_RETURN(rc, msg) { \
		if (rc != ERROR_OK) { \
			LOG_ERROR("status(%" PRId32 "):%s\n", rc, msg); \
			return rc; } }

#define CHECK_STATUS_BREAK(rc, msg) { \
		if (rc != ERROR_OK) { \
			LOG_ERROR("status(%" PRId32 "):%s\n", rc, msg); \
			break; } }

/*
 * Global storage for driver.
 */

/** Part names used by flash info command. */
static const char const *etacorePartnames[] = {
	"ETA M3",
	"ETA M3/DSP (Subzero)",
	"Unknown"
};

/**
 * magic numbers loaded into sram before bootrom call.
 */
static const uint32_t magic_numbers[] = {
	0xc001c0de,
	0xc001c0de,
	0xdeadbeef,
	0xc369a517,
};

/*
 * Utilities
 */

/**
 * Set magic numbers in target sram.
 * @param bank information.
 * @returns status of write buffer.
 */
static int set_magic_numbers(struct flash_bank *bank)
{
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;
	int retval = target_write_buffer(bank->target, etacorem3_bank->magic_address,
			sizeof(magic_numbers), (uint8_t *)magic_numbers);
	return retval;
}

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

	jedec_pid_container pid03;
	uint32_t i,addr;
	for (i= 0,addr= REG_JEDEC_PID0; i < 4; i++,addr+= 4) {
		uint32_t buf;
		int retval = target_read_u32(bank->target, addr, &buf);
		pid03.pids[i] = (uint8_t) (buf & 0x000000FF);
		if (retval != ERROR_OK) {
			LOG_ERROR("JEDEC PID%" PRId32 " not readable %" PRId32 ".", i, retval);
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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;
	int retval = ERROR_OK;

	/* This may have failed previously if not halted. */
	if (etacorem3_bank->jedec == 0) {
		TARGET_HALTED(bank->target);
		etacorem3_bank->jedec = get_jedec_pid03(bank);
	}

	/* Add parts here. We need to know the bootrom version. */
	switch (etacorem3_bank->jedec) {
		case 0x11201691:
			etacorem3_bank->variant = etacore_m3eta;
			break;
		case 0x09201791:
			etacorem3_bank->variant = etacore_subzero;
			break;
		default:
			etacorem3_bank->variant = etacore_subzero;
			break;
	}
	return retval;
}

/** write the caller's_is_erased flag to given sectors. */
static int write_is_erased(struct flash_bank *bank, int first, int last, int flag)
{
	if ((first > bank->num_sectors) || (last > bank->num_sectors))
		return ERROR_FAIL;

	for (int i = first; i < last; i++)
		bank->sectors[i].is_erased = flag;

	LOG_DEBUG("%" PRId32 " pages erased!", 1+(last-first));

	return ERROR_OK;
}

#if 0
/** Execute bootloader command with SRAM parameters. */
static int etacorem3_exec_command(struct target *target)
{
	int retval, retflash = ERROR_OK, timeout = 0;

	/* Call sram wrapper function. */
	retval = target_resume(
			target,
			false,
			SRAM_ENTRY_POINT,
			true,
			true);

	CHECK_STATUS(retval, "error executing etacorem3 command");

	/*
	 * Wait for halt, or fault during bootloader execution.
	 */
	int detected_failure = ERROR_OK;
	while (timeout++ < WAITHALT_TIMEOUT) {
		detected_failure = target_poll(target);
		if (detected_failure != ERROR_OK)
			break;
		else if (target->state == TARGET_HALTED)
			break;
		else if (target->state == TARGET_RUNNING ||
			target->state == TARGET_DEBUG_RUNNING) {
			/*
			 * Keep polling until target halts.
			 */
			target_poll(target);
			if (detected_failure != ERROR_OK)
				break;
			alive_sleep(150);
			LOG_DEBUG("Wait for Halt: target state = %" PRId32 ".", target->state);
		} else {
			LOG_ERROR("Target not halted or running: target state = %" PRId32 ".",
				target->state);
			break;
		}
	}

	/* Report the timeout. User can continue. */
	if (timeout >= WAITHALT_TIMEOUT)
		LOG_ERROR("Wait for Halt Timeout: target state = %" PRId32 ".", timeout);

	/*
	 * Fault detected during execution takes precedence over all.
	 */
	if (detected_failure != ERROR_OK) {
		LOG_ERROR("Fault during target execution: %" PRId32 ".", detected_failure);
		retval = detected_failure;
	} else if (retflash != ERROR_OK)
		retval = retflash;

	/* Return code from target_resume OR flash. */
	return retval;
}
#endif

/*
 * OpenOCD exec commands.
 */

/** Breakpoint loaded to sram location of return codes. */
#define BREAKPOINT                  (0xfffffffe)

/** Target sram wrapper code for erase. */
static const uint8_t erase_sector_code[] = {
#include "../../../contrib/loaders/flash/etacorem3/erase.inc"
};

/** SRAM parameters for erase. */
typedef struct {
	uint32_t flashAddress;
	uint32_t flashLength;
	uint32_t mass_erase;
	uint32_t BootROM_entry_point;
	uint32_t retval;
} eta_erase_interface;

static int common_erase_run(struct flash_bank *bank)
{
	int retval;

	/*
	 * Load Magic numbers required for bootrom help function execution.
	 */
	retval = set_magic_numbers(bank);
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing magic numbers to target.");
		goto err_write_code;
	}

	struct working_area *workarea;
	struct reg_param reg_params[1];
	struct armv7m_algorithm armv7m_algo;

	/*
	 * Load erase code.
	 */
	retval = target_alloc_working_area(bank->target, sizeof(erase_sector_code),
			&workarea);
	LOG_DEBUG("workarea address: 0x%08" PRIx64 ".", workarea->address);
	if (retval != ERROR_OK) {
		LOG_ERROR("No working area available.");
		retval = ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		goto err_alloc_code;
	}
	retval = target_write_buffer(bank->target, workarea->address,
			sizeof(erase_sector_code), erase_sector_code);
	if (retval != ERROR_OK)
		goto err_write_code;

	armv7m_algo.common_magic = ARMV7M_COMMON_MAGIC;
	armv7m_algo.core_mode = ARM_MODE_THREAD;

	/* wrapper function stack */
	init_reg_param(&reg_params[0], "sp", 32, PARAM_OUT);
	/* Set the sram stack. */
	buf_set_u32(reg_params[0].value, 0, 32, SRAM_PARAM_START);
	/* Run the code. */
	retval = target_run_algorithm(bank->target,
			0, NULL,
			ARRAY_SIZE(reg_params), reg_params,
			workarea->address, 0,
			1000, &armv7m_algo);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error executing flash sector erase %" PRId32 ".", retval);
		retval = ERROR_FLASH_OPERATION_FAILED;
		goto err_run;
	}

err_run:
	for (unsigned i = 0; i < ARRAY_SIZE(reg_params); i++)
		destroy_reg_param(&reg_params[i]);

err_write_code:
	target_free_working_area(bank->target, workarea);

err_alloc_code:

	return retval;
}

/**
 * Mass erase flash bank.
 * @param bank Pointer to the flash bank descriptor.
 * @return retval
 *
 */
static int etacorem3_mass_erase(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;
	int retval = ERROR_OK;

	TARGET_HALTED_AND_PROBED(target, etacorem3_bank);

	/*
	 * Load SRAM Parameters.
	 */
	eta_erase_interface sramargs = {
		0x1000000,	/**< Start of flash. */
		0x0000000,	/**< Length 0 for all. */
		0x00000001,	/**< Request mass erase. */
		etacorem3_bank->bootrom_erase_entry,	/**< BootROM entry point. */
		BREAKPOINT	/**< Return code from BootROM. */
	};
	retval = target_write_buffer(target, SRAM_PARAM_START,
			sizeof(eta_erase_interface), (uint8_t *)&sramargs);
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing target SRAM parameters.");
		goto err_alloc_code;
	}

	LOG_DEBUG("Mass erase on bank %" PRId32 ".", bank->bank_number);

err_alloc_code:
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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;
	struct target *target = bank->target;
	int retval;

	TARGET_HALTED_AND_PROBED(target, etacorem3_bank);

	LOG_DEBUG("ETA ECM35xx erase sectors %" PRId32 " to %" PRId32 ".", first, last);

	/*
	 * Check for valid page range.
	 */
	if ((first < 0) || (last < first) || (last >= (int)etacorem3_bank->num_pages))
		return ERROR_FLASH_SECTOR_INVALID;

	/*
	 * Mass Erase if all pages are given.
	 */
	if ((first == 0) && (last == ((int)etacorem3_bank->num_pages-1)))
		return etacorem3_mass_erase(bank);


	/*
	 * Load SRAM Parameters.
	 */
	eta_erase_interface sramargs = {
		0x1000000 + (first * etacorem3_bank->pagesize),	/**< Start address in flash. */
		(last - first + 1) * etacorem3_bank->pagesize,	/**< Length in bytes. */
		0x00000000,	/**< Request page erase. */
		etacorem3_bank->bootrom_erase_entry,	/**< BootROM entry point. */
		BREAKPOINT	/**< Return code from BootROM. */
	};
	retval = target_write_buffer(bank->target, SRAM_PARAM_START,
			sizeof(eta_erase_interface), (uint8_t *)&sramargs);
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing target SRAM parameters.");
		goto err_alloc_code;
	}

	/* Common erase execution code. */
	retval = common_erase_run(bank);

	/* if successful, mark sectors as erased */
	if (retval == ERROR_OK)
		write_is_erased(bank, first, last, 1);

err_alloc_code:

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
	/* struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv; */
	struct target *target = bank->target;
	uint32_t address = bank->base + offset;
	uint32_t buffer_pointer = SRAM_BUFFER_START;
	uint32_t maxbuffer;
	uint32_t thisrun_count;
	int retval = ERROR_OK;

	if (((count%4) != 0) || ((offset%4) != 0)) {
		LOG_ERROR("write block must be multiple of 4 bytes in offset & length");
		return ERROR_FAIL;
	}

	/*
	 * Max buffer size for this device...
	 * Bootrom bug can only write 512 bytes at a time.
	 */
	maxbuffer = 512;

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
		CHECK_STATUS_BREAK(retval, "error writing buffer to target.");

		LOG_DEBUG("address = 0x%08" PRIx32, address);

		/* load sram parameters. */

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
 */
static int etacorem3_protect(struct flash_bank *bank, int set, int first, int last)
{
	/*
	 * Can't protect/unprotect on the etacorem3.
	 * Initialized to unprotected.
	 */
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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;

	if (etacorem3_bank->probed)
		return ERROR_OK;
	else
		LOG_INFO("Probing part.");

	/* Load extra info. */
	etacorem3_bank->jedec = get_jedec_pid03(bank);
	if (etacorem3_bank->jedec == 0)
		LOG_WARNING("Could not read PID");
	LOG_DEBUG("ROM PID[0-3]: 0x%08" PRIx32, etacorem3_bank->jedec);

	/* sets default value for case statement. */
	set_variant(bank);
	LOG_DEBUG("variant: %" PRId32 ".", (int)etacorem3_bank->variant);

	switch (etacorem3_bank->variant) {
		case etacore_m3eta:
			etacorem3_bank->target_name = etacorePartnames[0];
			etacorem3_bank->num_pages = 0;
			etacorem3_bank->pagesize = 0;
			etacorem3_bank->magic_address = MAGIC_ADDR_M3ETA;
			etacorem3_bank->sram_base = ETA_COMMON_SRAM_BASE_M3ETA;
			etacorem3_bank->sram_size = ETA_COMMON_SRAM_SIZE_M3ETA;
			etacorem3_bank->flash_base = 0;
			etacorem3_bank->flash_size = 0;
			etacorem3_bank->bootrom_erase_entry = 0;
			etacorem3_bank->bootrom_program_entry = 0;
			break;
		case etacore_subzero:
			etacorem3_bank->target_name = etacorePartnames[1];
			etacorem3_bank->num_pages = ETA_COMMON_FLASH_NUM_PAGES_SUBZ;
			etacorem3_bank->pagesize = ETA_COMMON_FLASH_PAGE_SIZE_SUBZ;
			etacorem3_bank->magic_address = MAGIC_ADDR_SUBZ;
			etacorem3_bank->sram_base = ETA_COMMON_SRAM_BASE_SUBZ;
			etacorem3_bank->sram_size = ETA_COMMON_SRAM_SIZE_SUBZ;
			etacorem3_bank->flash_base = ETA_COMMON_FLASH_BASE_SUBZ;
			etacorem3_bank->flash_size = ETA_COMMON_FLASH_SIZE_SUBZ;
			if (etacorem3_bank->fpga) {
				etacorem3_bank->bootrom_erase_entry = BootROM_flash_erase_fpga;
				etacorem3_bank->bootrom_program_entry = BootROM_flash_program_fpga;
			} else {
				etacorem3_bank->bootrom_erase_entry = BootROM_flash_erase_board;
				etacorem3_bank->bootrom_program_entry = BootROM_flash_program_board;
			}
			break;
		/* default: leave everything initialized to 0 (calloc). */
		case etacore_unknown:
		default:
			etacorem3_bank->target_name = ARRAY_LAST(etacorePartnames);
			LOG_ERROR("Unknown target %d.", etacorem3_bank->variant);
			break;
	}

	if (bank->sectors) {
		free(bank->sectors);
		bank->sectors = NULL;
	}

	/* provide this for the benefit of the NOR flash framework */
	bank->base = (bank->bank_number * etacorem3_bank->flash_size) + \
		etacorem3_bank->flash_base;
	bank->size = etacorem3_bank->pagesize * etacorem3_bank->num_pages;
	bank->num_sectors = etacorem3_bank->num_pages;

	LOG_DEBUG("bank number: %" PRId32 ", base: 0x%08" PRIx32
		", size: %" PRId32 " KB, num sectors: %" PRId32 ".",
		bank->bank_number,
		bank->base,
		bank->size/1024,
		bank->num_sectors);

	bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
	for (int i = 0; i < bank->num_sectors; i++) {
		bank->sectors[i].offset = i * etacorem3_bank->pagesize;
		bank->sectors[i].size = etacorem3_bank->pagesize;
		bank->sectors[i].is_erased = -1;
		/* No flash protect in this hardware. */
		bank->sectors[i].is_protected = 0;
	}

	etacorem3_bank->probed = true;
	return ERROR_OK;
}

static int emcorem3_auto_probe(struct flash_bank *bank)
{
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;

	if (etacorem3_bank->probed)
		return ERROR_OK;

	return etacorem3_probe(bank);
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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;

	TARGET_PROBED(etacorem3_bank);

	int printed = snprintf(buf,
			buf_size,
			"\nETA Compute %s Partnum %" PRIu32 ".%" PRIu32 "."
			" %s"
			"\n\tTotal Flash: %" PRIu32 " KB, Sram: %" PRIu32 " KB\n"
			"\n\tStart Flash: 0x%08" PRIx32 ", Sram: 0x%08" PRIx32 "."
			"\n\tWorkareaPhys: 0x%08l" PRIx32 ".",
			etacorem3_bank->target_name,
			(etacorem3_bank->jedec & 0x7FF),
			((etacorem3_bank->jedec & 0x00F00000)>>20),
			(etacorem3_bank->fpga ? "FPGA" : ""),
			(etacorem3_bank->flash_size/1024),
			(etacorem3_bank->sram_size/1024)
			,
			etacorem3_bank->flash_base,
			etacorem3_bank->sram_base,
			bank->target->working_area_phys
			);

	if (printed < 0)
		return ERROR_BUF_TOO_SMALL;
	return ERROR_OK;
}

/*
 * openocd command interface
 */

/**
 * Match group of case independant patterns starting with "s".
 * @param s Prefix to match
 * @param pattern Pattern to match.
 * @returns true if match.
 */
static bool name_match(const char *s, const char *pattern)
{
	int i = 0;

	/* Always returns true if null, as match string is shorter... */
	if (s == NULL || s[0] == '\0') {
		if (pattern == NULL || pattern[0] == '\0')
			return true;
		else
			return false;
	}

	while (s[i]) {
		/* If the match string is shorter, ignore excess */
		if (!pattern[i])
			return true;
		/* Use x as wildcard */
		if (pattern[i] != 'x' && tolower(s[i]) != tolower(pattern[i]))
			return false;
		i++;
	}
	LOG_DEBUG("Matched %s in %d characters.", pattern, i);
	return true;
}

/**
 * Initialize etacorem3 bank info.
 * @returns success
 *
 * From target cfg file.
 * @code
 * flash_bank etacorem3 <base> <size> 0 0 <target#> [variant]
 * @endcode
 */
FLASH_BANK_COMMAND_HANDLER(etacorem3_flash_bank_command)
{
	struct etacorem3_flash_bank *etacorem3_bank;
	const char *variant = NULL;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (CMD_ARGC == 7)
		variant = CMD_ARGV[6];

	etacorem3_bank = calloc(1, sizeof(*etacorem3_bank));
	if (!etacorem3_bank)
		return ERROR_FLASH_OPERATION_FAILED;

	etacorem3_bank->fpga = false;
	if (name_match(variant, "FPGA")) {
		etacorem3_bank->fpga = true;
		command_print(CMD_CTX, "Board is FPGA");
	}

	etacorem3_bank->probed = false;

	bank->driver_priv = etacorem3_bank;

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
	.auto_probe = emcorem3_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = etacorem3_protect_check,
	.info = get_etacorem3_info,
};
