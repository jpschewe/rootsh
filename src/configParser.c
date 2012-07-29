/*
  Simple configuration file parser.

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


#include "configParser.h"
#include <strings.h>
#include <stdlib.h>

static bool isWhitespace(char const data) {
  return data == ' '
    || data == '\n'
    || data == '\r'
    || data == '\t'
    || data == '\f'
    || data == '\v'
    || data == '\0';
}


char * trimWhitespace(char const * const data) {
  size_t const len = strlen(data);
  size_t newLen = 0;
  char const *begin = NULL;
  char const *end = NULL;
  char *retval = NULL;
  
  if(len == 0) {
    return NULL;
  }
  
  retval = malloc(len+1);
  if(NULL == retval) {
    return NULL;
  }

  memset(retval, len, 0);
  
  begin = &(data[0]);
  end = &(data[len-1]);
  
  while(isWhitespace(*begin) && begin != end) {
    ++begin;
  }

  if(begin == end) {
    return retval;
  }
  
  while(isWhitespace(*end)) {
    --end;
  }

  /* Add 1 so we get [being, end] inclusive */
  newLen = end - begin + 1;
  
  memcpy(retval, begin, newLen);
  retval[newLen] = '\0';
  
  return retval;
}

bool isConfigLine(char const * const line) {
  size_t const lineLength = strlen(line);
  if(lineLength == 0) {
    return false;
  } else if(line[0] == '#') {
    return false;
  } else {
    /* just check for equals sign */
    char const *equals = index(line, '=');
    return equals != NULL;
  }
}

bool splitConfigLine(char const * const line,
                     size_t const keyLength,
                     char * const key,
                     size_t const valueLength,
                     char * const value) {
  char *equals;
  size_t actualKeyLength;
  size_t actualValueLength;
  char *tempKey;
  char *tempValue;
  char *trimmedKey;
  char *trimmedValue;
  
  if(!isConfigLine(line)) {
    return false;
  }

  equals = index(line, '=');
  if(NULL == equals) {
    return false;
  }

  actualKeyLength = equals - line;
  if(actualKeyLength >= keyLength) {
    /* too big */
    return false;
  }

  actualValueLength = strlen(line) - actualKeyLength - 1;
  if(actualValueLength >= valueLength) {
    return false;
  }

  
  /* create the key */
  tempKey = malloc(actualKeyLength+1);
  if(NULL == tempKey) {
    return false;
  }
  strncpy(tempKey, line, actualKeyLength);
  tempKey[actualKeyLength] = '\0';

  trimmedKey = trimWhitespace(tempKey);
  
  strcpy(key, trimmedKey);
  
  free(tempKey);
  free(trimmedKey);
  

  tempValue = malloc(actualValueLength+1);
  if(NULL == tempValue) {
    return false;
  }
  strncpy(tempValue, equals+1, actualValueLength);
  tempValue[actualValueLength] = '\0';
  
  trimmedValue = trimWhitespace(tempValue);
  
  strcpy(value, trimmedValue);
  
  free(tempValue);
  free(trimmedValue);
  
  return true;
}

bool parseBool(char const * const data) {
  if(NULL == data) {
    return false;
  } else if(0 == strcasecmp("true", data)) {
    return true;
  } else {
    return false;
  }
}
