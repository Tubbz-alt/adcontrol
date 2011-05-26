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
 * Copyright 2010 Develer S.r.l. (http://www.develer.com/)
 *
 * -->
 *
 * \brief ADE7753 low-level hardware macros
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 */

#ifndef HW_ADE7753_H
#define HW_ADE7753_H

#include <avr/io.h>

#include "cfg/macros.h"   /* BV() */


/**
 * \name ADE7753 I/O pins/ports
 * @{
 */
#define ADE7753_RESET_PIN   0 /* Implement me! */
#define ADE7753_CS_PIN      0 /* Implement me! */
#define ADE7753_IRQ_PIN     0 /* Implement me! */
#define ADE7753_SAG_PIN     0 /* Implement me! */
#define ADE7753_ZX_PIN      0 /* Implement me! */
#define ADE7753_CF_PIN      0 /* Implement me! */
/*@}*/

/**
 * \name ADE7753 chip control macros
 * @{
 */
#define ADE7753_RESET_HIGH()     do { /* Implement me! */ } while (0)
#define ADE7753_RESET_LOW()      do { /* Implement me! */ } while (0)
#define ADE7753_CS_HIGH()        do {\
	PORTB |=  BV8(PB4);\
} while(0)
#define ADE7753_CS_LOW()         do {\
	PORTB &= ~BV8(PB4);\
} while(0)
/*@}*/

/**
 * Setup I/O lines where ADE7753 is connected.
 */
INLINE void meter_ade7753_hw_bus_init(void)
{
	DDRB |= BV8(PB4);
	ADE7753_CS_HIGH();
}

#endif /* HW_ADE7753_H */
