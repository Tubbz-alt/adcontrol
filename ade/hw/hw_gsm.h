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

#warning TODO:This is an example implementation, you must implement it!

#include <cfg/compiler.h>
//#include <cfg/macros.h>

#include <drv/timer.h>
#include <avr/io.h>

#define POWER_DIR    DDRD
#define POWER_PORT   PORTD
#define POWER_PIN    PD4
#define RESET_DIR    DDRD
#define RESET_PORT   PORTD
#define RESET_PIN    PD5
#define STATUS_DIR   DDRA
#define STATUS_PORT  PORTA
#define STATUS_PIN   PA7

#define SET_LOW(SIG)  (BIT_CHANGE(SIG##_PORT, (SIG##_PIN, 0)))
#define SET_HIGH(SIG) (BIT_CHANGE(SIG##_PORT, (SIG##_PIN, 1)))
#define SET_OUT(SIG)  (BIT_CHANGE(SIG##_DIR, (SIG##_PIN, 1)))
#define SET_IN(SIG)   (BIT_CHANGE(SIG##_DIR, (SIG##_PIN, 0)))


INLINE uint8_t gsm_statusIn(void)
{
	return (PINA & BV(PA7));
}

INLINE void gsm_powerOn(void)
{
	while ( !gsm_statusIn() ) {
		PORTD &= ~BV(PD4);
		DDRD |= BV(PD4);
		//SET_LOW(POWER);
		//SET_OUT(POWER);
		timer_delay(1500);
		//SET_IN(POWER);
		DDRD &= ~BV(PD4);
		timer_delay(2500);
	}
}

INLINE void gsm_powerOff(void)
{
	while ( gsm_statusIn() ) {
		PORTD &= ~BV(PD4);
		DDRD |= BV(PD4);
		//SET_LOW(POWER);
		//SET_OUT(POWER);
		timer_delay(1500);
		//SET_IN(POWER);
		DDRD &= ~BV(PD4);
		timer_delay(2000);
	}

}

INLINE void gsm_reset(void)
{
	SET_LOW(RESET);
	SET_OUT(RESET);
	timer_delay(10);
	SET_IN(RESET);
	timer_delay(2000);
}


/**
 * This macro should initialized the GSM controlling pins.
 */
INLINE void gsm_init(void)
{
	DDRD &= ~(BV(PD4)|BV(PD5));
	//SET_IN(RESET);
	//SET_IN(POWER);
	DDRA &= ~(BV(PA7));
	//SET_IN(STATUS);
}

#endif /* HW_GSM_H */
