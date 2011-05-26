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
 * Copyright 2006 Develer S.r.l. (http://www.develer.com/)
 * -->
 *
 *
 * \brief ADE7753 Single-Phase Multifunction Metering IC with di/dt Sensor
 * Interface Driver
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 */

#ifndef DRV_ADE7753_H
#define DRV_ADE7753_H

#include <cfg/compiler.h>

#define ADE7753_WAVEFORM   0x01
#define ADE7753_AENERGY    0x02
#define ADE7753_RAENERGY   0x03
#define ADE7753_LAENERGY   0x04
#define ADE7753_VAENERGY   0x05
#define ADE7753_RVAENERGY  0x06
#define ADE7753_LVAENERGY  0x07
#define ADE7753_LVARENERGY 0x08
#define ADE7753_MODE       0x09
#define ADE7753_IRQEN      0x0A
#define ADE7753_STATUS     0x0B
#define ADE7753_RSTSTATUS  0x0C
#define ADE7753_CH1OS      0x0D
#define ADE7753_CH2OS      0x0E
#define ADE7753_GAIN       0x0F
#define ADE7753_PHCAL      0x10
#define ADE7753_APOS       0x11
#define ADE7753_WGAIN      0x12
#define ADE7753_WDIV       0x13
#define ADE7753_CFNUM      0x14
#define ADE7753_CFDEN      0x15
#define ADE7753_IRMS       0x16
#define ADE7753_VRMS       0x17
#define ADE7753_IRMSOS     0x18
#define ADE7753_VRMSOS     0x19
#define ADE7753_VAGAIN     0x1A
#define ADE7753_VADIV      0x1B
#define ADE7753_LINECYC    0x1C
#define ADE7753_ZXTOUT     0x1D
#define ADE7753_SAGCYC     0x1E
#define ADE7753_SAGLVL     0x1F
#define ADE7753_IPKLVL     0x20
#define ADE7753_VPKLVL     0x21
#define ADE7753_IPEAK      0x22
#define ADE7753_RSTIPEAK   0x23
#define ADE7753_VPEAK      0x24
#define ADE7753_RSTVPEAK   0x25
#define ADE7753_TEMP       0x26
#define ADE7753_PERIOD     0x27
#define ADE7753_TMODE      0x3D
#define ADE7753_CHKSUM     0x3E
#define ADE7753_DIEREV     0x3F

#define ADE7753_DISHPF     0x0
#define ADE7753_DISLPF2    0x1
#define ADE7753_DISCF      0x2
#define ADE7753_DISSAG     0x3
#define ADE7753_ASUSPEND   0x4
#define ADE7753_TEMPSEL    0x5
#define ADE7753_SWRST      0x6
#define ADE7753_CYCMODE    0x7
#define ADE7753_DISCH1     0x8
#define ADE7753_DISCH2     0x9
#define ADE7753_SWAP       0xA
#define ADE7753_DTRT0      0xB
#define ADE7753_DTRT1      0xC
#define ADE7753_WAVSEL0    0xD
#define ADE7753_WAVSEL1    0xE
#define ADE7753_POAM       0xF

#define ADE7753_MAX_TX    4
#define ADE7753_MAX_RX    4
#define ADE7753_STARTUP_DELAY 1


//typedef uint32_t ade7753_data_t;

struct KFile;

void meter_ade7753_init(struct KFile *spi);

void meter_ade7753_on(void);
void meter_ade7753_off(void);
void meter_ade7753_reset(void);

typedef struct meter_conf {
        unsigned char rev;
        unsigned char mode[2];
        unsigned char irqs[2];
} meter_conf_t;

void meter_ade7753_conf(meter_conf_t *conf);
void meter_ade7753_dumpConf(void);

uint32_t meter_ade7753_Irms(void);
uint32_t meter_ade7753_Vrms(void);
void meter_ade7753_Power(unsigned char *sample);

void meter_ade7753_setLCEA(uint8_t cycles);
int32_t meter_ade7753_getEnergyLCAE(void);

/*
uint8_t meter_ade7753_read_reg8u(uint8_t reg, uint8_t *data);
uint8_t meter_ade7753_read_reg8s(uint8_t reg, int8_t *data);
uint8_t meter_ade7753_read_reg16u(uint8_t reg, uint8_t *data);
uint8_t meter_ade7753_read_reg16s(uint8_t reg, int8_t *data);
uint8_t meter_ade7753_read_reg24u(uint8_t reg, uint8_t *data);
uint8_t meter_ade7753_read_reg24s(uint8_t reg, int8_t *data);

uint8_t meter_ade7753_read_reg8u(uint8_t reg, uint8_t *data);
uint8_t meter_ade7753_read_reg8s(uint8_t reg, int8_t *data);
uint8_t meter_ade7753_read_reg16u(uint8_t reg, uint8_t *data);
uint8_t meter_ade7753_read_reg16u(uint8_t reg, uint8_t *data);
*/

#endif  /* DRV_ADE7753_H */
