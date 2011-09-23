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
 * Copyright 2008 Develer S.r.l. (http://www.develer.com/)
 * All Rights Reserved.
 * -->
 *
 * \brief Configuration file for GSM module.
 *
 * \author Patrick Bellasi <derkling@gmail.com>
 */

#ifndef CFG_CONTROL_H
#define CFG_CONTROL_H

/**
 * Check this to enable GSM low-level debugging
 *
 * $WIZ$ type = "boolean"
 */
#define CONFIG_CONTROL_DEBUG   0

/**
 * Check this to enable GSM testing port
 *
 * $WIZ$ type = "boolean"
 */
#define CONFIG_CONTROL_TESTING 	0

/**
 * Check this to enable FAULT levels reporting
 *
 * $WIZ$ type = "boolean"
 */
#define CONFIG_REPORT_FAULT_LEVELS   1

/**
 * Number of consecutive and "stable" calibration samples
 *
 * $WIZ$ type = "int"
 */
#define CONFIG_CALIBRATION_SAMPLES 32

/**
 * Number of consecutive faulty reading to trigger a fault
 *
 * $WIZ$ type = "int"
 * $WIZ$ min = "16"
 * $WIZ$ max = "255"
 */
#define CONFIG_FAULT_SAMPLES 64

/**
 * Number of faults detections before an alarm is triggered
 *
 * $WIZ$ type = "int"
 * $WIZ$ min = "1"
 * $WIZ$ max = "255"
 */
#define CONFIG_FAULT_CHECKS 3

/**
 * Number of seconds between successive faults checks
 *
 * $WIZ$ type = "int"
 * $WIZ$ min = "1"
 * $WIZ$ max = "65535"
 */
#define CONFIG_FAULT_CHECK_TIME 180

/**
 * Default faulty level (K)
 *
 * $WIZ$ type = "int"
 * $WIZ$ min = "1"
 * $WIZ$ max = "65535"
 */
#define CONFIG_FAULT_LEVEL 160

/**
 * Number of weeks among forced re-calibration
 *
 * $WIZ$ type = "int"
 * $WIZ$ min = "1"
 * $WIZ$ max = "65535"
 */
#define CONFIG_CALIBRATION_WEEKS 16


/**
 * Set to use P[W] insetad of I[A]
 *
 * $WIZ$ type = "boolean"
 */
#define CONFIG_MONITOR_POWER 1

/**
 * Module logging level.
 *
 * $WIZ$ type = "enum"
 * $WIZ$ value_list = "log_level"
 */
#define CONTROL_LOG_LEVEL      LOG_LVL_INFO

/**
 * Module logging format.
 *
 * $WIZ$ type = "enum"
 * $WIZ$ value_list = "log_format"
 */
#define CONTROL_LOG_FORMAT     LOG_FMT_TERSE

#endif /* CFG_CONTROL_H */


