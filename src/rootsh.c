/*

rootsh - a logging shell wrapper for root wannabes

rootsh.c contains the main program and it's utility functions

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

#include <errno.h>
#include "config.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <pwd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#if HAVE_LIBGEN_H
#  include <libgen.h>
#endif
#if HAVE_PTY_H
#  include <pty.h>
#endif

#if HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#if HAVE_FCNTL_H 
#  include <fcntl.h>
#endif
#if HAVE_STROPTS_H
#  include <stropts.h>
#endif

#if HAVE_GETOPT_H
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

#if NEED_GETUSERSHELL_PROTO
/* 
//  solaris has no own prototypes for these three functions
*/
char *getusershell(void);
void setusershell(void);
void endusershell(void);
#endif

/*
//   our own functions 
*/
void finish(int);
char *whoami(void);
char *setupusername(char *);
char *setupshell(void);
int setupusermode(void);
int setupstandalonemode(void);
int createsessionid(void);
int beginlogging(void);
void dologging(char *, int);
void endlogging(void);
int recoverfile(int, char *);
int forceopen(char *);
char *defaultshell(void);
char **saveenv(char *name);
void restoreenv(void);
void version(void);
void usage(void);
#ifdef LOGTOSYSLOG
extern void write2syslog(const void *oBuffer, size_t oCharCount);
#endif
#ifndef HAVE_FORKPTY
pid_t forkpty(int *master,  char  *name,  struct  termios *termp, struct winsize *winp);
#endif

/* 
//  global variables 
*/
extern char **environ;
extern int errno;

char progName[MAXPATHLEN];             /* used for logfile naming    */
int masterPty;
int logFile;
ino_t logInode;                        /* the inode of the logfile   */
dev_t logDev;                          /* the device of the logfile  */
char logFileName[MAXPATHLEN - 9];      /* the final logfile name     */
char *userLogFileName = NULL;          /* what the user proposes     */
char *userLogFileDir = NULL;           /* what the user proposes     */
char sessionId[MAXPATHLEN + 11];
#ifndef LOGTOFILE
int logtofile = 0;
#else 
int logtofile = 1;
#endif
#ifndef LOGTOSYSLOG
int logtosyslog = 0;
#else
int logtosyslog = 1;
#endif
struct termios termParams, newTty;
struct winsize winSize;
char *userName = NULL;                 /* the name of the calling user */
char *runAsUser = NULL;                /* the user running the shell   */
pid_t runAsUserPid;                    /* this user's pid              */
int standalone;                        /* called by sudo or not?       */
int useLoginShell = 0;                 /* invoke the shell as a login shell */
int isaLoginShell = 0;                 /* rootsh was called by login   */


int main(int argc, char **argv, char **envp) {
  char *shell = NULL, *dashShell = NULL;
  char *sucmd = SUCMD;
  fd_set readmask;
  int n, nfd, childPid;
  char buf[BUFSIZ];
  char *shellCommands = NULL;
  int c, option_index = 0;
  static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"version", 0, 0, 'V'},
      {"user", 1, 0, 'u'},
      {"initial", 0, 0, 'i'},
      {"logfile", 1, 0, 'f'},
      {"logdir", 1, 0, 'd'},
      {"no-logfile", 0, 0, 'x'},
      {"no-syslog", 0, 0, 'y'},
      {0, 0, 0, 0}
  };

  /* 
  //  this should be rootsh, but it could have been renamed 
  */
  strncpy(progName, basename(argv[0]), (MAXPATHLEN - 1));

  while (1) {
    c = getopt_long (argc, argv, "hViu:f:d:xy",
        long_options, &option_index);
    if (c == -1) {
      /*
      //  arguments not belonging to known switches are treated
      //  as commands which will be passed to the shell
      */
      while (optind < argc) {
        if (! shellCommands) {
          shellCommands = calloc(sizeof(char), 1);
        }
        shellCommands = realloc(shellCommands, 
            strlen(shellCommands) + strlen(argv[optind]) + 2);
        strcat(shellCommands, argv[optind]);
        strcat(shellCommands, " ");
        optind++;
      }
      break;
    }
    switch (c) {
      case 'h':		
      case '?':
        usage();
        break;
      case 'V':
        version();
        break;
      case 'i':
        useLoginShell = 1;
        break;
      case 'u':
        runAsUser = strdup(optarg);
        break;
      case 'f':
        userLogFileName = strdup(optarg);
        break;
      case 'd':
        userLogFileDir = strdup(optarg);
        break;
      case 'x':
        logtofile = 0;
        break;
      case 'y':
        logtosyslog = 0;
        break;
      default:
        usage ();
      }
  }
/*
fprintf(stderr, "shell=:%s:\ndashShell=:%s:\nsucmd=:%s:\nshellCommands=:%s:\nsessionId=:%s:\n-------------------------------------\n",
    (shell == NULL) ? "..." : shell, (dashShell == NULL) ? "..." : dashShell, 
    (sucmd == NULL) ? "..." : sucmd, 
    (shellCommands == NULL)? "..." : shellCommands,
    (sessionId == NULL) ? "..." : sessionId);
*/
  if (! createsessionid()) {
    exit(EXIT_FAILURE);
  }
/*
fprintf(stderr, "shell=:%s:\ndashShell=:%s:\nsucmd=:%s:\nshellCommands=:%s:\nsessionId=:%s:\n-------------------------------------\n",
    (shell == NULL) ? "..." : shell, (dashShell == NULL) ? "..." : dashShell, 
    (sucmd == NULL) ? "..." : sucmd, 
    (shellCommands == NULL)? "..." : shellCommands,
    (sessionId == NULL) ? "..." : sessionId);
*/
  if ((userName = setupusername(shellCommands)) == NULL) {
    exit(EXIT_FAILURE);
  }
/*
fprintf(stderr, "shell=:%s:\ndashShell=:%s:\nsucmd=:%s:\nshellCommands=:%s:\nsessionId=:%s:\n-------------------------------------\n",
    (shell == NULL) ? "..." : shell, (dashShell == NULL) ? "..." : dashShell, 
    (sucmd == NULL) ? "..." : sucmd, 
    (shellCommands == NULL)? "..." : shellCommands,
    (sessionId == NULL) ? "..." : sessionId);
*/
  if (! setupusermode()) {
    exit(EXIT_FAILURE);
  }

  if (! setupstandalonemode()) {
    exit(EXIT_FAILURE);
  }
  
  if ((shell = setupshell()) == NULL) {
    exit(EXIT_FAILURE);
  }

  if (! beginlogging()) {
    exit(EXIT_FAILURE);
  }

  /* 
  //  save original terminal parameters 
  */
  tcgetattr(STDIN_FILENO, &termParams);
  /*
  //  save original window size 
  */
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&winSize);
  
  /* 
  //  fork a child process, create a pty pair, 
  //  make the slave the controlling terminal,
  //  create a new session, become session leader 
  //  and attach filedescriptors 0-2 to the slave pty 
  */
  if ((childPid = forkpty(&masterPty, NULL, &termParams, &winSize)) < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if (childPid == 0) {
    /* 
    //  this process will exec a shell
    */
    /*
    //  if rootsh was called with the -u user parameter, we will exec
    //  su user instead of a shell
    */
    if (runAsUser) {
      shell = sucmd;
    }
    /*
    //  if rootsh was called with the -i parameter (initial login)
    //  then prepend the shell's basename with a dash.
    //  otherwise call it as interactive shell.
    */
    if (useLoginShell) {
      dashShell = strdup(shell);
      dashShell = strrchr(dashShell, '/');
      dashShell[0] = '-';
    }
    if (runAsUser && useLoginShell && shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), "-", runAsUser, "-c", shellCommands, 0);
    } else if (runAsUser && useLoginShell && !shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), "-", runAsUser, 0);
    } else if (runAsUser && !useLoginShell && shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), runAsUser, "-c", shellCommands, 0);
    } else if (runAsUser && !useLoginShell && !shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), runAsUser, 0);
    } else if (!runAsUser && useLoginShell && shellCommands) {
      execl(shell, dashShell, "-c", shellCommands, 0);
    } else if (!runAsUser && useLoginShell && !shellCommands) {
      execl(shell, dashShell, 0);
    } else if (!runAsUser && !useLoginShell && shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), "-i", "-c", shellCommands, 0);
    } else if (!runAsUser && !useLoginShell && !shellCommands) {
      execl(shell, (strrchr(shell, '/') + 1), "-i", 0);
    } else {
      perror(shell);
    }
    perror(progName);
  } else {
    /* 
    //  handle these signals (posix functions preferred) 
    */
#if defined(HAVE_SIGACTION)
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = finish;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
#elif defined(HAVE_SIGSET)
    sigset(SIGINT, finish);
    sigset(SIGQUIT, finish);
#else
    signal(SIGINT, finish);
    signal(SIGQUIT, finish);
#endif
    newTty = termParams;
    /* 
    //  let read not return until at least 1 byte has been received 
    */
    newTty.c_cc[VMIN] = 1; 
    newTty.c_cc[VTIME] = 0;
    /* 
    //  don't perform output processing
    */
    newTty.c_oflag &= ~OPOST;
    /* 
    //  noncanonical input |signal characters off|echo off 
    */
    newTty.c_lflag &= ~(ICANON|ISIG|ECHO);
    /* 
    //  NL to CR off|don't ignore CR|CR to NL off|
    //  case sensitive input|no output flow control 
    */
    newTty.c_iflag &= ~(INLCR|IGNCR|ICRNL|
    /* 
    //  FreeBSD doesnt know this one 
    */
#ifdef IUCLC 
        IUCLC|
#endif
        IXON);

    /* 
    //  set the new tty modes 
    */
    if (tcsetattr(0, TCSANOW, &newTty) < 0) {
        perror("tcsetattr: stdin");
        exit(EXIT_FAILURE);
    }
    /* 
    //  now just sit in a loop reading from the keyboard and
    //  writing to the pseudo-tty, and reading from the  
    //  pseudo-tty and writing to the screen and the logfile.
    */
    for (;;) {
      /* 
      //  watch for users terminal and master pty to change status 
      */
      FD_ZERO(&readmask);
      FD_SET(masterPty, &readmask);
      FD_SET(0, &readmask);
      nfd = masterPty + 1;

      /* 
      //  wait for something to read 
      */
      n = select(nfd, &readmask, (fd_set *) 0, (fd_set *) 0,
        (struct timeval *) 0);
      if (n < 0) {
        perror("select");
        exit(EXIT_FAILURE);
      }

      /* 
      //  the user typed something... 
      //  read it and pass it on to the pseudo-tty 
      */
      if (FD_ISSET(0, &readmask)) {
        if ((n = read(0, buf, sizeof(buf))) < 0) {
          perror("read: stdin");
          exit(EXIT_FAILURE);
        }
        if (n == 0) {
          /* 
          //  the user typed end-of-file; we're done 
          */
          finish(0);
        }
        if (write(masterPty, buf, n) != n) {
          perror("write: pty");
          exit(EXIT_FAILURE);
        }
      }

      /* 
      //  there's output on the pseudo-tty... 
      //  read it and pass it on to the screen and the script file 
      //  echo is on, so we see here also the users keystrokes 
      */
      if (FD_ISSET(masterPty, &readmask)) {
        /* 
        //  the process died 
        */
        if ((n = read(masterPty, buf, sizeof(buf))) <= 0) {
          finish(0);
        } else {
          dologging(buf, n);
          write(STDIN_FILENO, buf, n);
        }
      }
    }
  }
  exit(EXIT_SUCCESS);
}



/* 
//  do some cleaning after the child exited 
//  this is the signal handler for SIGINT and SIGQUIT 
*/

void finish(int sig) {
  char msgbuf[BUFSIZ];
  int msglen;

  /* restore original tty modes */
  if (tcsetattr(0, TCSANOW, &termParams) < 0)
      perror("tcsetattr: stdin");
  if (sig == 0) {
    msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
        "\n*** %s session ended by user\r\n", progName);
  } else {
    msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
        "\n*** %s session interrupted by signal %d\r\n", progName, sig);
  }
  dologging(msgbuf, msglen);
  endlogging();
  close(masterPty);
  exit(EXIT_SUCCESS);
}



/* 
//  create a session identifier which helps you identify the lines
//  which belong to the same session when browsing large logfiles.
//  also used in the logfiles name.
//  for each host there may be no sessions running at the same time
//  having the same identifier.
*/

int createsessionid(void) {
  /* 
  //  the process number is always unique
  */
  snprintf(sessionId, sizeof(sessionId), "%s-%04x", 
     *progName == '-' ? progName + 1 : progName, getpid());
  return(1);
}
  


/* 
//  open a logfile and initialize syslog. 
//  generate a random session identifier.
*/

int beginlogging(void) {
#ifdef LOGTOFILE
  time_t now;
  char msgbuf[BUFSIZ];
  char defaultLogFileName[MAXPATHLEN - 7];
  int sec, min, hour, day, month, year, msglen;
  struct stat statBuf;
#endif

  if (logtofile == 0 && logtosyslog == 0) {
    fprintf(stderr, "you cannot switch off both file and syslog logging\n");
    return (0);
  }

#ifdef LOGTOFILE
  if (logtofile) {
    /*
    //  Construct the logfile name. 
    //  LOGDIR/<username>.YYYY.MM.DD.HH.MI.SS.<sessionId>
    //  In standalone mode, a user may propose his own filename
    //  When the session is over, the logfile will be renamed 
    //  to <logfile>.closed.
    //  If we don't log to a file at all, don't mention 
    //  a filename in the syslog logs.
    */
    now = time(NULL);
    year = localtime(&now)->tm_year + 1900;
    month = localtime(&now)->tm_mon + 1;
    day = localtime(&now)->tm_mday;
    hour = localtime(&now)->tm_hour;
    min = localtime(&now)->tm_min;
    sec = localtime(&now)->tm_sec;
    snprintf(defaultLogFileName, (sizeof(logFileName) - 1), 
        "%s.%04d%02d%02d%02d%02d%02d.%s", 
         userName, year,month, day, hour, min, sec,
         strrchr(sessionId, '-') + 1);
    if (standalone) {
      if (userLogFileName && userLogFileDir) {
        snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
            userLogFileDir, userLogFileName);
      } else if (userLogFileName && ! userLogFileDir) {
        if (userLogFileName[0] == '/') {
          snprintf(logFileName, (sizeof(logFileName) - 1), "%s",
              userLogFileName);
        } else {
          snprintf(logFileName, (sizeof(logFileName) - 1), "./%s",
              userLogFileName);
        }
      } else if (! userLogFileName && userLogFileDir) {
        snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
            userLogFileDir, defaultLogFileName);
      } else {
        snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
            LOGDIR, defaultLogFileName);
      }
    } else {
      snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
          LOGDIR, defaultLogFileName);
    }
    /* 
    //  Open the logfile 
    */
    if ((logFile = open(logFileName, O_RDWR|O_CREAT|O_EXCL|O_SYNC, S_IRUSR|S_IWUSR)) == -1) {
      perror(logFileName);
      return(0);
    }
    /*
    //  Remember inode and device. We will later see if the logfile
    //  we just opened is the same that we will close.
    */
    if (fstat(logFile, &statBuf) == -1) {
      perror(logFileName);
      return(0);
    }
    logInode = statBuf.st_ino;
    logDev = statBuf.st_dev;
    /* 
    //  Note the start time in the log file 
    */
    msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
        "%s%s session opened for %s as %s on %s at %s", 
         isaLoginShell ? "login " : "", progName, userName, 
         runAsUser ? runAsUser : getpwuid(getuid())->pw_name, 
         ttyname(0), ctime(&now)); 
    write(logFile, msgbuf, msglen);
  }
#endif
#ifdef LOGTOSYSLOG
  if(logtosyslog) {
    /* 
    //  Prepare usage of syslog with sessionid as prefix 
    */
    openlog(sessionId, LOG_NDELAY, SYSLOGFACILITY);
    /* 
    //  Note the log file name in syslog if there is one
    */
    if (logtofile) {
      syslog(SYSLOGFACILITY | SYSLOGPRIORITY, 
          "%s=%s,%s: logging new %s session (%s) to %s", 
          userName, runAsUser ? runAsUser : getpwuid(getuid())->pw_name, 
          ttyname(0), isaLoginShell ? "login" : "", sessionId, logFileName);
    } else {
      syslog(SYSLOGFACILITY | SYSLOGPRIORITY, 
          "%s=%s,%s: logging new %s session (%s)", 
          userName, runAsUser ? runAsUser : getpwuid(getuid())->pw_name, 
          ttyname(0), isaLoginShell ? "login" : "", sessionId);
    }
  }
#endif
  return(1);
}


/*
//  Send a buffer full of output to the selected logging destinations
//  Either to a local logfile or to the syslog server or both
*/

void dologging(char *msgbuf, int msglen) {
#ifdef LOGTOFILE
  if (logtofile) {
    write(logFile, msgbuf, msglen);
  }
#endif
#ifdef LOGTOSYSLOG
  if (logtosyslog) {
    write2syslog(msgbuf, msglen);
  }
#endif
}


/* 
//  Send a final cr-lf to flush the log.
//  Close the logfile and syslog.
//  Examine inode and device of the logfile to find traces of manipulation.
//  Append ".tampered" to the recovered logfile's name if something was found.
//  Append ".closed" to the logfile's name otherwise.
*/

void endlogging() {
#ifdef LOGTOFILE
  time_t now;
  int msglen;
  char msgbuf[BUFSIZ];
  struct stat statBuf;
  char closedLogFileName[MAXPATHLEN];
#endif

#ifdef LOGTOFILE
  if (logtofile) {
    now = time(NULL);
    msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
        "%s session closed for %s on %s at %s", 
        *progName == '-' ? progName + 1 : progName,
        userName, ttyname(0), ctime(&now)); 
    write(logFile, msgbuf, msglen);
    /*
    //  From here on, a filled message buffer means an error has occurred
    */
    msgbuf[0] = '\0';
    if (stat(logFileName, &statBuf) == -1) {
      /*
      //  There is no file named logFileName.
      */
      if (fstat(logFile, &statBuf) == -1) {
        /*
        //  Even the open file descriptor does not work.
        */
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** THIS FILEHANDLE HAS BEEN DELETED ***\r\n");
      } else {
        /*
        //  The file ist still reachable via file descriptor.
        */
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** USER TRIED TO DELETE THIS FILE ***\r\n");
      }
    } else {
      /*
      //  A file with the correct name and path was found. Now look
      //  for manipulations.
      */
      if ((logInode != statBuf.st_ino) || (logDev != statBuf.st_dev)) {
        /*
        //  Device or inode have changed. This is not the file we opened,
        //  it has just the same name.
        */
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** USER TRIED TO DELETE AND RECREATE THIS FILE ***\r\n");
        unlink(logFileName) && rmdir(logFileName);
      } else {
        if (fstat(logFile, &statBuf) == -1) {
          /*
          //  Something bad happened to the file descriptor.
          //  There's not much i can do here.
          */
          msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
              "*** THIS FILEHANDLE HAS BEEN DELETED ***\r\n");
        }
      }
    }
    if (strlen(msgbuf) > 0) {
      /*
      //  There is an error message. Send publish it and then try to
      //  save the contents of the (manipulated) logfile into a new
      //  file <logfile>.tampered
      */
      dologging(msgbuf, msglen);
      snprintf(closedLogFileName, sizeof(closedLogFileName), "%s.tampered",
          logFileName);
      if (! recoverfile(logFile, strncat(logFileName, ".tampered",
          sizeof(logFileName)))) {
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** THIS LOGFILE CANNOT BE RECOVERED ***\r\n");
      } else {
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** MANIPULATED LOGFILE RECOVERED ***\r\n");
      }
      dologging(msgbuf, msglen);
      close(logFile);
    } else {
      close(logFile);
      snprintf(closedLogFileName, sizeof(closedLogFileName), "%s.closed",
          logFileName);
      rename(logFileName, closedLogFileName);
    } 
  }
#endif
#ifdef LOGTOSYSLOG
  if (logtosyslog) {
    write2syslog("\r\n", 2);
    syslog(SYSLOGFACILITY | SYSLOGPRIORITY, "%s,%s: closing %s session (%s)", 
        userName, ttyname(0), progName, sessionId);
    closelog();
  }
#endif
}


/*
//  Try to save the contents of a deleted file with a still open
//  filehandle to another file
*/

int recoverfile(int ohandle, char *recoverFileName) {
  char msgbuf[BUFSIZ];
  int fd, n;
  if ((fd = forceopen(recoverFileName)) != -1) {
    lseek(ohandle, 0, SEEK_SET);
    while ((n = read(ohandle, (void *)msgbuf, BUFSIZ)) > 0) {
      if (write(fd, (void *)msgbuf, n) !=n) {
        perror("write recoverfile");
      }
    }
    close(fd);
    return (1);
  } else {
    return (0);
  }
}


/*
//  Try to open/create a file even if it already exists as a directory
//  or link.
*/

int forceopen(char *path) {
  int tries = 0;
  int fd;
  int msglen;
  char msgbuf[BUFSIZ];
  while ((fd = open(path, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) == -1) {
    if (++tries >= 3)
      break;
    switch(errno) {
      case EEXIST:
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** FILE ALREADY EXISTS ***\r\n");
        unlink(path);
        break;
      case EISDIR:
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** FILE ALREADY EXISTS AS A DIRECTORY ***\r\n");
        rmdir(path);
        break;
      default:
        msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
            "*** FILE COULD NOT BE CREATED (ERRNO %d)  ***\r\n", errno);
        break;
    }
    dologging(msgbuf, msglen);
  }
  return (fd);
}


/*
//  find out the username of the calling user
*/

char *setupusername(char *shcmd) {
  char *userName = NULL;
  struct stat ttybuf;

/*
fprintf(stderr, "enter getlogin with sc :%s: and ttx :%s:\n", shcmd, ttyname(STDIN_FILENO));
*/
/*
if(isatty(STDIN_FILENO)) {
fprintf(stderr, "its a tty\n");
}
if (ttyname(STDIN_FILENO) == NULL) {
fprintf(stderr, "ttyname is null\n");
}
if (getlogin() == NULL) {
fprintf(stderr, "getlogin is null\n");
}
*/
  if((userName = getlogin()) == NULL) {
    /* 
    //  HP-UX returns NULL here so we take the controlling terminal's owner 
    */
    if(ttyname(0) != NULL) {
      if (stat(ttyname(0), &ttybuf) == 0) {
        if ((userName = getpwuid(ttybuf.st_uid)->pw_name) == NULL) {
          fprintf(stderr, "i don\'t know who you are\n");
        }
      }
    } else {
      /* 
      //  rootsh must be run interactively 
      */
      fprintf(stderr, "i don\'t know who you are\n");
    }
  }
  return userName;
}



/* 
//  find out which shell to use. return the pathname of the shell 
//  or NULL if an error occurred 
*/ 

char *setupshell() {
  int isvalid;
  char *shell, *shellenv;
  char *validshell;
  
  /* 
  //  try to get the users current shell with two methods 
  */
  if ((shell = getenv("SHELL")) == NULL) {
    shell = getpwnam(userName)->pw_shell;
  }
  if (shell == NULL) {
    fprintf(stderr, "could not determine a valid shell\n");
  } else {
#if HAS_ETC_SHELLS
    /*
    //  compare it to the allowed shells in /etc/shells 
    */
    isvalid = 0;
    for (setusershell(), validshell = getusershell(); validshell && ! isvalid; validshell = getusershell()) {
      if (strcmp(validshell, shell) == 0) {
        isvalid = 1;
      }
    }
    endusershell();
#else
    isvalid = 1;
#endif

    /* 
    //  do not allow invalid shells 
    */
    if (isvalid == 0) {
      fprintf(stderr, "%s is not in /etc/shells\n", shell);
      return(NULL);
    }
    /*
    //  if SHELL is /bin/rootsh and argv[0] is -rootsh we were probably
    //  started as a commandline interpreter found in /etc/passwd
    */
    if (*progName == '-' && ! strcmp(basename(shell), progName + 1)) {
      shell = calloc(sizeof(char), strlen(defaultshell()) + 1);
      strcpy(shell, defaultshell());
      shellenv = calloc(sizeof(char), strlen(shell) + 7);
      sprintf(shellenv, "SHELL=%s", shell);
      putenv(shellenv);
      useLoginShell = 1;
      isaLoginShell = 1;
    }
  }
  return(shell);
}


/*
//  if a username was given on the command line via -u 
//  see, if it has an acceptable length (yes, some have 64 character usernames)
//  see, if it exists. 
//  get the uid. 
//  clean up the environment
//  if not, forget this and run as root
*/

int setupusermode(void) {
  struct passwd *pass;

#ifndef SUCMD
  fprintf(stderr, "user mode is not possible with this version of %s\n", progName);
  return(1);
#endif
  if (runAsUser == NULL) {
    return(1);
  } else if (strlen(runAsUser) > 64) {
    fprintf(stderr, "this username is too long. i don\'t trust you\n");
    return(0);
  } else {
    if ((pass = getpwnam(runAsUser)) == NULL) {
      fprintf(stderr, "user %s does not exist\n", runAsUser);
      return(0);
    } else {
      runAsUserPid = pass->pw_uid;
      saveenv("TERM");
#if HAVE_CLEARENV
      clearenv();
#else
      environ = NULL;
#endif
      restoreenv();
      return (1);
    }
  }
}


/*
//  find out, if rootsh was started via sudo or as a standalone command
//  if rootsh was called directly instead of via sudo the user
//  can provide additional command line options 
*/

int setupstandalonemode(void) {
  standalone = (getenv("SUDO_USER") == NULL) ? 1 : 0;
  return (1);
}

/*
//  If rootsh is used as user command interpreter in /etc/passwd, we have
//  to find an alternative shell which will handle the users commands.
*/

char *defaultshell(void) {
  char *defaultshell = DEFAULTSHELL;
  if (strlen(defaultshell) > 0) {
    return defaultshell;
  } else {
    /*
    //  maybe there are other possibilities to find a suitable shell
    */
    return NULL;
  }
}


/*
//  Save an environment variable to a copy of the system pointer environ.
//  The pointer is static so we can later create a new environment from
//  the saved variables.
//  Calling savenv with a NULL name simply returns the pointer to
//  the saved environment.
*/
char **saveenv(char *name) {
  static char **senv = NULL;
  static char **senvp;
  if (name == NULL) {
    return senv;
  } else {
    /*
    //  Only save existing variables
    */
    if (getenv(name) != NULL) {
      if (senv == NULL) {
        /*
        //  We are called for the first time. Allocate memory to
        //  a pointer containing name=value and a closing null pointer.
        */
        senv = (char **)calloc(2, sizeof(char *));
      } else {
        /*
        //  Allocate memory for another pointer
        */
        senv = realloc(senv, (senvp - senv + 2) * sizeof(char *));
      }
      senvp = senv;
      /*
      //  Now position senvp to the closing null pointer or to an
      //  already existing name=value entry
      */
      while (*senvp != NULL && 
          strncmp(*senvp, name, strchr(*senvp, '=') - *senvp)) {
        senvp++;
      }
      if (*senvp == NULL) {
        /*
        //  A pointer to the new name=value will be attached to senv.
        //  The closing NULL pointer moves one position up.
        */
        *senvp = malloc(strlen(name) + strlen(getenv(name)) + 2);
        sprintf(*senvp, "%s=%s", name, getenv(name));
        *(++senvp) = NULL;
      }
    }
    return senv;
  }
}


/*
//  Rebuild the environment from saved variables.
*/
void restoreenv(void) {
  char **senv;
  senv = saveenv(NULL);
  while (*senv != NULL) {
    putenv(*senv++);
  }
}

#ifndef HAVE_FORKPTY
/* 
//  emulation of the BSD function forkpty 
*/
#ifndef MASTERPTYDEV
#  error you need to specify a master pty device
#endif
pid_t forkpty(int *amaster,  char  *name,  struct  termios *termp, struct winsize *winp) {
  struct termios currentterm;
  struct winsize currentwinsize;
  pid_t pid;
  int master, slave;
  char *slavename;
  char *mastername = MASTERPTYDEV;

  /* 
  //  get current settings if termp was not provided by the caller 
  */
  if (termp == NULL) {
    tcgetattr(STDIN_FILENO, &currentterm);
    termp = &currentterm;
  }

  /* 
  //  same for window size 
  */
  if (winp == NULL) {
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&currentwinsize);
    winp->ws_row = currentwinsize.ws_row;
    winp->ws_col = currentwinsize.ws_col;
    winp->ws_xpixel = currentwinsize.ws_xpixel;
    winp->ws_ypixel = currentwinsize.ws_ypixel;
  }

  /*
  //  get a master pseudo-tty 
  */
  if ((master = open(mastername, O_RDWR)) < 0) {
    perror(mastername);
    return(-1);
  }

  /*
  //  set the permissions on the slave pty 
  */
  if (grantpt(master) < 0) {
    perror("grantpt");
    close(master);
    return(-1);
  }

  /*
  //  unlock the slave pty 
  */
  if (unlockpt(master) < 0) {
    perror("unlockpt");
    close(master);
    return(-1);
  }

  /*
  //  start a child process 
  */
  if ((pid = fork()) < 0) {
    perror("fork in forkpty");
    close(master);
    return(-1);
  }

  /*
  //  the child process will open the slave, which will become
  //  its controlling terminal 
  */
  if (pid == 0) {
    /*
    //  get rid of our current controlling terminal 
    */
    setsid();

    /*
    //  get the name of the slave pseudo tty 
    */
    if ((slavename = ptsname(master)) == NULL) {
      perror("ptsname");
      close(master);
      return(-1);
    }

    /* 
    //  open the slave pseudo tty 
    */
    if ((slave = open(slavename, O_RDWR)) < 0) {
      perror(slavename);
      close(master);
      return(-1);
    }

#ifndef HAVE_AIX_PTY
    /*
    //  push the pseudo-terminal emulation module 
    */
    if (ioctl(slave, I_PUSH, "ptem") < 0) {
      perror("ioctl: ptem");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  push the terminal line discipline module 
    */
    if (ioctl(slave, I_PUSH, "ldterm") < 0) {
      perror("ioctl: ldterm");
      close(master);
      close(slave);
      return(-1);
    }

#ifdef HAVE_TTCOMPAT
    /*
    //  push the compatibility for older ioctl calls module (solaris) 
    */
    if (ioctl(slave, I_PUSH, "ttcompat") < 0) {
      perror("ioctl: ttcompat");
      close(master);
      close(slave);
      return(-1);
    }
#endif
#endif

    /*
    //  copy the caller's terminal modes to the slave pty 
    */
    if (tcsetattr(slave, TCSANOW, termp) < 0) {
      perror("tcsetattr: slave pty");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  set the slave pty window size to the caller's size 
    */
    if (ioctl(slave, TIOCSWINSZ, winp) < 0) {
      perror("ioctl: slave winsz");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  close the logfile and the master pty
    //  no need for these in the slave process 
    */
    close(master);
    if (logtofile) {
      close(logFile);
    }
    if (logtosyslog) {
      closelog();
    }
    /*
    //  set the slave to be our standard input, output,
    //  and error output.  Then get rid of the original
    //  file descriptor 
    */
    dup2(slave, 0);
    dup2(slave, 1);
    dup2(slave, 2);
    close(slave);
    /*
    //  if the caller wants it, give him back the slave pty's name 
    */
    if (name != NULL) strcpy(name, slavename);
    return(0); 
  } else {
    /*
    //  return the slave pty device name if caller wishes so 
    */
    if (name != NULL) {          
      if ((slavename = ptsname(master)) == NULL) {
        perror("ptsname");
        close(master);
        return(-1);
      }
      strcpy(name, slavename);
    }
    /*
    //  return the file descriptor for communicating with the process
    //  to our caller 
    */
    *amaster = master; 
    return(pid);      
  }
}
#endif

void version() {
  printf("%s version %s\n", progName,VERSION);
#ifdef LOGTOFILE
  printf("logfiles go to directory %s\n", LOGDIR);
#endif
#ifdef LOGTOSYSLOG
  printf("syslog messages go to priority %s.%s\n", SYSLOGFACILITYNAME, SYSLOGPRIORITYNAME);
#else
  printf("no syslog logging\n");
#endif
#ifdef LINECNT
  printf("line numbering is on\n");
#else
  printf("line numbering is off\n");
#endif
#ifndef SUCMD
  printf("running as non-root user is not possible\n");
#endif
#ifdef DEFAULTSHELL
  printf("%s can be used as login shell. %s will interpret your commands\n",
      progName, DEFAULTSHELL);
#endif
  exit(0);
}


void usage() {
  printf("Usage: %s [OPTION [ARG]] ...\n"
    " -?, --help            show this help statement\n"
    " -i, --login           start a (initial) login shell\n"
    " -u, --user=username   run shell as a different user\n"
    " -f, --logfile=file    name of your logfile (standalone only)\n"
    " -d, --logdir=DIR      directory for your logfile (standalone only)\n"
    " --no-logfile          switch off logging to a file (standalone only)\n"
    " --no-syslog           switch off logging to syslog (standalone only)\n"
    " -V, --version         show version statement\n", progName);
  exit(0);
}
