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

static void meter_read(unsigned char addr, unsigned char * data,
		unsigned char count)
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

static void meter_write(unsigned char addr, unsigned char * data,
		unsigned char count)
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

//void meter_ade7753_Vrms(unsigned char *sample) {
//        meter_read(ADE7753_VRMS, sample, 3);
//}

void meter_ade7753_Power(unsigned char *sample) {
        meter_read(ADE7753_WAVEFORM, sample, 3);
}

uint32_t meter_ade7753_Irms(void) {
	unsigned char irms[3];
	uint32_t irms_value;

    meter_read(0x16, irms, 3);
	irms_value  = ((uint32_t)irms[0]<<16)
			+((uint32_t)irms[1]<<8)
			+(uint32_t)irms[2];

	LOG_INFO("Irms=0x%02X%02X%02X=%08ld\n",
			irms[0], irms[1], irms[2], irms_value);

	return irms_value;
}

uint32_t meter_ade7753_Vrms(void) {
	unsigned char vrms[3];
	uint32_t vrms_value;

    meter_read(0x17, vrms, 3);
	vrms_value  = ((uint32_t)vrms[0]<<16)
			+((uint32_t)vrms[1]<<8)
			+(uint32_t)vrms[2];

	LOG_INFO("Vrms=0x%02X%02X%02X=%08ld\n",
			vrms[0], vrms[1], vrms[2], vrms_value);

	return vrms_value;
}
/**
 * @brief Enable Line Cycle Energy Accumulation mode
 *
 * @param the number of line cycles to use
 */
void meter_ade7753_setLCEA(uint8_t cycles) {
	unsigned char linecyc[2];
	uint16_t hc = cycles;

	// Compute the number of half line cycles
	hc <<= 1;
	
	// Configure the LINECYC register with the number of half line cycles
	linecyc[0] = (unsigned char)(hc>>8);
	linecyc[1] = (unsigned char)(hc & 0xFF);
	meter_write(0x1C, linecyc, 2);

	// Read back value for write check
	DB(meter_read(0x1C, linecyc, 2));
	LOG_INFO("Set LCAE [0x%02X%02X: %hd*2 half-cycles]\n",
			linecyc[0], linecyc[1], cycles);

}

static int32_t meter_ade7753_LCAE(void) {
	unsigned char lcae[3];
	int32_t lcae_value;

	// Read LAENERGY (register 0x04)
    meter_read(0x04, lcae, 3);
	lcae_value  = ((int32_t)lcae[0]<<16)
			+((int32_t)lcae[1]<<8)
			+(int32_t)lcae[2];

	// Adjust sign (which is 24bit mod-2 signed)
	if (lcae_value & 0x008000) {
		lcae_value &= ~0x008000;
		lcae_value |=  0x800000;
	}

	LOG_INFO("LCAE=0x%02X%02X%02X=%08ld\n",
			lcae[0], lcae[1], lcae[2], lcae_value);

	return lcae_value;
}

/**
 * @brief Get the Energy accumulated in the specifued line cycles.
 */
int32_t meter_ade7753_getEnergyLCAE(void) {
	unsigned char conf[2] = {
		0x00,0x00};

	// Resetting Interrupt status register
	conf[0] = 0x00;
	conf[1] = 0x00;
	meter_write(0x0C, conf, 2);

	// Read current configuration
	meter_read(0x09, conf, 2);

	// Setting Mode register bit 7 (CYCMODE)
	conf[0] |= 0x01; 
	meter_write(0x09, conf, 2);

	// Dump ADE7753 configuration
	DB(meter_ade7753_dumpConf());

	// wait for a measurement to complete
	// by polling the Interrupt register bit 3 (CYCEND)
	for (uint8_t c = 0; ; c++) {//c<64; c++) { 
		// Wait for next line cycle (@50Hz = 20ms ==> wait 1ms more)
		timer_delay(21);
		// Read the interrupt status register (0x0B)
		meter_read(0x0B, conf, 2);
		LOG_INFO("IRQs %#04X\r\n",
				((uint16_t)conf[0]<<7|conf[1]));
		if (conf[1] & 0x04)
			break;
	}

	// read the Accumulated Energy
	return meter_ade7753_LCAE();
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

	// Software Chip Reset. A data transfer should not take place to the
	// ADE7753 for at least 18 μs after a software reset.
	conf[0] = 0x00;
	conf[1] = 0x40;
	meter_write(0x09, conf, 2);
	timer_delay(1);

	// Load conf
	//conf[0] = 0x00;
	conf[0] = 0x00; // CH2 on waveform register
	conf[1] = 0x0C;
	meter_write(0x09, conf, 2);
	//      conf[1] = (_BV(SWRST) | _BV(DISCF) | _BV(DISSAG));
	//      meter_write(MODE, conf, 2);
	timer_delay(1);

	// Enable all interrupts
	conf[0] = 0xFF;
	conf[1] = 0xFF;
	meter_write(0x0A, conf, 2);

}

/**
 * Dump ADE7753 configuration
 */
void meter_ade7753_dumpConf(void) {
	meter_conf_t ade7753_conf;

	LOG_INFO(".:: ADE7753 Conf\r\n");

	meter_ade7753_conf(&ade7753_conf);
	LOG_INFO("Rev: %#02X, Mode %#04X, IRQs %#04X\r\n",
			ade7753_conf.rev,
			(((uint16_t)ade7753_conf.mode[0]<<7)|ade7753_conf.mode[1]),
			(((uint16_t)ade7753_conf.irqs[0]<<7)|ade7753_conf.irqs[1]));

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

