/**
 *       @file  control.h
 *      @brief  The FRN control loop
 *
 * This module provides the definition of the main control loop for the Remote
 * Fault Notifier device.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  03/31/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * ============================================================================
 */

#ifndef DRK_CONTROL_H_
#define DRK_CONTROL_H_

#include "console.h"

#include <io/kfile.h>

void controlSetup(void);
void controlLoop(void);

#endif // DRK_CONTROL_H_

