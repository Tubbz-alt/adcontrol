/**
 *       @file  signals.h
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

#ifndef ADE_SIGNALS_H_
#define ADE_SIGNALS_H_

#include <cfg/macros.h>
#include <cfg/cfg_sig.h>
#include <avr/io.h>

// Signals on PORTD [8..15]
#define SIGNAL_GSM_RI		56
// Signals on PORTC [0..7]
#define SIGNAL_ADE_ZX 		 7
#define SIGNAL_ADE_IRQ  	 6
#define SIGNAL_RTC_IRQ		 5
#define SIGNAL_UNIT_IRQ		 4
#define SIGNAL_PLAT_BUTTON 	 3
#define SIGNAL_PLAT_I2C		 2

#define SIGNAL_PORTC 		 0
#define SIGNAL_PORTD 		 1
#define SIGNAL_PORT_COUNT    2

// The signal flag for each pin on the respective port
extern uint8_t signals_pending[SIGNAL_PORT_COUNT];
extern uint8_t signals_status[SIGNAL_PORT_COUNT];
extern volatile unsigned char *port_PCMSK[SIGNAL_PORT_COUNT];

// Verify if a signal has been set
inline uint8_t signal_pending(uint8_t sig);
inline uint8_t signal_pending(uint8_t sig) {
	uint8_t port = 0;
	uint8_t result;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return 0;

	result = (signals_pending[port] & BV8(sig));
	if (result) {
		signals_pending[port] &= ~BV8(sig);
		return 1;
	}

	return 0;
}

// Get the value of the PIN
inline uint8_t signal_status(uint8_t sig);
inline uint8_t signal_status(uint8_t sig) {
	uint8_t port = 0;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return 0;

	if (signals_status[port] & BV8(sig))
		return 1;

	return 0;
}

// Enable the specified signal
inline void signal_enable(uint8_t sig);
inline void signal_enable(uint8_t sig) {
	uint8_t port = 0;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return;

	signals_pending[port] &= ~BV8(sig);
	*port_PCMSK[port] |= BV8(sig);

#if 0
	if (sig & 0x07) {
		// Reset signal flag
		signals_pending[SIGNAL_PORTC] &= ~BV8(sig);
		// Enable signal interrupt
		PCMSK2 |= BV8(sig);
	}
	sig >>= 3;
	if (sig & 0x07) {
		// Reset signal flag
		signals_pending[SIGNAL_PORTD] &= ~BV8(sig);
		// Enable signal interrupt
		PCMSK3 |= BV8(sig);
	}
#endif
}

// Disable the specified signal
inline void signal_disable(uint8_t sig);
inline void signal_disable(uint8_t sig) {
	uint8_t port = 0;

	while (!(sig & 0x07)) {
		port++;
		sig >>= 3;
	}

	if (port >= SIGNAL_PORT_COUNT)
		return;

	*port_PCMSK[port] &= ~BV8(sig);

#if 0
	if (sig & 0x07) {
		PCMSK2 &= ~BV8(sig);
		return;
	}
	sig >>= 3;
	if (sig & 0x07) {
		PCMSK3 &= ~BV8(sig);
	}
#endif
}

#if CONFIG_SIG_TESTING
void sigTesting(void);
#else
# define sigTesting()
#endif

void signals_init(void);
void signal_wait(uint8_t sig);


#endif /* end of include guard: ADE_SIGNALS_H_ */

