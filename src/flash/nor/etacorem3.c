/*
 * Copyright (C) 2017-2018 by Rick Foos
 * rfoos@solengtech.com
 *
 * Copyright (C) 2017-2018 by Eta Compute, Inc.
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
 */

/* SRAM Address for magic numbers. */

#define MAGIC_ADDR_M3ETA    (0x0001FFF0)
#define MAGIC_ADDR_ECM3501  (0x1001FFF0)

/* Max flash/sram common size all supported parts. */

#define ETA_COMMON_SRAM_SIZE_MAX        (0x00020000)
#define ETA_COMMON_FLASH_SIZE_MAX       (0x00080000)

/* M3ETA */

#define ETA_COMMON_SRAM_MAX_M3ETA  (0x00020000)
#define ETA_COMMON_SRAM_BASE_M3ETA (0x00000000)
#define ETA_COMMON_SRAM_SIZE_M3ETA \
	(ETA_COMMON_SRAM_MAX_M3ETA  - ETA_COMMON_SRAM_BASE_M3ETA)

#define ETA_COMMON_FLASH_MAX_M3ETA  0
#define ETA_COMMON_FLASH_BASE_M3ETA 0
#define ETA_COMMON_FLASH_SIZE_M3ETA 0

/* ECM3501 */

#define ETA_COMMON_SRAM_MAX_ECM3501  (0x10020000)
#define ETA_COMMON_SRAM_BASE_ECM3501 (0x10000000)
#define ETA_COMMON_SRAM_SIZE_ECM3501 \
	(ETA_COMMON_SRAM_MAX_ECM3501 - ETA_COMMON_SRAM_BASE_ECM3501)

#define ETA_COMMON_FLASH_MAX_ECM3501  (0x01080000)
#define ETA_COMMON_FLASH_BASE_ECM3501 (0x01000000)
#define ETA_COMMON_FLASH_SIZE_ECM3501 \
	(ETA_COMMON_FLASH_MAX_ECM3501 - ETA_COMMON_FLASH_BASE_ECM3501)

#define ETA_COMMON_FLASH_PAGE_SIZE_ECM3501 (4096)
#define ETA_COMMON_FLASH_NUM_PAGES_ECM3501 \
	(ETA_COMMON_FLASH_SIZE_ECM3501 / ETA_COMMON_FLASH_PAGE_SIZE_ECM3501)
#define ETA_COMMON_FLASH_PAGE_ADDR_BITS (12)
#define ETA_COMMON_FLASH_PAGE_ADDR_MASK (0xFFFFF000)

#if 0
/**
@verbatim
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

/* Jedec ROM (Debug) Registers to ID chip/bootrom version. */

/** ROM Peripheral ID 0 for Cortex M3. */
#define REG_JEDEC_PID0             (0xE00FFFE0)
#define REG_JEDEC_PID1             (0xE00FFFE4)
#define REG_JEDEC_PID2             (0xE00FFFE8)
#define REG_JEDEC_PID3             (0xE00FFFEC)

typedef union {
	uint8_t pids[4];
	uint32_t jedec;
} jedec_pid_container;

#define PID_M3ETA       (0x11201691)
#define PID_ECM3501     (0x09201791)
#endif

/**
 * ETA flash bank info from probe.
 */
struct etacorem3_flash_bank {

	/* flash geometry */

	uint32_t num_pages;	/**< Number of flash pages.  */
	uint32_t pagesize;	/**< Flash Page Size  */

	/* part specific info needed by driver. */

	const char *target_name;
	uint8_t *magic_address;	/**< Location of keys in sram. */
	uint32_t sram_base;	/**< SRAM Start Address. */
	uint32_t sram_size;	/**< SRAM size calculated during probe. */
	uint32_t sram_max;
	uint32_t flash_base;	/**< Flash Start Address. */
	uint32_t flash_size;	/**< Flash size calculated during probe. */
	uint32_t flash_max;
	uint32_t bootrom_erase_entry;
	uint32_t bootrom_write_entry;	/**< bootrom_flash_program */
#if 0
	uint32_t jedec;	/**< chip info from rom PID. */
#endif
	int fpga;	/**< m3eta, ecm3501 (chip or fpga). */
	bool probed;	/**< Flash bank has been probed. */
};

/**
 * Load and entry points of wrapper function.
 * @note Depends on -work-area-phys in target file.
 */
#define SRAM_ENTRY_POINT        (0x10000000)
/** Location wrapper functions look for parameters, and top of stack. */
#define SRAM_PARAM_START        (0x10001000)
/** Target buffer start address for write operations. */
#define SRAM_BUFFER_START       (0x10002000)
/** Target buffer size. */
#define SRAM_BUFFER_SIZE        (0x00004000)
/** Target algorithm start, Set in target -work-area-size. */
#define SRAM_ALGO_START         (0x10006000)
/** Target algo code max size. Set in target file -work-area-phys. */
#define SRAM_ALGO_SIZE          (0x00004000)

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
			LOG_ERROR("status(%d):%s\n", rc, msg); }

#define CHECK_STATUS_RETURN(rc, msg) { \
		if (rc != ERROR_OK) { \
			LOG_ERROR("status(%d):%s\n", rc, msg); \
			return rc; } }

#define CHECK_STATUS_BREAK(rc, msg) { \
		if (rc != ERROR_OK) { \
			LOG_ERROR("status(%d):%s\n", rc, msg); \
			break; } }

/*
 * Global storage for driver.
 */


/** Magic numbers loaded into SRAM before BootROM call. */
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
	/* Use endian neutral call. */
	target_buffer_set_u32_array(bank->target, etacorem3_bank->magic_address,
		(sizeof(magic_numbers)/sizeof(uint32_t)), magic_numbers);
	return ERROR_OK;
}

#if 0
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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;

	/* If we have already read PID successfully, don't erase. */
	if (etacorem3_bank->jedec != 0)
		return etacorem3_bank->jedec;

	/* special error return code 0 for this routine. */

	/* If target is not halted, PID's don't read back a good value. */
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return 0;
	}

	jedec_pid_container pid03;
	uint32_t i, addr;
	for (i = 0, addr = REG_JEDEC_PID0; i < 4; i++, addr += 4) {
		uint32_t buf;
		int retval = target_read_u32(bank->target, addr, &buf);
		pid03.pids[i] = (uint8_t) (buf & 0x000000FF);
		if (retval != ERROR_OK) {
			LOG_ERROR("JEDEC PID%d not readable %d.", i, retval);
			return 0;
		}
	}

	return pid03.jedec;
}
#endif
/*
 * Jump table for ecm35xx bootroms.
 */
#define BOOTROM_LOADER_FLASH_M3ETA	(0x000004F8)
#define BOOTROM_LOADER_FPGA_M3ETA	(0x00000564)
#define BOOTROM_FLASH_ERASE_BOARD       (0x00000385)
#define BOOTROM_FLASH_PROGRAM_BOARD     (0x000004C9)
#define BOOTROM_FLASH_ERASE_FPGA        (0x00000249)
#define BOOTROM_FLASH_PROGRAM_FPGA      (0x000002CD)

/*
 * Check for BootROM version with values at jump table locations.
 */
#define CHECK_FLASH_M3ETA (0xb08cb580)
#define CHECK_FPGA_M3ETA (0xb08cb580)
#define CHECK_FLASH_ERASE_FPGA          (0x00b089b4)
#define CHECK_FLASH_PROGRAM_FPGA        (0x00b089b4)
#define CHECK_FLASH_ERASE_BOARD          (0x00b086b5)
#define CHECK_FLASH_PROGRAM_BOARD        (0x00b086b5)

/**
 * Get BootROM variant from BootRom address contents.
 * Set default, do not return errors.
 */
static int get_variant(struct flash_bank *bank)
{
	/* Detect chip or FPGA bootROM. */
	uint32_t check_erase_fpga, check_program_fpga;
	uint32_t check_erase_board, check_program_board;
	uint32_t check_flash_m3eta, check_fpga_m3eta;
	int retval;
	/* ECM3501 FPGA */
	retval = target_read_u32(bank->target, BOOTROM_FLASH_PROGRAM_FPGA, &check_program_fpga);
	if (retval == ERROR_OK)
		retval = target_read_u32(bank->target, BOOTROM_FLASH_ERASE_FPGA, &check_erase_fpga);
	/* ECM3501 CHIP */
	if (retval == ERROR_OK)
		retval = target_read_u32(bank->target, BOOTROM_FLASH_PROGRAM_BOARD, &check_program_board);
	if (retval == ERROR_OK)
		retval = target_read_u32(bank->target, BOOTROM_FLASH_ERASE_BOARD, &check_erase_board);
	/* M3ETA CHIP */
	if (retval == ERROR_OK)
		retval = target_read_u32(bank->target, BOOTROM_LOADER_FLASH_M3ETA, &check_flash_m3eta);
	if (retval == ERROR_OK)
		retval = target_read_u32(bank->target, BOOTROM_LOADER_FPGA_M3ETA, &check_fpga_m3eta);
	/** @todo As more BootROM versions become available, change this to a table lookup. */
	if (retval == ERROR_OK) {
		if ((check_program_fpga == CHECK_FLASH_PROGRAM_FPGA) && \
			(check_erase_fpga == CHECK_FLASH_ERASE_FPGA))
			retval = 1;	/* fpga bootrom */
		else if ((check_program_board == CHECK_FLASH_PROGRAM_BOARD) && \
			(check_erase_board == CHECK_FLASH_ERASE_BOARD))
			retval = 0;	/* chip bootrom */
		else if ((check_flash_m3eta == CHECK_FLASH_M3ETA) && \
			(check_fpga_m3eta == CHECK_FPGA_M3ETA))
			retval = 2;	/* m3eta bootrom */
		else
			retval = 0;	/* chip bootrom */
	} else {
		LOG_WARNING("BootROM entry points could not be read (%d).", retval);
		retval = 0;	/* Default is ECM3501 chip. */
	}
	LOG_DEBUG("Bootrom version: %d", retval);
	return retval;
}


/** write the caller's_is_erased flag to given sectors. */
static int write_is_erased(struct flash_bank *bank, int first, int last, int flag)
{
	if ((first > bank->num_sectors) || (last > bank->num_sectors))
		return ERROR_FAIL;

	for (int i = first; i < last; i++)
		bank->sectors[i].is_erased = flag;

	LOG_DEBUG("%d pages erased!", 1+(last-first));

	return ERROR_OK;
}

/**
 * Find memory size.
 * @param bank
 * @param startaddress
 * @param maxsize
 * @param increment
 * @returns on success size in bytes.
 * @returns on failure 0.
 *
 */
static uint32_t get_memory_size(struct flash_bank *bank,
	uint32_t startaddress,
	uint32_t maxsize,
	uint32_t increment)
{
	int retval;
	uint32_t i, data;

	/* Chip has no Memory. */
	retval = target_read_u32(bank->target, startaddress, &data);
	if (retval != ERROR_OK) {
		LOG_ERROR("Memory not found at 0x%08X.", startaddress);
		return 0;
	}

	/* The memory scan causes a bus fault. Ignore expected error messages. */
	int save_debug_level = debug_level;
	debug_level = LOG_LVL_OUTPUT;
	/* Read flash size we are testing. 0 - Max flash size, 16k increments. */
	for (i = 0; i < maxsize; i += increment) {
		retval = target_read_u32(bank->target, startaddress+i, &data);
		if (retval != ERROR_OK)
			break;
	}
	/* Restore output level. */
	debug_level = save_debug_level;

	LOG_DEBUG("Memory starting at 0x%08X size: %d KB.",
		startaddress, i/1024);
	return i;
}

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
	uint32_t flash_address;
	uint32_t flash_length;
	uint32_t options;
	uint32_t bootrom_entry_point;
	uint32_t retval;
} eta_erase_interface;

/** Target sram wrapper code for write. */
static const uint8_t write_sector_code[] = {
#include "../../../contrib/loaders/flash/etacorem3/write.inc"
};

/** SRAM parameters for write. */
typedef struct {
	uint32_t flash_address;
	uint32_t flash_length;
	uint32_t sram_buffer;
	uint32_t bootrom_entry_point;
	uint32_t options;	/**< 1 - Write 512 bytes at a time. */
	uint32_t retval;
} eta_write_interface;

static int common_erase_run(struct flash_bank *bank)
{
	int retval;
	struct working_area *workarea = NULL;

	/*
	 * Load Magic numbers required for bootrom help function execution.
	 */
	retval = set_magic_numbers(bank);
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing magic numbers to target.");
		goto err_write_code;
	}

	struct reg_param reg_params[1];
	struct armv7m_algorithm armv7m_algo;

	/*
	 * Load erase code.
	 */
	retval = target_alloc_working_area(bank->target,
			sizeof(erase_sector_code), &workarea);
	LOG_DEBUG("workarea address: " TARGET_ADDR_FMT ".", workarea->address);
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

	/* wrapper function needs stack */
	init_reg_param(&reg_params[0], "sp", 32, PARAM_OUT);
	/* Set the sram stack. */
	buf_set_u32(reg_params[0].value, 0, 32, SRAM_PARAM_START);
	/* Run the code. */
	retval = target_run_algorithm(bank->target,
			0, NULL,
			ARRAY_SIZE(reg_params), reg_params,
			workarea->address, 0,
			5000, &armv7m_algo);
	uint32_t retvalT;
	int retval1 = target_read_u32(bank->target,
			(target_addr_t)(SRAM_PARAM_START + 0x10),
			&retvalT);
	if ((retval != ERROR_OK) || (retval1 != ERROR_OK) || (retvalT != 0)) {
		LOG_ERROR(
			"Error executing flash erase %d, RC1 %d, TRC %d.",
			retval,
			retval1,
			retvalT);
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
		etacorem3_bank->flash_base,	/**< Start of flash. */
		0x00000000,	/**< Length 0 for all. */
		0x00000001,	/**< Option 1, mass erase. */
		etacorem3_bank->bootrom_erase_entry,	/**< bootrom entry point. */
		BREAKPOINT	/**< Return code from bootrom. */
	};
	/* endian neutral call. */
	target_buffer_set_u32_array(target, (uint8_t *)SRAM_PARAM_START,
		(sizeof(eta_erase_interface)/sizeof(uint32_t)), (uint32_t *)&sramargs);
#if 0
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing target SRAM parameters.");
		goto err_alloc_code;
	}
#endif

	/* Common erase execution code. */
	retval = common_erase_run(bank);

	/* if successful, mark sectors as erased */
	if (retval == ERROR_OK)
		write_is_erased(bank,
			etacorem3_bank->flash_base,
			etacorem3_bank->flash_base + etacorem3_bank->flash_size,
			1);

	LOG_DEBUG("Mass erase on bank %d.", bank->bank_number);

/*err_alloc_code:*/
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

	LOG_DEBUG("ETA ECM35xx erase sectors %d to %d.", first, last);

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
		etacorem3_bank->flash_base + (first * etacorem3_bank->pagesize),
		(last - first + 1) * etacorem3_bank->pagesize,	/**< Length in bytes. */
		0x00000000,	/**< Request page erase. */
		etacorem3_bank->bootrom_erase_entry,	/**< bootrom entry point. */
		BREAKPOINT	/**< Return code from bootrom. */
	};
	target_buffer_set_u32_array(bank->target, (uint8_t *)SRAM_PARAM_START,
		(sizeof(eta_erase_interface)/sizeof(uint32_t)), (uint32_t *)&sramargs);
#if 0
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing target SRAM parameters.");
		goto err_alloc_code;
	}
#endif

	/* Common erase execution code. */
	retval = common_erase_run(bank);

	/* if successful, mark sectors as erased */
	if (retval == ERROR_OK)
		write_is_erased(bank, first, last, 1);
#if 0
err_alloc_code:
#endif

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
	struct etacorem3_flash_bank *etacorem3_bank = bank->driver_priv;
	struct target *target = bank->target;
	uint32_t address = bank->base + offset;
	uint32_t sram_buffer = SRAM_BUFFER_START;
	uint32_t maxbuffer;
	uint32_t thisrun_count;
	struct working_area *workarea = NULL;
	int retval = ERROR_OK;

	/* Bootrom uses 64 bit count. */
	if (((count%4) != 0) || ((offset%4) != 0)) {
		LOG_ERROR("write block must be multiple of 4 bytes in offset & length");
		return ERROR_FAIL;
	}

	/*
	 * Load Magic numbers required for bootrom help function execution.
	 */
	retval = set_magic_numbers(bank);
	if (retval != ERROR_OK) {
		CHECK_STATUS(retval, "error writing magic numbers to target.");
		goto err_write_code;
	}

	/*
	 * Max buffer size for this device...
	 * Bootrom can only write 512 bytes at a time.
	 * Target side code will block the write into 512 bytes.
	 */
	maxbuffer = SRAM_BUFFER_SIZE;

	struct reg_param reg_params[1];

	/*
	 * Allocate space on target for write code.
	 */
	retval = target_alloc_working_area(bank->target,
			sizeof(write_sector_code),
			&workarea);
	LOG_DEBUG("workarea address: " TARGET_ADDR_FMT ".", workarea->address);
	if (retval != ERROR_OK) {
		LOG_ERROR("No working area available.");
		retval = ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		goto err_alloc_code;
	}
	/*
	 * Load code on target.
	 */
	retval = target_write_buffer(bank->target, workarea->address,
			sizeof(write_sector_code), write_sector_code);
	if (retval != ERROR_OK)
		goto err_write_code;

	while (count > 0) {
		if (count > maxbuffer)
			thisrun_count = maxbuffer;
		else
			thisrun_count = count;

		/*
		 * Load target Write Buffer.
		 */
		retval = target_write_buffer(target, sram_buffer, thisrun_count, buffer);
		CHECK_STATUS_BREAK(retval, "error writing buffer to target.");

		LOG_DEBUG("address = 0x%08" PRIx32, address);

		/*
		 * Load SRAM Parameters.
		 */
		eta_write_interface sramargs = {
			address,	/**< Start address in flash. */
			thisrun_count,	/**< Length in bytes. */
			sram_buffer,
			etacorem3_bank->bootrom_write_entry,	/**< bootrom entry point. */
			0x00000001,	/**< Option 1, write flash in 512 byte blocks. */
			BREAKPOINT	/**< Return code from bootrom. */
		};
		target_buffer_set_u32_array(bank->target, (uint8_t *)SRAM_PARAM_START,
			(sizeof(eta_write_interface)/sizeof(uint32_t)), (uint32_t *)&sramargs);
#if 0
		if (retval != ERROR_OK) {
			CHECK_STATUS(retval, "error writing target SRAM parameters.");
			goto err_alloc_code;
		}
#endif
		struct armv7m_algorithm armv7m_algo;

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
				4000, &armv7m_algo);
		uint32_t retvalT;
		int retval1 = target_read_u32(bank->target,
				SRAM_PARAM_START + 0x14,/* byte offset to error code. */
				&retvalT);
		if ((retval != ERROR_OK) || (retval1 != ERROR_OK) || (retvalT != 0)) {
			LOG_ERROR(
				"Error executing flash erase %d, RC1 %d, TRC %d.",
				retval,
				retval1,
				retvalT);
			LOG_DEBUG("address: 0x%08X, count: 0x%08X", address, thisrun_count);
			retval = ERROR_FLASH_OPERATION_FAILED;
			goto err_run;
		}
		buffer += thisrun_count;
		address += thisrun_count;
		count -= thisrun_count;
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
	LOG_WARNING("Cannot protect/unprotect.");
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
	/*
	* sectors are always unprotected.
	* set at initialization.
	*/
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
		LOG_DEBUG("Probing part.");

	/* Check bootrom version, get_variant sets default, no errors. */
	etacorem3_bank->target_name = "ECM3501";
	etacorem3_bank->pagesize = 4096;
	
	etacorem3_bank->fpga = get_variant(bank);
	if (etacorem3_bank->fpga == 0) {
		etacorem3_bank->bootrom_erase_entry = BOOTROM_FLASH_ERASE_BOARD;
		etacorem3_bank->bootrom_write_entry = BOOTROM_FLASH_PROGRAM_BOARD;
	/* Load for ECM3501. */
	etacorem3_bank->sram_base = ETA_COMMON_SRAM_BASE_ECM3501;
	etacorem3_bank->sram_max = ETA_COMMON_SRAM_MAX_ECM3501;
	etacorem3_bank->flash_base = ETA_COMMON_FLASH_BASE_ECM3501;
	etacorem3_bank->flash_max = ETA_COMMON_FLASH_MAX_ECM3501;

	} else if (etacorem3_bank->fpga == 1) {
		etacorem3_bank->bootrom_erase_entry = BOOTROM_FLASH_ERASE_FPGA;
		etacorem3_bank->bootrom_write_entry = BOOTROM_FLASH_PROGRAM_FPGA;
	/* Load for ECM3501. */
	etacorem3_bank->sram_base = ETA_COMMON_SRAM_BASE_ECM3501;
	etacorem3_bank->sram_max = ETA_COMMON_SRAM_MAX_ECM3501;
	etacorem3_bank->flash_base = ETA_COMMON_FLASH_BASE_ECM3501;
	etacorem3_bank->flash_max = ETA_COMMON_FLASH_MAX_ECM3501;

	} else if (etacorem3_bank->fpga == 2) {
		etacorem3_bank->target_name = "M3ETA";
		etacorem3_bank->bootrom_erase_entry = 0;
		etacorem3_bank->bootrom_write_entry = 0;
		etacorem3_bank->pagesize = 0;
	/* Load for M3ETA. */
	etacorem3_bank->sram_base = ETA_COMMON_SRAM_BASE_M3ETA;
	etacorem3_bank->sram_max = ETA_COMMON_SRAM_MAX_M3ETA;
	etacorem3_bank->flash_base = ETA_COMMON_FLASH_BASE_M3ETA;
	etacorem3_bank->flash_max = ETA_COMMON_FLASH_MAX_M3ETA;

	}
	/* Do a size test. */
	etacorem3_bank->sram_size  = get_memory_size(bank,
			etacorem3_bank->sram_base,
			etacorem3_bank->sram_max,
			(16*1024));
	etacorem3_bank->flash_size = get_memory_size(bank,
			etacorem3_bank->flash_base,
			etacorem3_bank->flash_max,
			(32*1024));
	if (etacorem3_bank->flash_size)
		etacorem3_bank->num_pages = etacorem3_bank->flash_size / etacorem3_bank->pagesize;
	/* provide this for the benefit of the NOR flash framework */
	bank->base = (bank->bank_number * etacorem3_bank->flash_size) + \
		etacorem3_bank->flash_base;
	bank->size = etacorem3_bank->pagesize * etacorem3_bank->num_pages;
	bank->num_sectors = etacorem3_bank->num_pages;

	LOG_DEBUG("bank number: %d, base: 0x%08" PRIx32
		", size: %d KB, num sectors: %d.",
		bank->bank_number,
		bank->base,
		bank->size/1024,
		bank->num_sectors);

	if (bank->sectors) {
		free(bank->sectors);
		bank->sectors = NULL;
	}

	if (bank->num_sectors != 0) {
		bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
		for (int i = 0; i < bank->num_sectors; i++) {
			bank->sectors[i].offset = i * etacorem3_bank->pagesize;
			bank->sectors[i].size = etacorem3_bank->pagesize;
			bank->sectors[i].is_erased = -1;
			/* No flash protect in this hardware. */
			bank->sectors[i].is_protected = 0;
		}
	}

	etacorem3_bank->probed = true;
	return ERROR_OK;
}

static int etacorem3_auto_probe(struct flash_bank *bank)
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
			"\nETA Compute %s. %s"
			"\n\tTotal Flash: %d KB, Sram: %d KB."
			"\n\tStart Flash: 0x%08X, Sram: 0x%08X.",
			etacorem3_bank->target_name,
			((etacorem3_bank->fpga == 1) ? "FPGA" : ""),
			(etacorem3_bank->flash_size/1024),
			(etacorem3_bank->sram_size/1024)
			,
			etacorem3_bank->flash_base,
			etacorem3_bank->sram_base
			);

	if (printed < 0)
		return ERROR_BUF_TOO_SMALL;
	return ERROR_OK;
}

/*
 * openocd command interface
 */

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

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	etacorem3_bank = calloc(1, sizeof(*etacorem3_bank));
	if (!etacorem3_bank)
		return ERROR_FLASH_OPERATION_FAILED;

	bank->driver_priv = etacorem3_bank;
	etacorem3_bank->probed = false;

	return ERROR_OK;
}

/**
 * Handle external mass erase command.
 * @returns ERROR_COMMAND_SYNTAX_ERROR or ERROR_OK.
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
 * Register exec commands, extensions to standard OCD commands.
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
	COMMAND_REGISTRATION_DONE
};

/**
 * Register required commands and chain to optional exec commands.
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
	.auto_probe = etacorem3_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = etacorem3_protect_check,
	.info = get_etacorem3_info,
};
