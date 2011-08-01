
#include "console.h"
#include "eeprom.h"
#include "control.h"
#include "signals.h"

#include "gsm.h"

#include "hw/hw_led.h"

#include <cfg/debug.h>
#include <cfg/macros.h>

#include <avr/io.h>
#include <avr/wdt.h>
#include <cpu/irq.h>

#include <drv/meter_ade7753.h>
#include <drv/pca9555.h>
#include <drv/ser.h>
#include <drv/timer.h>

#include <verstag.h>

static Serial spi_port;
static Serial gsm_port;
Serial dbg_port;

I2c i2c_bus;
Pca9555 pe;

// Reset status
uint8_t rst_reason = 0;
void getResetReason(void);
void getResetReason(void) {
      rst_reason = MCUSR;
      MCUSR = 0x00;
      wdt_disable();
}

const char rst_reasons[] = "PEBWJ";
static void printResetReason(void) {
        kprintf("Reset reasons [0x%02X]: ", rst_reason);
		for (uint8_t i = 0; i<5; i++) {
			if (rst_reason & BV8(i))
				kprintf("%c", rst_reasons[i]);
		} 
        kprintf("\r\n");
}

static void init(void) {

	getResetReason();

	// Setting LEDs pins (PB1, PA4..7)
	LED_INIT();

	// Setting AMUX pins  (PA0..3)
	DDRA  |= 0x0F;
	PORTA &= 0xF0;
	// Select AMUX CH[1] => 8
	PORTA |= 0x08;

	kdbg_init();
	timer_init();
	signals_init();

	IRQ_ENABLE;

}

int main(void) {

	init();
	LED_ON();

	kprintf("RFN (c) 2011 RCT\r\nBuildNr %d\r\n", vers_build_nr);
	printResetReason();

	// Testing LEDs
	for (uint8_t i = 5; i; --i) {
		LED_GSM_CSQ(0);
		timer_delay(100);
		LED_GSM_CSQ(1);
		timer_delay(100);
		LED_GSM_CSQ(2);
		timer_delay(100);
		LED_GSM_CSQ(3);
		timer_delay(100);
	}

	/* Open the Console port */
	ser_init(&dbg_port, SER_UART0);
	ser_setbaudrate(&dbg_port, 115200);

	/* Initialize I2C bus and PCA9555 (addr=0) */
	i2c_init(&i2c_bus, I2C0, CONFIG_I2C_FREQ);
	pca9555_init(&i2c_bus, &pe, 0);

	/* Open the GSM port */
	ser_init(&gsm_port, SER_UART1);
	ser_setbaudrate(&gsm_port, 115200);
	LED_GSM_OFF();

	/* Initialize ADE7753 SPI port and data structure */
	spimaster_init(&spi_port, SER_SPI);
	ser_setbaudrate(&spi_port, 500000L);
	meter_ade7753_init((KFile *)&spi_port);

	/* Testing SIGNALS (if enabled by configuration) */
	sigTesting();
	/* Testing GSM (if enabled by configuration) */
	gsmTesting(&gsm_port);
	/* Testing PCA9555 (if enabled by configuration) */
	pca9555_testing(&i2c_bus, &pe);
	/* Testing CHANNELS (if enabled by configuration) */
	chsTesting();

#if 1
	/* Power-on Modem */
	gsmInit(&gsm_port);
	gsmPowerOn();
	gsmSMSConf(0);
#endif

	/* Entering the main control loop */
	controlSetup();
	while(1) {
		controlLoop();
	}

	return 0;
}

