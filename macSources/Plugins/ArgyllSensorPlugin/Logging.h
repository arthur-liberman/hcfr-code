/*
 *  Logging.h
 *  ColorHCFR
 *
 *  Created by Nick on 3/03/12.
 *  Copyright 2012 Homecinema-fr.com. All rights reserved.
 *
 */

#include <stdarg.h>
// no logging in the unit tests for now
void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args);