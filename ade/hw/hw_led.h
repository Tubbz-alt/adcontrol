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
 * All Rights Reserved.
 * -->
 *
 * \brief Led on/off macros.
 *
 * \author Daniele Basile <asterix@develer.com>
 */

#ifndef HW_LED_H
#define HW_LED_H

#include <avr/io.h>

#define LED_ON()		(PORTB &= ~BV8(PB1))
#define LED_OFF()		(PORTB |=  BV8(PB1))
#define LED_SWITCH()	(PINB  |=  BV8(PB1))

#define LED_GSM_OFF() do {\
	volatile uint8_t reg = PORTA;\
	PORTA = (reg | 0xF0);\
} while(0)
#define LED_GSM_CSQ(level) do {\
	volatile uint8_t reg = PORTA;\
	PORTA = ((reg | 0xF0) & ~BV8(4+(level)));\
} while(0)

#define LED_NOTIFY_ON()  PORTA &= ~0xF0
#define LED_NOTIFY_OFF() PORTA |=  0xF0

#define ERR_ON()		(PORTB |=  BV8(PB0))
#define ERR_OFF()		(PORTB &= ~BV8(PB0))

#define LED_INIT() do {\
	/* Function LED */\
	DDRB  |=  BV8(PB1);\
	PORTB |=  BV8(PB1);\
	/* Fault LED (and rele) */\
	DDRB  |=  BV8(PB0);\
	PORTB &= ~BV8(PB0);\
	/* GMS Link status*/\
	DDRA  |= (BV8(PB4)|BV8(PB5)|BV8(PB6)|BV8(PB7));\
	PORTA |= (BV8(PB4)|BV8(PB5)|BV8(PB6)|BV8(PB7));\
} while(0)

#endif /* HW_LED_H */
