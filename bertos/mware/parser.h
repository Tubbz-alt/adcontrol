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
 * Copyright 2003, 2006 Develer S.r.l. (http://www.develer.com/)
 * All Rights Reserved.
 * -->
 *
 * \author Bernie Innocenti <bernie@codewiz.org>
 * \author Stefano Fedrigo <aleph@develer.com>
 * \author Giovanni Bajo <rasky@develer.com>
 *
 * \brief Channel protocol parser and commands.
 *
 * $WIZ$ module_name = "parser"
 * $WIZ$ module_configuration = "bertos/cfg/cfg_parser.h"
 * $WIZ$ module_depends = "kfile", "hashtable"
 */


#ifndef MWARE_PARSER_H
#define MWARE_PARSER_H

#include <cpu/types.h>

/** Max number of arguments and results for each command */
#define PARSER_MAX_ARGS       8

/**
 * Error generated by the commands through the return code.
 */
typedef enum
{
	RC_ERROR  = -1, ///< Reply with error.
	RC_OK     = 0,  ///< No reply (ignore reply arguments).
	RC_REPLY  = 1,  ///< Reply command arguments.
	RC_SKIP   = 2   ///< Skip following commands
} ResultCode;

/** union that contains parameters passed to and from commands */
typedef union { long l; const char *s; } parms;
/** pointer to commands */
typedef ResultCode (*CmdFuncPtr)(parms args_results[]);

/**
 * Define a command that can be tokenized by the parser.
 *
 * The format strings are sequences of characters, one for each
 * parameter/result. Valid characters are:
 *
 *  d - a long integer, in decimal format
 *  s - a var string (in RAM)
 *
 * \note To create and fill an instance for this function, it is strongly
 * advised to use \c DECLARE_CMD_HUNK (cmd_hunk.h).
 */
struct CmdTemplate
{
	const char *name;          ///< Name of command
	const char *arg_fmt;       ///< Format string for the input
	const char *result_fmt;    ///< Format string for the output
	CmdFuncPtr func;           ///< Pointer to the handler function
	uint16_t   flags;          ///< Currently unused.
};

/**
 * Initialize the parser module
 *
 * \note This function must be called before any other function in this module
 */
void parser_init(void);


/**
 * Register a new command into the parser
 *
 * \param cmd Command template describing the command
 *
 */
void parser_register_cmd(const struct CmdTemplate* cmd);


/**
 * Hook for readline to provide completion support for the commands
 * registered in the parser.
 *
 * \note This is meant to be used with mware/readline.c. See the
 * documentation there for a description of this hook.
 */
const char* parser_rl_match(void* dummy, const char* word, int word_len);


/**
 * \brief Command input handler.
 *
 * Process the input, calling the requested command
 * (if found) and calling printResult() to give out
 * the result (on device specified with parameter fd).
 *
 * \param line Text line to be processed (ASCIIZ)
 *
 * \return true if everything is OK, false in case of errors
 */
bool parser_process_line(const char* line);


/**
 * Execute a command with its arguments, and fetch its results.
 *
 * \param templ Template of the command to be executed
 * \param args Arguments for the command, and will contain the results
 *
 * \return False if the command returned an error, true otherwise
 */
INLINE bool parser_execute_cmd(const struct CmdTemplate* templ, parms args[PARSER_MAX_ARGS])
{
	return (templ->func(args) == 0);
}


/**
 * Find the template for the command contained in the text line.
 * The template can be used to tokenize the command and interpret
 * it.
 *
 * This function can be used to find out which command is contained
 * in a given text line without parsing all the parameters and
 * executing it.
 *
 * \param line Text line to be processed (ASCIIZ)
 *
 * \return The command template associated with the command contained
 * in the line, or NULL if the command is invalid.
 */
const struct CmdTemplate* parser_get_cmd_template(const char* line);


/**
 * Extract the arguments for the command contained in the text line.
 *
 * \param line Text line to be processed (ASCIIZ)
 * \param templ Command template for this line
 * \param args Will contain the extracted parameters
 *
 * \return True if everything OK, false in case of parsing error.
 */
bool parser_get_cmd_arguments(const char* line, const struct CmdTemplate* templ, parms args[PARSER_MAX_ARGS]);


/**
 * Extract the ID from the command text line.
 *
 * \param line Text line to be processed (ASCIIZ)
 * \param ID Will contain the ID extracted.
 *
 * \return True if everything ok, false if there is no ID
 *
 */
bool parser_get_cmd_id(const char* line, unsigned long* ID);

#endif /* MWARE_PARSER_H */

