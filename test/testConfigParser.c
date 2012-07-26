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
bool testTrimWhitespace(void);


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

int main(int argc, char **argv) {
  int retval = 0;

  printf("testTrimWhitespace: ");
  if(!testTrimWhitespace()) {
    printf("FAILED\n");
    retval = 1;
  } else {
    printf("PASSED\n");
  }
  
  printf("testCommentLine: ");
  if(!testCommentLine()) {
    printf("FAILED\n");
    retval = 1;
  } else {
    printf("PASSED\n");
  }
  
  printf("testConfigLine: ");
  if(!testConfigLine()) {
    printf("FAILED\n");
    retval = 1;
  } else {
    printf("PASSED\n");
  }
  
  return retval;
}
