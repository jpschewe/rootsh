/*

rootsh - a logging shell wrapper for root wannabes

basename.c is an implementation of the basename algorithm, which 
returns the filename component of a null-terminated pathname string.
I made it for platforms which lack their own basename function.

Copyright (C) 2004 Gerhard Lausser

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdio.h>
#include <string.h>

char *basename(char *path) {
  char *newpath, *ptr = path;
  int slashfound = 0;

  /*
  //  If path is a NULL pointer or points to an empty string, 
  //  then basename returns the string ".".
  */
  if ((path == NULL) || (path[0] == '\0')) {
    return(".");
  }
  /*
  //  If path is the string "/", basename returns the string "/".
  */
  if ((path[0] == '/') && (path[1] == '\0')) {
    return("/");
  }
  /*
  //  This implementation of basename does not modify the original path.
  //  We will allocate memory for a copy.
  */
  ptr = newpath = strdup(path);
  /*
  //  Now position ptr at the last element of newpath. Look for
  //  slashes on the fly.
  */
  while(*(ptr + 1) != '\0') {
    if (*ptr == '/') {
      slashfound = 1;
    }
    ptr++;
  }
  /*
  //  If path does not contain a slash, basename returns a copy of path.
  */
  if (slashfound == 0) {
    return(newpath);
  }
  /*
  //  Trailing '/' characters are not counted as part of the pathname.
  */
  while ((*ptr == '/') && (ptr > newpath)) {
    *(ptr--) = '\0';
  }
  /*
  //  If path consists only of slashes, return "/".
  */
  if ((ptr == newpath) && (*ptr == '/')) {
    return(ptr);
  }
  /*
  //  Now go backwards and look if there's a slash left. Return everything
  //  that comes after the slash.
  */
  while (ptr > newpath) {
    if (*ptr == '/') {
      return(ptr + 1);
    }
    ptr--;
  }
  /*
  //  We should never come here. Return something anyway.
  */
  return(ptr);
}
