
#include "console.h"
#include "eeprom.h"

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
static Serial dbg_port;

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

	kprintf("ACSense\nBuildNr %d\n", vers_build_nr);

	/* Open the Console port */
	ser_init(&dbg_port, SER_UART0);
	ser_setbaudrate(&dbg_port, 115200);

	/* Initialize ADE7753 SPI port and data structure */
	spimaster_init(&spi_port, SER_SPI);
	ser_setbaudrate(&spi_port, 500000L);
	//meter_ade7753_init((KFile *)&spi_port);

	ee_dumpConf(&dbg_port.fd);

	console_init(&dbg_port.fd);
	while(1) {
		console_run(&dbg_port.fd);
	}

	return 0;
}

