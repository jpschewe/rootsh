/*
  Header for simple configuration file parser.

  Copyright (C) 2012 Jon Schewe

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 3
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include <string.h>
#include <stdbool.h>

/**
 * @param line the line to check, must be NULL terminated
 * @return true if this line is not a comment and is not blank
 */
bool isConfigLine(char const * const line);

/**
 * Split the line into the key and value. The key and
 * value are trimmed. The value is anything after the
 * equals sign.
 *
 * @param line the line to split, must be NULL terminated
 * @param keyLength the length of the key buffer
 * @param key output parameter containing the key,
 * will be null terminated
 * @param valueLength the length of the value buffer
 * @param value output parameter containing the value,
 * will be null terminated
 * 
 * @return false if there was an error splitting the line,
 * such as the equals sign not being found or one of
 * the return values not being large enough.
 */
bool splitConfigLine(char const * const line,
                     size_t const keyLength,
                     char * const key,
                     size_t const valueLength,
                     char * const value);

/**
 * Trim whitespace from data.
 *
 * @param data the string to trim, must be NULL terminated
 * @return newly allocated string with whitespace removed.
 * Will be NULL in the case of an error, such as a failure
 * to allocate memory. An empty string will be returned if
 * all characters are whitespace.
 */
char * trimWhitespace(char const * const data);

/**
 * Trim whitespace from data.
 *
 * @param data the string to trim, must be NULL terminated
 * @return newly allocated string with whitespace removed.
 * Will be NULL in the case of an error, such as a failure
 * to allocate memory. An empty string will be returned if
 * all characters are whitespace.
 */
char * trimWhitespace(char const * const data);

/**
 * Parse data as a boolean. Any version of "true"
 * is true, all else is false.
 *
 * @param data the string to compare, must be null terminated
 */
bool parseBool(char const * const data);
