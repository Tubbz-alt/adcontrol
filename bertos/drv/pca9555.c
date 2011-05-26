/**
 * \file
 * <!--
 * This file is part of BeRTOS.
 *
 * Bertos is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 *
 * Copyright 2007 Develer S.r.l. (http://www.develer.com/)
 *
 * -->
 *
 * \brief PCA9555 I2C port expander driver.
 *
 * This driver controls the PCA9555.
 * The PCA9555 is an 16bit I2C port expander.
 * You can read/write 16 pins through an I2C bus.
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 */

#include "pca9555.h"

#include "hw/hw_pca9555.h"

#define LOG_LEVEL  PCA9555_LOG_LEVEL
#define LOG_FORMAT PCA9555_LOG_FORMAT
#include <cfg/log.h>

#include <cfg/module.h>

#include <drv/i2c.h>
#include <drv/timer.h>

/**
 * Read PCA9555 \a specified register.
 * \return the true if ok, false on errors.
 */
bool pca9555_get_4(I2c *i2c, Pca9555 *pca, uint8_t reg, uint16_t *regs)
{

	i2c_start_w(i2c, PCA9555ID | ((pca->addr << 1) & 0x0E), 1, I2C_NOSTOP);
	i2c_putc(i2c, reg);
	if (i2c_error(i2c))
		return false;

	i2c_start_r(i2c, PCA9555ID | ((pca->addr << 1) & 0x0E), 2, I2C_STOP);
	i2c_read(i2c, (void*)regs, 2);
	if (i2c_error(i2c))
		return false;

	return true;
}

/**
 * Write to PCA9555 \a pca port \a data.
 * \return true if ok, false on errors.
 */
bool pca9555_put_4(I2c *i2c, Pca9555 *pca, uint8_t reg, uint16_t data)
{
	i2c_start_w(i2c, PCA9555ID | ((pca->addr << 1) & 0x0E), 3, I2C_STOP);
	i2c_putc(i2c, reg);
	i2c_write(i2c, (void*)&data, 2);

	if (i2c_error(i2c))
		return false;

	return true;
}

/**
 * Init a PCA9555 on the bus with addr \a addr.
 * \return true if device is found, false otherwise.
 */
bool pca9555_init_3(I2c *i2c, Pca9555 *pca, pca9555_addr addr)
{
	uint16_t regs;

	ASSERT(i2c);
	pca->addr = addr;

	PCA9555_HW_INIT();

	return pca9555_get(i2c, pca, PCA9555_REG_DIRECTION, &regs);
}

#if CONFIG_PCA9555_TESTING
# warning PCA9555 TESTING ENABLED
void NORETURN pca9555_testing(I2c *i2c_bus, Pca9555 *pe) {
	uint16_t in = 0xBEEF;

	kprintf("PCA9555 Test\r\n");

	while(1) {
		timer_delay(1000);
		pca9555_in(i2c_bus, pe, &in);
		kprintf("IN 0x%04X\r\n", in);
	}

}
#endif

