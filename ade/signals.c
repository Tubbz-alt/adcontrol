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

// The mask of enabled Interrupt pins on PORTC
static const uint8_t portc_mask = 0xFC;
// The mask of pulled-up Interrupt pins on PORTC
static const uint8_t portc_pullups = 0x7C;
// The mask of (default) enabled Interrupt pins on PORTC
static const uint8_t signals_enabled = 0x3C;
// The bit values on PORTC
uint8_t signals_status = 0x00;
// The mask of signals which are pedning (waiting to be processed)
uint8_t signals_pending = 0x00;

#define PORTC_READ (portc_mask & PINC)

#define SET_SIGNAL(NUM)\
	(signals_pending |= BV8(SIGNAL_##NUM))

#define SET_AND_DISABLE(NUM)\
	signals_pending |= BV8(SIGNAL_##NUM);\
	PCMSK2 &= ~BV8(SIGNAL_##NUM)


//----- Signals utility functions

// Wait for the specified signal
void signal_wait(uint8_t sig) {
	signal_enable(sig);
//kprintf("\nSingnals: e(0x%02X), s(0x%02X)\n", PCMSK2, signals_pending);
	while (!signal_pending(sig)) {
		cpu_relax();
//		DB(
//				DELAY(100);
//				kprintf("v");
//		)
	}
}

//----- Setup interrupt controller
void signals_init(void) {

	// Enable required pull-ups
	PORTC |=  portc_pullups;
	// Configure PINs as INPUT
	DDRC  &= ~portc_mask;
	// Enable required interrupts PCINT[18..19]
	PCMSK2 = signals_enabled;
	// Getting current status of PORTC
	signals_status = PORTC_READ;
	// Enable interrupt on PORTC
	PCICR |= BV8(PCIE2);

}

//----- Interrupt handlers
void intr_ade_zx(uint8_t level) {
//PINA = 0x80;
	(void)level;
	SET_AND_DISABLE(ADE_ZX);
}

void intr_ade_irq(uint8_t level) {
	(void)level;
	SET_AND_DISABLE(ADE_IRQ);
}

void intr_rtc_irq(uint8_t level) {
	(void)level;
	SET_AND_DISABLE(RTC_IRQ);
}

void intr_unit_irq(uint8_t level) {
//PINA = 0x40;
	(void)level;
	SET_AND_DISABLE(UNIT_IRQ);
}

void intr_plat_button(uint8_t level) {
//PINA = 0x20;
	(void)level;
	SET_SIGNAL(PLAT_BUTTON);
}

void intr_plat_i2c(uint8_t level) {
//PINA = 0x10;
	(void)level;
	SET_SIGNAL(PLAT_I2C);
}


#if CONFIG_SIG_TESTING
# warning SIGNALS TESTING ENABLED
void sigTesting() {

	LOG_INFO(".:: External Interrupt Testing\r\n");

	while(1) {

		DELAY(1000);

		LOG_INFO("PINC: 0x%02X\r\n", PINC);
		if (signal_pending(SIGNAL_PLAT_BUTTON)) {
			LOG_INFO("EVT: Button [%hd]\r\n",
					signal_status(SIGNAL_PLAT_BUTTON));
		}
		if (signal_pending(SIGNAL_UNIT_IRQ)) {
			LOG_INFO("EVT: RCT Unit Fault [%hd]\r\n",
					signal_status(SIGNAL_UNIT_IRQ));
		}
	}
}
#endif

typedef void (*intrHandler_t)(uint8_t level);

// Interrupt handlers on PORTC
intrHandler_t intrTable[] = {
	intr_plat_i2c, 			// PC2 - I2C Interrupt
	intr_plat_button,		// PC3 - User Button
	intr_unit_irq,			// PC4 - RCT Unit Fault
	intr_rtc_irq,			// PC5 - RTC Interrupt
	intr_ade_irq,			// PC6 - ADE Interrupt
	intr_ade_zx				// PC7 - ADE Zero Crossing
};

//----- Low-level Interrupt handler
DECLARE_ISR_CONTEXT_SWITCH(PCINT2_vect) {
	uint8_t portc_levels = PORTC_READ;
	uint8_t portc_changed;

	// Getting changes bits
	portc_changed = (signals_status ^ portc_levels);

	// Calling handler of changed signals
	for (uint8_t sig = 0x04, i = 0; sig; sig<<=1, i++) {
		if (portc_changed & sig)
			(*intrTable[i])(portc_levels);
	}

	signals_status = portc_levels;

}

