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

meter_conf_t conf;
unsigned char irms[3];
unsigned char vrms[3];
unsigned char power[3];

uint32_t irms_value;
uint32_t vrms_value;
uint32_t power_value;


static void NORETURN monitor_load(void) {
	uint32_t irms_value;
	uint32_t max_load = 0;
	const uint32_t fault_load = 6000l;
	uint8_t count = 0;
	uint8_t notify = 1;

	kputs("Calibrating...\n");
	while (count++ < 10) {
		irms_value  = meter_ade7753_Irms();
		kprintf("Irms: %08ld, Ipeak: %08ld\n", irms_value, max_load);
		timer_delay(100);
		LED_SWITCH();
	}

	kputs("Sensing...\n");
	while (1) {

		irms_value  = meter_ade7753_Irms();
		kprintf("Irms: %08ld, Ipeak: %08ld\n", irms_value, max_load);

		if (max_load < irms_value) {

			if ( (irms_value-max_load) > fault_load)
				max_load += (fault_load>>2);
			else
				max_load = irms_value;

			notify = 1;

		} else {
			if ( (max_load-irms_value) > fault_load) {
				if (notify) {
					kprintf("Load fault (%8ld => %08ld): sending SMS notification...\n",
								max_load, irms_value);
					max_load = irms_value;
					notify = 0;
				}
			} else {
				if ( !(++count%10) ) {
					kprintf("Load OK, (%8ld)\n", max_load);
				}
			}

		}

		timer_delay(500);
		LED_SWITCH();
	}



}


int main(void)
{
	init();

	kputs("ACSense Test\n");
	kprintf("Program build: %d\n", vers_build_nr);

	meter_ade7753_conf(&conf);
	kprintf("Rev: %#02X, Mode %#04X, IRQs %#04X\n",
			conf.rev,
			(conf.mode[0]*256)+conf.mode[1],
			(conf.irqs[0]*256)+conf.irqs[1]);

	kputs("Sensing loop...\n");
	monitor_load();










#if 0
	while (1) {

		meter_ade7753_Irms(irms);
		meter_ade7753_Vrms(vrms);
    	meter_ade7753_Power(power);

		irms_value  = ((uint32_t)irms[0]<<16)+((uint32_t)irms[1]<<8)+(uint32_t)irms[2];
		vrms_value  = ((uint32_t)vrms[0]<<16)+((uint32_t)vrms[1]<<8)+(uint32_t)vrms[2];
		//power_value  = ((uint32_t)(prms[0]&0xEF)<<16)+ ((uint32_t)prms[1]<<8)+(uint32_t)prms[2];

		kprintf("RMS(I,V,P)=("
				"0x%02X%02X%02X,"
				"0x%02X%02X%02X,"
				"0x%02X%02X%02X) => "
				"(%8ld,%8ld,%8ld)\n",
				irms[0],irms[1],irms[2],
				vrms[0],vrms[1],vrms[2],
				power[0],power[1],power[2],
				irms_value, vrms_value, power_value
				);

		LED_SWITCH();
		timer_delay(1000);
	}
#endif

	return 0;
}

