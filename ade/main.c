#include <avr/io.h>

#include <cpu/irq.h>
#include <cfg/debug.h>

#include <drv/meter_ade7753.h>
#include <drv/ser.h>
#include <drv/timer.h>


#include <verstag.h>

#include "hw/hw_led.h"

Serial spi_port;

static void init(void)
{

	timer_init();
	kdbg_init();

	LED_INIT();

	spimaster_init(&spi_port, SER_SPI);
	ser_setbaudrate(&spi_port, 500000L);

	IRQ_ENABLE;

	meter_ade7753_init((KFile *)&spi_port);

	LED_ON();
}


int main(void)
{
	init();

	kputs("ACSense Test\n");
	kprintf("Program build: %d\n", vers_build_nr);

	while (1) {
		kputs(".");
		LED_SWITCH();
		timer_delay(1000);
	}

	return 0;
}

