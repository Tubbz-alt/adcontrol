/**
 *       @file  signals.c
 *      @brief  Interrupts handlers for external signal management
 *
 * This provides a set of utility function to handle external interrupts.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  05/07/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * ============================================================================
 */

#include "signals.h"

#include "hw/hw_led.h"

#include <cpu/irq.h>
#include <drv/timer.h>
#include <avr/interrupt.h>

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   SIG_LOG_LEVEL
#define LOG_FORMAT  SIG_LOG_FORMAT
#include <cfg/log.h>

// Forward declarations
void intr_ade_zx(uint8_t level);
void intr_ade_irq(uint8_t level);
void intr_rtc_irq(uint8_t level);
void intr_unit_irq(uint8_t level);
void intr_plat_button(uint8_t level);
void intr_plat_i2c(uint8_t level);
void intr_gsm_ri(uint8_t level);

// The mask of enabled Interrupt pins on PORTC
static const uint8_t port_mask[SIGNAL_PORT_COUNT] = {0xFC, 0x80};
// The mask of pulled-up Interrupt pins on PORTC
static const uint8_t port_pullups[SIGNAL_PORT_COUNT] = {0x7C, 0x80};
// The mask of (default) enabled Interrupt pins on PORTC
static const uint8_t port_signals_enabled[SIGNAL_PORT_COUNT] = {0x3C, 0x80};
// The PIN change mask
volatile unsigned char * PROGMEM port_PCMSK[SIGNAL_PORT_COUNT] =
	{&PCMSK2, &PCMSK3}; 

// The bit values
uint8_t signals_status[SIGNAL_PORT_COUNT] = {0x00, 0x00};
// The mask of signals which are pedning (waiting to be processed)
uint8_t signals_pending[SIGNAL_PORT_COUNT] = {0x00, 0x00};

#define PORTC_READ (port_mask[SIGNAL_PORTC] & PINC)
#define PORTD_READ (port_mask[SIGNAL_PORTD] & PIND)

void set_signal(uint8_t sig);
void set_signal(uint8_t sig) {
	uint8_t port = 0;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return;

	signals_pending[port] |= BV8(sig);
}

void set_and_disable(uint8_t sig);
void set_and_disable(uint8_t sig) {
	uint8_t port = 0;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return;

	signals_pending[port] |= BV8(sig);
	*port_PCMSK[port] &= ~BV8(sig);
}

//----- Signals utility functions

// Wait for the specified signal
void signal_wait(uint8_t sig) {
	signal_enable(sig);
	while (!signal_pending(sig)) {
		cpu_relax();
	}
}

//----- Setup interrupt controller
void signals_init(void) {

	// Enable required pull-ups
	PORTC |=  port_pullups[SIGNAL_PORTC];
	// Configure PINs as INPUT
	DDRC  &= ~port_mask[SIGNAL_PORTC];
	// Enable required interrupts PCINT[18..19]
	PCMSK2 = port_signals_enabled[SIGNAL_PORTC];
	// Getting current status of PORTC
	signals_status[SIGNAL_PORTC] = PORTC_READ;

	// Enable required pull-ups
	PORTD |=  port_pullups[SIGNAL_PORTD];
	// Configure PINs as INPUT
	DDRD  &= ~port_mask[SIGNAL_PORTD];
	// Enable required interrupts PCINT[18..19]
	PCMSK3 = port_signals_enabled[SIGNAL_PORTD];
	// Getting current status of PORTC
	signals_status[SIGNAL_PORTD] = PORTD_READ;

	// Enable interrupt on PORTC and PORTD
	PCICR |= (BV8(PCIE2) | BV8(PCIE3));

}

//----- Interrupt handlers
void intr_ade_zx(uint8_t level) {
	(void)level;
	set_and_disable(SIGNAL_ADE_ZX);
}

void intr_ade_irq(uint8_t level) {
	(void)level;
	set_and_disable(SIGNAL_ADE_IRQ);
}

void intr_rtc_irq(uint8_t level) {
	(void)level;
	set_and_disable(SIGNAL_RTC_IRQ);
}

void intr_unit_irq(uint8_t level) {
	(void)level;
	set_and_disable(SIGNAL_UNIT_IRQ);
}

void intr_plat_button(uint8_t level) {
	(void)level;
	set_signal(SIGNAL_PLAT_BUTTON);
}

void intr_plat_i2c(uint8_t level) {
	(void)level;
	set_signal(SIGNAL_PLAT_I2C);
}

void intr_gsm_ri(uint8_t level) {
	(void)level;
	set_and_disable(SIGNAL_GSM_RI);
}

#if CONFIG_SIG_TESTING
# warning SIGNALS TESTING ENABLED
void sigTesting() {

	LOG_INFO(".:: External Interrupt Testing\r\n");

	while (1) {

		timer_delay(1000);

		LOG_INFO("PINC: 0x%02X, PIND: 0x%02X\r\n", PINC, PIND);
		if (signal_pending(SIGNAL_PLAT_BUTTON)) {
			LOG_INFO("EVT: Button [%hd]\r\n",
					signal_status(SIGNAL_PLAT_BUTTON));
		}
		if (signal_pending(SIGNAL_UNIT_IRQ)) {
			LOG_INFO("EVT: RCT Unit Fault [%hd]\r\n",
					signal_status(SIGNAL_UNIT_IRQ));
		}
		if (signal_pending(SIGNAL_GSM_RI)) {
			LOG_INFO("EVT: GSM RI [%hd]\r\n",
					signal_status(SIGNAL_GSM_RI));
		}
	}
}
#endif

typedef void (*intrHandler_t)(uint8_t level);

// Interrupt handlers on PORTC
intrHandler_t intrTable[] = {
	//--- PORTC
	intr_plat_i2c, 			// PC2 - I2C Interrupt
	intr_plat_button,		// PC3 - User Button
	intr_unit_irq,			// PC4 - RCT Unit Fault
	intr_rtc_irq,			// PC5 - RTC Interrupt
	intr_ade_irq,			// PC6 - ADE Interrupt
	intr_ade_zx,			// PC7 - ADE Zero Crossing
	//--- PORTD
	intr_gsm_ri				// PD7 - GSM Ring
};

//----- Low-level Interrupt handler
DECLARE_ISR_CONTEXT_SWITCH(PCINT2_vect) {
	uint8_t portc_levels = PORTC_READ;
	uint8_t portc_changed;

	// Getting changes bits
	portc_changed = (signals_status[SIGNAL_PORTC] ^ portc_levels);

	// Calling handler of changed signals
	for (uint8_t sig = 0x04, i = 0; sig; sig<<=1, i++) {
		if (portc_changed & sig)
			(*intrTable[i])(portc_levels);
	}

	signals_status[SIGNAL_PORTC] = portc_levels;

}

DECLARE_ISR_CONTEXT_SWITCH(PCINT3_vect) {
	uint8_t portd_levels = PORTD_READ;
	uint8_t portd_changed;

	// Getting changes bits
	portd_changed = (signals_status[SIGNAL_PORTD] ^ portd_levels);

	// Calling handler of changed signals
	for (uint8_t sig = 0x80, i = 6; sig; sig<<=1, i++) {
		if (portd_changed & sig)
			(*intrTable[i])(portd_levels);
	}

	signals_status[SIGNAL_PORTD] = portd_levels;

}
