/*

rootsh - a logging shell wrapper for root wannabes

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
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include "config.h"

extern void write2syslog(const char *oBuffer, size_t oCharCount);
char *stripesc(char *escBuffer);

#define OBUFSIZ 1024

#define BEL 0x07
#define BS 0x08
#define TAB 0x09
#define LF 0x0A
#define VT 0x0B
#define FF 0x0C
#define CR 0x0D
#define SO 0x0E
#define SI 0x0F
#define CAN 0x18
#define SUB 0x1A
#define ESC 0x1B
#define DEL 0x7F

#define SS2 0x8E
#define SS3 0x8F
#define DCS 0x90
#define CSI 0x9B
#define ST 0x9C
#define OSC 0x9D
#define PM 0x9E
#define APC 0x9F


enum {
  Normal, Esc, Csi, Dcs, DcsString, Osc, OscString, DropOne, DecSet
} vtstate = Normal;



/* 
//  remove escape sequences from a string
//  algorithm mostly copied from 
//  news:<3g5hjg$re2@newhub.xylogics.com> by james carlson
*/

char *stripesc(char *escBuffer) {
  int buflen = strlen(escBuffer);
  char *escBufferPtr = escBuffer;
  char *cleanBuffer = (char *)malloc(buflen + 1);
  char *cleanBufferPtr = cleanBuffer;
  int chr;

  while ((chr = *escBufferPtr++) && (buflen-- > 0)) {
    chr &= 0xFF;
    if (vtstate == DropOne) {
      vtstate = Normal;
      continue;
    }
    /* 
    //  Handle normal ANSI escape mechanism
    //  (Note that this terminates DCS strings!) 
    */
    if (vtstate == Esc && chr >= 0x40 && chr <= 0x5F) {
     vtstate = Normal;
     chr += 0x40;
    }
    switch (chr) {
      case CAN: case SUB:
        vtstate = Normal;
        break;
      case ESC:
        vtstate = Esc;
        break;
      case CSI:
        vtstate = Csi;
        break;
      case DCS: case PM: case APC:
        vtstate = Dcs;  
        break;
      case OSC:
        vtstate = Osc;  
        break;
      default:
        if ((chr & 0x6F) < 0x20) { /* Check controls */
          switch (chr) {
            case BEL:
	      if (vtstate == OscString)
		vtstate = Normal;
	      else
		*cleanBufferPtr++ = chr;
              break;
            case BS: case TAB: case LF: case VT: case FF: case CR:
              *cleanBufferPtr++ = chr;
              break;
          }
          break;
        }
        switch (vtstate) {
          case Normal:
            *cleanBufferPtr++ = chr;
            break;
          case Esc:
            vtstate = Normal;
            switch (chr) {
              case 'c': case '7': case '8': case '=': case '>': case '~':
              case 'n': case '\123': case 'o':
              case '|':
                break;
              case '#': case ' ': case '(': case ')': case '*': case '+':
                vtstate = DropOne;
                break;
            }
            break;
          case Csi: case Dcs:
            if (chr >= 0x40 && chr <= 0x7E) {
              if (vtstate == Csi) {
                vtstate = Normal;
              } else {
                vtstate = DcsString;
              }
            }
            break;
	  case Osc:
            if (chr >= 0x40 && chr <= 0x7E)
	      vtstate = OscString;
	    break;
          case DecSet:
            if (chr == 0x68) {
              vtstate = Normal;
            }
            break;
          case DcsString:
            break;
	  case OscString:
	    break;
          case DropOne:
            break;
        }
    }
  }
  *cleanBufferPtr = '\0';
  strcpy(escBuffer, cleanBuffer);
  free((void *)cleanBuffer);
  return(escBuffer);
}



/*
//  takes a variable sized string
//  breaks the string into pieces (lines) separated by \r
//  cleans the lines from escape sequences and writes it to syslog
//  keeps remainings (not closed by a newline) in a static area
*/

void write2syslog(const char *optr, size_t optrLength) {
  static size_t rptrLength = 0;
  /* 
  //  buffer where remaining input will be kept 
  */
  static char *rptr;              
  /* 
  //  pointer to the string which will be output 
  */
  char *eptr;                     
  /* 
  //  pointer from where the search for '\r' begins 
  */
  char *tptr;                     
  /* 
  //  pointer to occurrences of '\r' 
  */
  char *lptr;                     
  /* 
  //  signals the last output character was a '\r' 
  */
  static int rflag = 0;           
#ifdef LINECNT
  /*
  //  a 3-digit counter which prepends each line sent to the syslog server 
  //  this allows the detection of dropped lines 
  */
  static int linecnt = 0;
#endif
  
  /* 
  //  ignore empty input 
  */
  if ((optrLength == 0) || (optr == NULL)) {    
    return;
  }
  /*
  //  skip newlines 
  */
  if ((rflag == 1) && (*optr == '\n')) {     
    optr++;
    optrLength--;
  }
  rflag = 0;
  /*
  //  no remaining output from last run 
  */
  if (rptr == NULL) {                        
    /*
    //  allocate new buffer for current output 
    */
    rptr = malloc(optrLength);                  
  } else {
    /*
    //  add space for current output to remaining output 
    */
    rptr = realloc(rptr, rptrLength + optrLength);     
  }
  /*
  //  fill the newly created memory with the current output 
  */
  memcpy(rptr + rptrLength, optr, optrLength);  
  /*
  //  new length is now 
  //  old buffer length (might have been 0) + current output's length 
  */
  rptrLength += optrLength;                     
  /*
  //  tptr points to where be begin looking for crs 
  */
  tptr = rptr;                               
  lptr = memchr(tptr, '\r', rptrLength);
  /*
  //  we found a \r at position lptr and we started searching for it
  //  at an allowed position 
  */
  while ((lptr != NULL) && (tptr < (rptr + rptrLength))) { 
    /*
    //  skip a following \n if rflag is set 
    */
    rflag = 1;                             
    /* 
    //  allocate space for the line (exclude the found \r) plus
    //  space for a \0 
    */
    eptr = malloc(lptr - tptr + 1);        
    /*
    // copy everything except the \r 
    */
    memcpy(eptr, tptr, lptr - tptr);       
    /*
    //  make a c string from it 
    */
    *(eptr + (lptr - tptr)) = '\0';        
    /*
    //  filter out escape sequences 
    */
    stripesc(eptr);
    /*
    //  send the resulting line to syslog 
    */
#ifdef LOGTOSYSLOG /* workaround until i find out how makefiles work */
#ifdef LINECNT
    syslog(SYSLOGFACILITY | SYSLOGPRIORITY, "%03d: %s", linecnt++, eptr);
    if (linecnt == 101) linecnt = 0;
#else
    syslog(SYSLOGFACILITY | SYSLOGPRIORITY, "%s", eptr);
#endif
#endif
    /*
    //  release the escape-free buffer 
    */
    free(eptr);
    if ((lptr == rptr + rptrLength - 1) && (*lptr == '\r')) {
      /*
      //  the \r was the last character, we're done 
      */
      tptr = lptr + 1;
      lptr = NULL;
    } else if (lptr < rptr + rptrLength - 1) { 
      /*
      //  after the \r there are still characters left 
      //  start a new search 1 position behind the old \r 
      */
      tptr = lptr + 1;         
      if ((*tptr == '\n') && (tptr == (rptr + rptrLength - 1))) { 
        /*
        //  there is excatly 1 character left and it's a \n 
        //  ignore it, we're done 
        */
        lptr = NULL;                                              
      } else if ((*tptr == '\n') && (tptr < (rptr + rptrLength - 1))) { 
        /*
        //  the next character is a \n but there are still more after it
        //  skip the \n 
        */
        tptr++;                            
        /* start a new search for \r */
        lptr = memchr(tptr, '\r', rptrLength - (tptr - rptr)); 
      } else {                             
        /*
        //  there are other characters left 
        //  start a new search for \r 
        */
        lptr = memchr(tptr, '\r', rptrLength - (tptr - rptr)); 
      }
    } else {
      /*
      //  the \r was the last character, we're done 
      */
      lptr = NULL;                         
    }
  }
  while((tptr < (rptr + rptrLength)) && (*(tptr) == '\n')) { 
    /*
    //  skip empty lines 
    */
    tptr++;
  }  
  if ((lptr == NULL) && (tptr < (rptr + rptrLength))) {    
    /*
    //  there are characters left which are kept until a linebreak occurs
    //  move the remainings to the beginning of the buffer 
    */
    memmove(rptr, tptr, ((rptr + rptrLength) - tptr));    
    /* 
    //  recalculate the length of the rest 
    */
    rptrLength = (rptr + rptrLength) - tptr;              
    /* 
    //  cut off everything we don't need 
    */
    rptr = realloc(rptr, (size_t)rptrLength);     
  } else {
    /* 
    //  we are done. a complete line was output with no extra characters left 
    */
    free(rptr);
    rptr = NULL;
    rptrLength = 0;
  }
}

