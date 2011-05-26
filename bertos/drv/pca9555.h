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
 * \author Patrick Bellasi <derkling@gmail.com>
 *
 * $WIZ$ module_name = "pca9555"
 * $WIZ$ module_depends = "i2c"
 */

#ifndef DRV_PCA9555_H
#define DRV_PCA9555_H

#include "cfg/cfg_i2c.h"
#include "cfg/cfg_pca9555.h"

#include <cfg/compiler.h>

#include <drv/i2c.h>

#if COMPILER_C99
	#define pca9555_init(...)       PP_CAT(pca9555_init ## _, COUNT_PARMS(__VA_ARGS__)) (__VA_ARGS__)
	#define pca9555_get(...)        PP_CAT(pca9555_get ## _, COUNT_PARMS(__VA_ARGS__)) (__VA_ARGS__)
	#define pca9555_put(...)        PP_CAT(pca9555_put ## _, COUNT_PARMS(__VA_ARGS__)) (__VA_ARGS__)
#else
	#define pca9555_init(args...)   PP_CAT(pca9555_init ## _, COUNT_PARMS(args)) (args)
	#define pca9555_get(args...)    PP_CAT(pca9555_get ## _, COUNT_PARMS(args)) (args)
	#define pca9555_put(args...)    PP_CAT(pca9555_put ## _, COUNT_PARMS(args)) (args)
#endif

#define PCA9555_REG_INPUT       0
#define PCA9555_REG_OUTPUT      2
#define PCA9555_REG_POLARITY    4
#define PCA9555_REG_DIRECTION   6

#define pca9555_in(i2c, pca, inputs)\
	pca9555_get_4(i2c, pca, PCA9555_REG_INPUT, inputs)
#define pca9555_dir(i2c, pca, mask)\
	pca9555_put_4(i2c, pca, PCA9555_REG_DIRECTION, mask)
#define pca9555_out(i2c, pca, value)\
	pca9555_put_4(i2c, pca, PCA9555_REG_OUTPUT, value)

typedef uint8_t pca9555_addr;

/**
 * Context for accessing a PCA9555.
 */
typedef struct Pca9555
{
	pca9555_addr addr;
} Pca9555;

#define PCA9555ID 0x40 ///< I2C address

/**
 * Read PCA9555 \a specified register.
 * \return the true if ok, false on errors.
 */
bool pca9555_get_4(I2c *i2c, Pca9555 *pca, uint8_t reg, uint16_t *regs);

/**
 * Write to PCA9555 \a pca port \a data.
 * \return true if ok, false on errors.
 */
bool pca9555_put_4(I2c *i2c, Pca9555 *pca, uint8_t reg, uint16_t data);

/**
 * Init a PCA9555 on the bus with addr \a addr.
 * \return true if device is found, false otherwise.
 */
bool pca9555_init_3(I2c *i2c, Pca9555 *pca, pca9555_addr addr);

#if CONFIG_PCA9555_TESTING
void pca9555_testing(I2c *i2c_bus, Pca9555 *pe);
#else
# define pca9555_testing(i2c_bus, pe)
#endif


#endif /* DRV_PCA9555_H */
