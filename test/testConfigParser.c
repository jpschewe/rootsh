/*
  Test for simple config parser.

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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "configParser.h"

/* function declarations */
bool testCommentLine(void);
bool testConfigLine(void);
bool testNotConfigLine(void);
bool testTrimWhitespace(void);
bool testAllWhitespace(void);
bool testSplitLine(void);

/* implementations */
bool testTrimWhitespace(void) {
  char const *line = "   Something in between  \n";
  char const *expected = "Something in between";
  char *actual = trimWhitespace(line);
  bool retval;

  if(NULL == actual) {
    return false;
  }

  if(0 == strcmp(expected, actual)) {
    retval = true;
  } else {
    printf("Bad trimmed value: '%s'\n", actual);
    retval = false;
  }
  
  free(actual);
  return retval;
}

bool testAllWhitespace(void) {
  char const *line = "   \t \v \f \r  \n";
  char const *expected = "";
  char *actual = trimWhitespace(line);
  bool retval;

  if(NULL == actual) {
    return false;
  }

  if(0 == strcmp(expected, actual)) {
    retval = true;
  } else {
    retval = false;
  }
  
  free(actual);
  return retval;
}

bool testCommentLine(void) {
  char const * line = "# this is a comment\n";
  
  if(!isConfigLine(line)) {
    return true;
  } else {
    return false;
  }
}

bool testConfigLine(void) {
  char const * line = "var = 10\n";
  
  if(isConfigLine(line)) {
    return true;
  } else {
    return false;
  }
}

bool testNotConfigLine(void) {
  char const * line = "var\n";
  
  if(!isConfigLine(line)) {
    return true;
  } else {
    return false;
  }
}

bool testSplitLine(void) {
  char const *line = "var = 10\n";
  char const *expectedKey = "var";
  char const *expectedValue = "10";
  char key[256];
  char value[256];
    
  if(!splitConfigLine(line, sizeof(key), key, sizeof(value), value)) {
    printf("splitConfigLine returned false\n");
    return false;
  }

  if(0 != strcmp(expectedKey, key)) {
    printf("Bad key. Expected: '%s' Actual: '%s'\n", expectedKey, key);
    return false;
  }

  if(0 != strcmp(expectedValue, value)) {
    printf("Bad value. Expected: '%s' Actual: '%s'\n", expectedValue, value);
    return false;
  }

  return true;
}

int main(int argc, char **argv) {
  int retval = 0;

  printf("testAllWhitespace:\n");
  if(!testAllWhitespace()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }
  
  printf("testTrimWhitespace:\n");
  if(!testTrimWhitespace()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }

  printf("testCommentLine:\n");
  if(!testCommentLine()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }
  
  printf("testConfigLine:\n");
  if(!testConfigLine()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }

  printf("testNotConfigLine:\n");
  if(!testNotConfigLine()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }

  printf("testSplitLine:\n");
  if(!testSplitLine()) {
    printf("\tFAILED\n");
    retval = 1;
  } else {
    printf("\tPASSED\n");
  }
  
  return retval;
}
