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
 * Copyright 2005 Develer S.r.l. (http://www.develer.com/)
 * -->
 *
 *
 * \brief ADE7753 Single-Phase Multifunction Metering IC with di/dt Sensor
 * Interface Driver
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 *
 * $WIZ$ module_name = "meter_ade7753"
 * $WIZ$ module_depends = "timer", "ser"
 * $WIZ$ module_configuration = "bertos/cfg/cfg_ade7753.h"
 * $WIZ$ module_hw = "bertos/hw/hw_ade7753.h"
 */

#include "meter_ade7753.h"

#include "hw/hw_ade7753.h"
#include "cfg/cfg_ade7753.h"

#include <drv/timer.h>
#include <io/kfile.h>

#include <cfg/debug.h>
#include <cfg/module.h>

// Define logging setting (for cfg/log.h module).
#define LOG_LEVEL   ADE7753_LOG_LEVEL
#define LOG_FORMAT  ADE7753_LOG_FORMAT
#include <cfg/log.h>

static struct KFile *spi;

static void meter_read(unsigned char addr, unsigned char * data, unsigned char count)
{
	LOG_INFO("%s: @%#02X\n", __func__, addr);

	ADE7753_CS_LOW();
	timer_delay(10);

	kfile_write(spi, &addr, 1);
	timer_delay(10);

	for (int i=0; i<count; i++) { 
		kfile_read(spi, data+i, 1);
		timer_delay(10);
	}

	ADE7753_CS_HIGH();
} 

static void meter_write(unsigned char addr, unsigned char * data, unsigned char count)
{

	LOG_INFO("%s: @%#02X\n", __func__, addr);

	ADE7753_CS_LOW();
	timer_delay(10);
	
	addr |= 0x80;
	kfile_write(spi, &addr, 1);
	timer_delay(10);

	for (int i=0; i<count; i++) { 
		kfile_write(spi, data+i, 1);
		timer_delay(10);
	}

	ADE7753_CS_HIGH();

}

static void meter_set(uint16_t bits)
{
	uint16_t conf;

	meter_read(ADE7753_MODE, (unsigned char*)&conf, 2);
	conf |= bits;
	meter_write(ADE7753_MODE, (unsigned char*)&conf, 2);
}

static void meter_clear(uint16_t bits)
{
	uint16_t conf;

	meter_read(ADE7753_MODE, (unsigned char*)&conf, 2);
	conf &= ~bits;
	meter_write(ADE7753_MODE, (unsigned char*)&conf, 2);
}
/**
 * Turn off 
 */
void meter_ade7753_off(void)
{
	meter_set(ADE7753_ASUSPEND);
}

/**
 * Turn on
 */
void meter_ade7753_on(void)
{
	meter_clear(ADE7753_ASUSPEND);
}

void meter_ade7753_conf(meter_conf_t *conf) {
        meter_read(ADE7753_DIEREV, &(conf->rev), 1);
        timer_delay(5);
        meter_read(ADE7753_MODE, conf->mode, 2);
        timer_delay(5);
        meter_read(ADE7753_IRQEN, conf->irqs, 2);
        timer_delay(5);
}

//void meter_ade7753_Irms(unsigned char *sample) {
//        meter_read(ADE7753_IRMS, sample, 3);
//}

void meter_ade7753_Vrms(unsigned char *sample) {
        meter_read(ADE7753_VRMS, sample, 3);
}

void meter_ade7753_Power(unsigned char *sample) {
        meter_read(ADE7753_WAVEFORM, sample, 3);
}

uint32_t meter_ade7753_Irms() {
	unsigned char irms[3];
	uint32_t irms_value;

    meter_read(0x16, irms, 3);
	irms_value  = ((uint32_t)irms[0]<<16)
			+((uint32_t)irms[1]<<8)
			+(uint32_t)irms[2];

	kprintf("Irms=0x%02X%02X%02X=%08ld\n",
			irms[0], irms[1], irms[2], irms_value);

	return irms_value;
}




/**
 * Reset 
 */
//void meter_ade7753_reset(void)
//{
//	LOG_INFO("Reset...\n");
//	meter_set(ADE7753_SWRST);
//	timer_udelay(500);
//}
//
void meter_ade7753_reset(void)
{
	unsigned char conf[2] = {
		0x00,0x00};

	// Software reset
	conf[0] = 0x00;
	conf[1] = 0x40;
	meter_write(0x09, conf, 2);
	timer_delay(100);

	// Load conf
	//conf[0] = 0x00;
	conf[0] = 0x00; // CH2 on waveform register
	conf[1] = 0x0C;
	meter_write(0x09, conf, 2);
	//      conf[1] = (_BV(SWRST) | _BV(DISCF) | _BV(DISSAG));
	//      meter_write(MODE, conf, 2);
	timer_delay(100);

	// Enable all interrupts
	conf[0] = 0xFF;
	conf[1] = 0xFF;
	meter_write(0x0A, conf, 2);

}

MOD_DEFINE(meter_ade7753)

/**
 * Initialize ADE7753
 */
void meter_ade7753_init(struct KFile *_spi)
{
	#if CONFIG_KERN_IRQ
		MOD_CHECK(irq);
	#endif

	ASSERT(_spi);
	spi = _spi;
	meter_ade7753_hw_bus_init();
	meter_ade7753_reset();
	
	MOD_INIT(meter_ade7753);
}

