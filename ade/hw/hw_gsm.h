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
 * Copyright 2003, 2004, 2006, 2008 Develer S.r.l. (http://www.develer.com/)
 * Copyright 2000 Bernie Innocenti <bernie@codewiz.org>
 * All Rights Reserved.
 * -->
 *
 * \brief Macro for GSM pins control operations.
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 */

#ifndef HW_GMS_H
#define HW_GMS_H

//#warning TODO:This is an example implementation, you must implement it!

#include <cfg/compiler.h>
//#include <cfg/macros.h>

#include <drv/timer.h>
#include <avr/io.h>

#define STATUS_DIR   DDRD
#define STATUS_PORT  PORTD
#define STATUS_PIN   PD4
#define POWER_DIR    DDRD
#define POWER_PORT   PORTD
#define POWER_PIN    PD5
#define RESET_DIR    DDRD
#define RESET_PORT   PORTD
#define RESET_PIN    PD6

#define SET_LOW(SIG)  (BIT_CHANGE(SIG##_PORT, (SIG##_PIN, 0)))
#define SET_HIGH(SIG) (BIT_CHANGE(SIG##_PORT, (SIG##_PIN, 1)))
#define SET_OUT(SIG)  (BIT_CHANGE(SIG##_DIR, (SIG##_PIN, 1)))
#define SET_IN(SIG)   (BIT_CHANGE(SIG##_DIR, (SIG##_PIN, 0)))


INLINE uint8_t gsm_status(void)
{
	return (PIND & BV8(PD4));
}

INLINE void gsm_on(void)
{
	while ( !gsm_status() ) {
		PORTD &= ~BV8(PD5);
		DDRD |= BV8(PD5);
		//SET_LOW(POWER);
		//SET_OUT(POWER);
		DELAY(1500);
		//SET_IN(POWER);
		DDRD &= ~BV8(PD5);
		DELAY(2500);
	}
}

INLINE void gsm_off(void)
{
	while ( gsm_status() ) {
		PORTD &= ~BV8(PD5);
		DDRD |= BV8(PD5);
		//SET_LOW(POWER);
		//SET_OUT(POWER);
		DELAY(1500);
		//SET_IN(POWER);
		DDRD &= ~BV8(PD5);
		DELAY(2000);
	}

}

INLINE void gsm_reset(void)
{
	PORTD &= ~BV8(PD6);
	DDRD |= BV8(PD6);
	//SET_LOW(RESET);
	//SET_OUT(RESET);
	DELAY(10);
	//SET_IN(RESET);
	DDRD &= ~BV8(PD6);
	DELAY(2000);
}

/**
 * This macro should initialized the GSM controlling pins.
 */
INLINE void gsm_init(void)
{
// RST PD6
// PWR PD5
// STS PD4
	DDRD &= ~(BV8(PD4)|BV8(PD5)|BV8(PD6));
	//SET_IN(RESET);
	//SET_IN(POWER);
	//SET_IN(STATUS);
	PORTD &= ~(BV8(PD2)|BV8(PD3));
}

#endif /* HW_GSM_H */
