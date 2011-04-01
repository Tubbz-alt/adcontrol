
#include "console.h"
#include "eeprom.h"
#include "control.h"

#include "gsm.h"

#include "hw/hw_led.h"

#include <cfg/debug.h>
#include <cfg/macros.h>

#include <avr/io.h>
#include <avr/wdt.h>
#include <cpu/irq.h>

#include <drv/meter_ade7753.h>
#include <drv/ser.h>
#include <drv/timer.h>


#include <verstag.h>

static Serial spi_port;
static Serial gsm_port;
Serial dbg_port;

static void init(void) {

	LED_INIT();

	kdbg_init();
	timer_init();

	MCUSR = 0;
	wdt_disable();

	IRQ_ENABLE;

}

int main(void) {

	init();
	LED_ON();

	kprintf("RFN (c) 2011 RCT\r\nBuildNr %d\r\n", vers_build_nr);

	/* Open the Console port */
	ser_init(&dbg_port, SER_UART0);
	ser_setbaudrate(&dbg_port, 115200);

	/* Open the GSM port */
	ser_init(&gsm_port, SER_UART1);
	ser_setbaudrate(&gsm_port, 115200);

	/* Initialize ADE7753 SPI port and data structure */
	spimaster_init(&spi_port, SER_SPI);
	ser_setbaudrate(&spi_port, 500000L);
	meter_ade7753_init((KFile *)&spi_port);

	/* Entering the main control loop */
	controlSetup();
	while(1) {
		controlLoop();
	}

#if 0
	ee_dumpConf(&dbg_port.fd);

	gsmPortingTest(&gsm_port);

	console_init(&dbg_port.fd);
	while(1) {
		console_run(&dbg_port.fd);
	}
#endif

	return 0;
}

