/*

rootsh - a logging shell wrapper for root wannabes

rootsh.c contains the main program and it's utility functions

Copyright (C) 2004 Gerhard Lausser

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

#ifdef S_SPLINT_S
#  include <err.h>
#endif
#include <sys/ioctl.h>
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
#include <wordexp.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#if HAVE_LIBGEN_H
#  include <libgen.h>
#endif
#if HAVE_PTY_H
#  include <pty.h>
#endif
#if HAVE_UTIL_H
#  include <util.h>
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
void logSession(const int);
void execShell(const char *, const char *);
char *setupusername(void);
char *setupshell(void);
int setupusermode(void);
void finish(int);
char* consume_remaining_args(int, char **, char *);
void handle_sig_winch(int);
int beginlogging(const char *);
void dologging(char *, int);
void endlogging(void);
int recoverfile(int, char *);
int forceopen(char *);
char *defaultshell(void);
char **saveenv(char *);
void restoreenv(void);
#ifndef HAVE_CLEARENV
int clearenv(void);
#endif
void version(void);
void usage(void);
#ifdef LOGTOSYSLOG
extern void write2syslog(const void *, size_t);
#endif
#ifndef HAVE_FORKPTY
pid_t forkpty(int *, char *, struct termios *, struct winsize *);
#endif
char **build_scp_args( char *str, size_t reserve );

/* 
//  global variables 
//
//
//  environ, errno	These are imported variables. See the manpages.
//
//  progName		The name of this executable as passed in argv[0].
//
//  sessionId		A unique identifier. It's made up of the 
//			program's name (without a leading "-") and
//			a 4-digit hex representation of the process id.
//
//  sessionIdWithUid	The same identifier extended with  a colon and 
//			the calling user's username. Used for extended
//			logging to syslog.
//
//			program's name (without a leading "-") and
//			a 4-digit hex representation of the process id.
//
//  logFile		The file descriptor of the logfile.
//
//  logFileName		The name of the logfile, generated from userName,
//			a timestamp and the pid. May be overwritten by
//			userLogFileName.
//
//  userLogFileName	A user supplied filename for the logfile.
//
//  userLogFileDir	A user supplied directory where the logfile 
//			will be created.
//
//  logInode		The inode of the logfile.
//
//  logDev		The device of the logfile. These two values will
//			be determined when the logfile will be opened
//			and before the logfile will be closed. Should they
//			be different, then somebody manipulated the logfile.
//			
//  logtofile		A flag indicating that logging to a file has 
//			been switched off with --no-logfile.
//
//  logtosyslog		A flag indicating that logging to syslog has
//			been switched off with --no-logfile.
//
//  userName		The name of the user who called this executable.
//
//  runAsUser		The name of the user under whose identity the shell
//			or command will run.
//
//  standalone		A flag indicating that this program has been started
//			without calling sudo.
//
//  useLoginShell	A flag indication that the shell will be called as
//			a login shell with "-" as first character in argv[0].
//
//  isaLoginShell	A flag indicating that this executable was called
//			by the login process.
//
//  masterPty		The pty pair which will be the controlling terminal
//			of the shell.
//
//  termParams		A structure where the terminal parameters are saved
//			before the shell is executed. These parameters will
//			be restored at program end.
//
//  newTty		A structure with terminal parameters which will be
//			set as long as input/output will be sent/read to/from
//			the pty pair.
//
//  winSize		The size of the calling terminal. The slave pty will
//			be set to these values.
//  
*/
extern char **environ;

volatile sig_atomic_t sigwinch_received;

static char progName[MAXPATHLEN];
static char sessionId[MAXPATHLEN + 11];
static int logFile;
static char logFileName[MAXPATHLEN - 9];
static char *userLogFileName;
static char *userLogFileDir;
static ino_t logInode;
static dev_t logDev;
#ifndef LOGTOFILE
static int logtofile = 0;
#else 
static int logtofile = 1;
#endif
#ifndef LOGTOSYSLOG
static int logtosyslog = 0;
#else
static int logtosyslog = 1;
#endif
static char *userName;
static char *runAsUser;
static int standalone;
static int useLoginShell = 0;
static int isaLoginShell = 0;
static int masterPty;
static struct termios termParams, newTty;
static struct winsize winSize;


int main(int argc, char **argv) {
  /*
  //  shell		The path to the shell which will be executed.
  //
  //  sessionIdEnv	A pointer to an environment variable assignment
  //			ROOTSH_SESSIONID=sessionId.
  //
  //  childPid		The pid of the process executing the shell.
  //  
  //  shellCommands	The commands which will be executed instead of
  //			an interactive shell.
  //  
  //  c, option_index	Used by getopt_long.
  //
  //  long_options	Used by getopt_long.
  */
  char *shell, *shellCommands = NULL;
  static char sessionIdEnv[sizeof(sessionId) + 17];
  int childPid;
  int c, option_index = 0;
  struct option long_options[] = {
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
  //  This should be rootsh, but it could have been renamed.
  */
  strncpy(progName, basename(argv[0]), (MAXPATHLEN - 1));

  while (1) {
    c = getopt_long (argc, argv, "hViu:f:d:xyc:",
        long_options, &option_index);
    if (c == -1) {
      /*
      //  Arguments not belonging to known switches are treated
      //  as commands which will be passed to the shell.
      */
      shellCommands = consume_remaining_args(argc, argv, shellCommands);
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
      case 'c':
        /* back up 1 argument to allow consume_remaining_args to get the current arg to the -c */
        --optind;
        
        /* Everything after -c should be passed directly to the execed shell
         * to be POSIX compliant */
        shellCommands = consume_remaining_args(argc, argv, shellCommands);
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

  snprintf(sessionId, sizeof(sessionId), "%s[%05x]", 
      *progName == '-' ? progName + 1 : progName, getpid());
  snprintf(sessionIdEnv, sizeof(sessionIdEnv), "ROOTSH_SESSIONID=%s",
      sessionId);
  /*
  //  Create an environment variable ROOTSH_SESSIONID with the sessionId
  //  as its value.
  */
  putenv(sessionIdEnv);

  standalone = ((getenv("SUDO_USER") == NULL) ? 1 : 0);

  if ((userName = setupusername()) == NULL) {
    exit(EXIT_FAILURE);
  }

  if (! setupusermode()) {
    exit(EXIT_FAILURE);
  }
  
  if ((shell = setupshell()) == NULL) {
    exit(EXIT_FAILURE);
  }

  if (! beginlogging(shellCommands)) {
    exit(EXIT_FAILURE);
  }

  if(isatty(0)) {
    /* 
    //  Save original terminal parameters.
    */
    tcgetattr(STDIN_FILENO, &termParams);
    /*
    //  Save original window size.
    */
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&winSize);
  
    /* 
    //  fork a child process, create a pty pair, 
    //  make the slave the controlling terminal,
    //  create a new session, become session leader 
    //  and attach filedescriptors 0-2 to the slave pty.
    */
    if ((childPid = forkpty(&masterPty, NULL, &termParams, &winSize)) < 0) {
      perror("fork");
      exit(EXIT_FAILURE);
    }
    if (childPid == 0) {
      execShell(shell, shellCommands);
    } else {
      logSession(childPid);
    }
  } else {
    int msglen;
    char msgbuf[BUFSIZ];

    /* note that it's not a tty and exec the shell */
    msglen = snprintf(msgbuf, (sizeof(msgbuf) -1),
                      "Not a tty, just execing command\n");
    dologging(msgbuf, msglen);
    endlogging();
    execShell(shell, shellCommands);
  }
  exit(EXIT_SUCCESS);
}

void logSession(const int childPid) {
  /*
  //  readmask		A set of filedescriptors to watch. Here we have
  //			the stdin of the calling terminal and the stdout.
  //  
  //  n			Either the filedescriptor which changed state in
  //			the event loop or a simple counter.
  //  buf		A buffer used for various red/write operations.
  //  
  //  
  */
  int n;
  fd_set readmask;
  char buf[BUFSIZ];

  /* 
  //  Handle these signals (posix functions preferred).
  */
#if HAVE_SIGACTION
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = finish;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
#elif HAVE_SIGSET
  sigset(SIGINT, finish);
  sigset(SIGQUIT, finish);
#else
  signal(SIGINT, finish);
  signal(SIGQUIT, finish);
#endif
  signal(SIGWINCH, handle_sig_winch);
  sigwinch_received = 0;

  newTty = termParams;
  /* 
  //  Let read not return until at least 1 byte has been received.
  */
  newTty.c_cc[VMIN] = 1; 
  newTty.c_cc[VTIME] = 0;
  /* 
  //  Don't perform output processing.
  */
  newTty.c_oflag &= ~OPOST;
  /* 
  //  Noncanonical input | signal characters off |echo off.
  */
  newTty.c_lflag &= ~(ICANON|ISIG|ECHO);
  /* 
  //  NL to CR off | don't ignore CR | CR to NL off |
  //  Case sensitive input (not known to FreeBSD) | 
  //  no output flow control 
  */
#ifdef IUCLC
  newTty.c_iflag &= ~(INLCR|IGNCR|ICRNL|IUCLC|IXON);
#else
  newTty.c_iflag &= ~(INLCR|IGNCR|ICRNL|IXON);
#endif
  /* 
  //  Set the new tty modes.
  */
  if (tcsetattr(0, TCSANOW, &newTty) < 0) {
    perror("tcsetattr: stdin");
    exit(EXIT_FAILURE);
  }
  
  /* 
  //  Now just sit in a loop reading from the keyboard and
  //  writing to the pseudo-tty, and reading from the  
  //  pseudo-tty and writing to the screen and the logfile.
  */
  for (;;) {
    /* 
    //  Watch for users terminal and master pty to change status.
    */
    FD_ZERO(&readmask);
    FD_SET(masterPty, &readmask);
    FD_SET(0, &readmask);

    /* 
    //  Wait for something to read.
    */
    n = select(masterPty + 1, &readmask, (fd_set *) 0, (fd_set *) 0,
               (struct timeval *) 0);
    if (n < 0 && sigwinch_received == 0) {
      perror("select");
      exit(EXIT_FAILURE);
    }

    if(sigwinch_received) {
      /* pass SIGWINCH to the child */
      sigwinch_received = 0;
      signal(SIGWINCH, handle_sig_winch);
      ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&winSize);
      ioctl(masterPty, TIOCSWINSZ, (char *)&winSize);
      kill(childPid, SIGWINCH);
      continue;
    }

    /* 
    //  The user typed something... 
    //  Read it and pass it on to the pseudo-tty.
    */
    if (FD_ISSET(0, &readmask)) {
      if ((n = read(0, buf, sizeof(buf))) < 0) {
        perror("read: stdin");
        exit(EXIT_FAILURE);
      }
      if (n == 0) {
        /* 
        //  The user typed end-of-file; we're done.
        */
        finish(0);
      }
      if (write(masterPty, buf, n) != n) {
        perror("write: pty");
        exit(EXIT_FAILURE);
      }
    }

    /* 
    //  There's output on the pseudo-tty... 
    //  Read it and pass it on to the screen and the script file.
    //  Echo is on, so we see here also the users keystrokes.
    */
    if (FD_ISSET(masterPty, &readmask)) {
      /* 
      //  The process died.
      */
      if ((n = read(masterPty, buf, sizeof(buf))) <= 0) {
        finish(0);
      } else {
        dologging(buf, n);
        write(STDOUT_FILENO, buf, n);
      }
    }
  }
  exit(EXIT_SUCCESS);
}

void execShell(const char *shell, const char *shellCommands) {
  /*
  //
  //  dashShell		The basename of shell, prepended with "-".
  //  
  //  sucmd		The path to the su command.
  //
  */
  char *dashShell;
  char *sucmd = SUCMD;
  
  /* 
  //  This process will exec a shell.
  //  If rootsh was called with the -u user parameter, we will exec
  //  su user instead of a shell.
  */
  if (runAsUser) {
    shell = sucmd;
  }
  /*
  //  If rootsh was called with the -i parameter (initial login)
  //  then prepend the shell's basename with a dash,
  //  otherwise call it as an interactive shell.
  */
  if (useLoginShell) {
    dashShell = strdup(shell);
    dashShell = strrchr(dashShell, '/');
    dashShell[0] = '-';
  } else {
    dashShell = NULL;
  }
  if (runAsUser && useLoginShell && shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), "-", runAsUser, "-c", shellCommands, (char *)NULL);
  } else if (runAsUser && useLoginShell && !shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), "-", runAsUser, (char *)NULL);
  } else if (runAsUser && !useLoginShell && shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), runAsUser, "-c", shellCommands, (char *)NULL);
  } else if (runAsUser && !useLoginShell && !shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), runAsUser, (char *)NULL);
  } else if (!runAsUser && useLoginShell && shellCommands) {
    execl(shell, dashShell, "-c", shellCommands, (char *)NULL);
  } else if (!runAsUser && useLoginShell && !shellCommands) {
    execl(shell, dashShell, (char *)NULL);
  } else if (!runAsUser && !useLoginShell && shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), "-i", "-c", shellCommands, (char *)NULL);
  } else if (!runAsUser && !useLoginShell && !shellCommands) {
    execl(shell, (strrchr(shell, '/') + 1), "-i", (char *)NULL);
  } else {
    perror(shell);
  }
  perror(progName);
  exit(EXIT_FAILURE);
}

/**
 * Consume the remaining args and append them to shellCommands.
 * 
 * @return the new value of shellCommands
 */
char * consume_remaining_args(int argc, char **argv, char *shellCommands) {
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
  return shellCommands;
}

void handle_sig_winch(int sig) {

  sigwinch_received = 1;

}

/* 
//  Do some cleaning after the child exited.
//  This is also the signal handler for SIGINT and SIGQUIT.
//  Restore original tty modes.
//  Send some final words to the logging function.
*/

void finish(int sig) {
  /*
  //  sig	Either 0 if the shell exited or the number of a
  //		signal that was catched.
  //
  //  msgbuf	A buffer where a variable text will be written.
  //
  //  msglen	Counts how many characters have been written.
  //
  */
  char msgbuf[BUFSIZ];
  int msglen;

  if(isatty(0)) {
    if (tcsetattr(0, TCSANOW, &termParams) < 0) {
      perror("tcsetattr: stdin");
    }
  }
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
//  Open a logfile and initialize syslog. 
//  Remember the logfile's inode and device for later comparison.
//  Send introducing lines to the logging functions.
*/

int beginlogging(const char *shellCommands) {
  /*
  //  msgbuf		A buffer where a variable text will be written.
  //  
  //  msglen		Counts how many characters have been written.
  //  
  //  defLogFileName	The name of the logfile how it will be called,
  //			if the user did not provide his own name.
  //			Made up from username, a timestamp and the
  //			process id.
  //  
  //  now		A structure filled with the current time.
  //  
  //  sec, min, hour	Components of now.
  //  
  //  day, month, year	Components of now.
  //  
  //  statBuf		A buffer for the stat system call which contains
  //			inode and device.
  //  
  */
#ifdef LOGTOFILE
  time_t now;
  char msgbuf[BUFSIZ];
  char defLogFileName[MAXPATHLEN - 7];
  int sec, min, hour, day, month, year, msglen;
  struct stat statBuf;
#endif
#ifdef LOGTOSYSLOG
  static char sessionIdWithUid[sizeof(sessionId) + 10];
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
    snprintf(defLogFileName, (sizeof(logFileName) - 1), 
        "%s.%04d%02d%02d%02d%02d%02d.%05x", 
         userName, year,month, day, hour, min, sec, getpid());
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
            userLogFileDir, defLogFileName);
      } else {
        snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
            LOGDIR, defLogFileName);
      }
    } else {
      snprintf(logFileName, (sizeof(logFileName) - 1), "%s/%s",
          LOGDIR, defLogFileName);
    }
    /* 
    //  Open the logfile 
    */
    if ((logFile = open(logFileName, O_RDWR|O_CREAT|O_SYNC|O_CREAT|O_APPEND,
        S_IRUSR|S_IWUSR)) == -1) {
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
    //  Note the start time in the log file.
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
    //  Prepare usage of syslog with sessionid as prefix.
    */
#ifdef LOGUSERNAMETOSYSLOG
    snprintf(sessionIdWithUid, sizeof(sessionIdWithUid), "%s: %s",
        sessionId, userName);
    openlog(sessionIdWithUid, LOG_NDELAY, SYSLOGFACILITY);
#else
    openlog(sessionId, LOG_NDELAY, SYSLOGFACILITY);
#endif
    /* 
    //  Note the log file name in syslog if there is one.
    */
    if (logtofile) {
      syslog(SYSLOGFACILITY | SYSLOGPRIORITY, 
          "%s=%s,%s: logging new %ssession (%s) to %s", 
          userName, runAsUser ? runAsUser : getpwuid(getuid())->pw_name, 
          ttyname(0), isaLoginShell ? "login " : "", sessionId, logFileName);
    } else {
      syslog(SYSLOGFACILITY | SYSLOGPRIORITY, 
          "%s=%s,%s: logging new %ssession (%s)", 
          userName, runAsUser ? runAsUser : getpwuid(getuid())->pw_name, 
          ttyname(0), isaLoginShell ? "login " : "", sessionId);
    }
  }
#endif

  if(NULL != shellCommands) {
    msglen = snprintf(msgbuf, (sizeof(msgbuf) - 1),
                      "shell commands: %s\n", shellCommands);
    dologging(msgbuf, msglen);
  }
  
  return(1);
}


/*
//  Send a buffer full of output to the selected logging destinations.
//  Either to a local logfile or to the syslog server or both.
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
  /*
  //  msgbuf		A buffer where a variable text will be written.
  //  
  //  msglen		Counts how many characters have been written.
  //  
  //  closedLogFileName	After the logfile is closed, it will be renamed
  //			to this name. Normally a ".closed" will be attached
  //			but if inode and dev differ from their values at
  //			opening time, ".tampered" will be attached.
  //  
  //  now		A structure filled with the current time.
  //  
  //  sec, min, hour	Components of now.
  //  
  //  day, month, year	Components of now.
  //  
  //  statBuf		A buffer for the stat system call which contains
  //			inode and device.
  //  
  */
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
    //  From here on, a filled message buffer means an error has occurred.
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
        if(unlink(logFileName)) {
          /* must have been a directory, try and delete it */
          rmdir(logFileName);
        }
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
      if (! recoverfile(logFile, strcat(logFileName, ".tampered"))) {
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
//  filehandle to another file.
*/

int recoverfile(int ohandle, char *recoverFileName) {
  /*
  //  msgbuf		A buffer where a variable text will be written.
  //  
  //  msglen		Counts how many characters have been written.
  //  
  //  fd		The file descriptor of the recover file.
  //
  */
  char msgbuf[BUFSIZ];
  int fd, msglen;
  if ((fd = forceopen(recoverFileName)) != -1) {
    lseek(ohandle, 0, SEEK_SET);
    while ((msglen = read(ohandle, (void *)msgbuf, BUFSIZ)) > 0) {
      if (write(fd, (void *)msgbuf, msglen) != msglen) {
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
  /*
  //  msgbuf		A buffer where a variable text will be written.
  //  
  //  msglen		Counts how many characters have been written.
  //  
  //  fd		The file descriptor of the file which will be
  //			hopefully opened under all circumstances.
  //
  //  tries		A counter for the unsuccessful attempts to open
  //			the file. 
  */
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
//  Find out the username of the calling user.
*/

char *setupusername() {
  /*
  //  userName	A pointer to allocated memory containing the username.
  //  
  //  ttybuf	A structure containing file information about 
  //		the controlling terminal of this process.
  //  
  */
  char *userName = NULL;
  struct stat ttybuf;
  struct passwd *passwd_entry;
    
  passwd_entry = getpwuid(geteuid());
  userName = passwd_entry->pw_name;

  if(userName == NULL) {
  /*if((userName = getlogin()) == NULL) {*/
  /*if((cuserid(userName)) == NULL) {*/
  
    /* 
    //  HP-UX returns NULL here so we take the controlling terminal's owner.
    */
    if(ttyname(0) != NULL) {
      if (stat(ttyname(0), &ttybuf) == 0) {
        if ((userName = getpwuid(ttybuf.st_uid)->pw_name) == NULL) {
          fprintf(stderr, "i don\'t know who you are\n");
        }
      }
    } else {
      /* 
      //  Rootsh must be run interactively.
      */
      fprintf(stderr, "i don\'t know who you are\n");
    }
  }
  return userName;
}


/* 
//  Find out which shell to use. return the pathname of the shell 
//  or NULL if an error occurred.
//  If this executable has been called by login, it would also become
//  the command interpreter for the just logged in user. This would
//  result in a recursive invocation of rootsh. In this case we
//  must find a replacement in form of a default command interpreter.
*/ 

char *setupshell() {
  /*
  //  isvalid	A flag indicating a shell found in /etc/shells.
  //  
  //  shell	The path to the shell of the calling user.
  //  
  //  shellenv	A string containing "SHELL=<path_to_shell>" which will
  //		be added to the environment.
  */
  int isvalid;
  char *shell, *shellenv;
  char *validshell;
  
  /* 
  //  Try to get the users current shell with two methods.
  */
  if ((shell = getenv("SHELL")) == NULL) {
    shell = getpwnam(userName)->pw_shell;
  }
  if (shell == NULL) {
    fprintf(stderr, "could not determine a valid shell\n");
  } else {
#if HAS_ETC_SHELLS
    /*
    //  Compare it to the allowed shells in /etc/shells.
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
    //  Do not allow invalid shells.
    */
    if (isvalid == 0) {
      fprintf(stderr, "%s is not in /etc/shells\n", shell);
      return(NULL);
    }
    /*
    //  If SHELL is rootsh and argv[0] is -rootsh or rootsh we were probably
    //  started as a commandline interpreter found in /etc/passwd.
    //  We cannot execute ourself again. Find a real shell.
    */
    if( (*progName == '-' && strcmp(basename(shell), progName + 1) == 0 )
         || strcmp(basename(shell), progName) == 0
         ) {
      char *dshell = defaultshell();
      shell = calloc(sizeof(char), strlen(dshell) + 1);
      strcpy(shell, dshell);
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
//  If a username was given on the command line via -u 
//  See, if it has an acceptable length (yes, some have 64 character usernames)
//  See, if it exists. 
//  get the uid. 
//  Clean up the environment.
//  If not, forget this and run as root.
*/

int setupusermode(void) {
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
    if (getpwnam(runAsUser) == NULL) {
      fprintf(stderr, "user %s does not exist\n", runAsUser);
      return(0);
    } else {
      saveenv("TERM");
      clearenv();
      restoreenv();
      return (1);
    }
  }
}


/*
//  If rootsh is used as user command interpreter in /etc/passwd, we have
//  to find an alternative shell which will handle the users commands.
*/

char *defaultshell(void) {
  /*
  //  defaultshell	A static memory area containing the path of the 
  //			default shell to use.
  */
  char *defaultshell = DEFAULTSHELL;
  if (strlen(defaultshell) > 0) {
    return defaultshell;
  } else {
    /*
    //  Maybe there are other possibilities to find a suitable shell.
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
  /*
  //  senv	A pointer to a static area holding the saved
  //		environment variables. It's an equivalent to
  //		the **environ pointer.
  //  
  //  senvp	A pointer to the last entry in senv.
  //  
  */
  static char **senv = NULL;
  static char **senvp;
  if (name == NULL) {
    return senv;
  } else {
    /*
    //  Only save existing variables.
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
      //  already existing name=value entry.
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
  /*
  //  senv	A pointer to the saved environment.
  */
  char **senv;
  senv = saveenv(NULL);
  while (*senv != NULL) {
    putenv(*senv++);
  }
}


/*
//  Clear the environment completely.
*/

#ifndef HAVE_CLEARENV
int clearenv(void) {
  free(environ);
  if ((environ = malloc(sizeof(char *))) == NULL) {
    return(-1);
  } else {
    return(0);
  }
}
#endif


#ifndef HAVE_FORKPTY
/* 
//  Emulation of the BSD function forkpty.
*/
#ifndef MASTERPTYDEV
#  error you need to specify a master pty device
#endif
pid_t forkpty(int *amaster,  char  *name,  struct  termios *termp, struct winsize *winp) {
  /*
  //  amaster		A pointer to the master pty's file descriptor which
  //			will be set here.
  //  
  //  name		If name is NULL, the name of the slave pty will be
  //			returned in name.
  //  
  //  termp		If termp is not NULL, the ter minal parameters
  //			of the slave will be set to the values in termp.
  //  
  //  winsize		If winp is not NULL, the window size of the  slave
  //			will be set to the values in winp.
  //  
  //  currentterm	A structure filled with the characteristics of
  //			the current controlling terminal.
  //  
  //  currentwinsize	A structure filled with size characteristics of
  //			the current controlling terminal.
  //  
  //  pid		The process id of the forked child process.
  //  
  //  master		The file descriptor of the master pty.
  //  
  //  slave		The file descriptor of the slave pty.
  //  
  //  slavename		The file name of the slave pty.
  //  
  */
  struct termios currentterm;
  struct winsize currentwinsize;
  pid_t pid;
  int master, slave;
  char *slavename;

  /* 
  //  Get current settings if termp was not provided by the caller.
  */
  if (termp == NULL) {
    tcgetattr(STDIN_FILENO, &currentterm);
    termp = &currentterm;
  }

  /* 
  //  Same for window size.
  */
  if (winp == NULL) {
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&currentwinsize);
    winp->ws_row = currentwinsize.ws_row;
    winp->ws_col = currentwinsize.ws_col;
    winp->ws_xpixel = currentwinsize.ws_xpixel;
    winp->ws_ypixel = currentwinsize.ws_ypixel;
  }

  /*
  //  Get a master pseudo-tty.
  */
  if ((master = open(MASTERPTYDEV, O_RDWR)) < 0) {
    perror(MASTERPTYDEV);
    return(-1);
  }

  /*
  //  Set the permissions on the slave pty.
  */
  if (grantpt(master) < 0) {
    perror("grantpt");
    close(master);
    return(-1);
  }

  /*
  //  Unlock the slave pty.
  */
  if (unlockpt(master) < 0) {
    perror("unlockpt");
    close(master);
    return(-1);
  }

  /*
  //  Start a child process.
  */
  if ((pid = fork()) < 0) {
    perror("fork in forkpty");
    close(master);
    return(-1);
  }

  /*
  //  The child process will open the slave, which will become
  //  its controlling terminal.
  */
  if (pid == 0) {
    /*
    //  Get rid of our current controlling terminal.
    */
    setsid();

    /*
    //  Get the name of the slave pseudo tty.
    */
    if ((slavename = ptsname(master)) == NULL) {
      perror("ptsname");
      close(master);
      return(-1);
    }

    /* 
    //  Open the slave pseudo tty.
    */
    if ((slave = open(slavename, O_RDWR)) < 0) {
      perror(slavename);
      close(master);
      return(-1);
    }

#ifndef AIX_COMPAT
    /*
    //  Push the pseudo-terminal emulation module.
    */
    if (ioctl(slave, I_PUSH, "ptem") < 0) {
      perror("ioctl: ptem");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  Push the terminal line discipline module.
    */
    if (ioctl(slave, I_PUSH, "ldterm") < 0) {
      perror("ioctl: ldterm");
      close(master);
      close(slave);
      return(-1);
    }

#ifdef HAVE_TTCOMPAT
    /*
    //  Push the compatibility for older ioctl calls module (solaris).
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
    //  Copy the caller's terminal modes to the slave pty.
    */
    if (tcsetattr(slave, TCSANOW, termp) < 0) {
      perror("tcsetattr: slave pty");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  Set the slave pty window size to the caller's size.
    */
    if (ioctl(slave, TIOCSWINSZ, winp) < 0) {
      perror("ioctl: slave winsz");
      close(master);
      close(slave);
      return(-1);
    }

    /*
    //  Close the logfile and the master pty.
    //  No need for these in the slave process.
    */
    close(master);
    if (logtofile) {
      close(logFile);
    }
    if (logtosyslog) {
      closelog();
    }
    /*
    //  Set the slave to be our standard input, output and error output.
    //  Then get rid of the original file descriptor.
    */
    dup2(slave, 0);
    dup2(slave, 1);
    dup2(slave, 2);
    close(slave);
    /*
    //  If the caller wants it, give him back the slave pty's name.
    */
    if (name != NULL) strcpy(name, slavename);
    return(0); 
  } else {
    /*
    //  Return the slave pty device name if caller wishes so.
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
    //  Return the file descriptor for communicating with the process
    //  to the caller.
    */
    *amaster = master; 
    return(pid);      
  }
}
#endif

char **build_scp_args( char *str, size_t reserve ) {

  wordexp_t       result;
  int             retc;

  result.we_offs = reserve;
  if ( (retc = wordexp(str, &result, WRDE_NOCMD|WRDE_DOOFFS)) ){
    switch( retc ){
    case WRDE_BADCHAR:
    case WRDE_CMDSUB:
      fprintf(stderr, "%s: bad characters in arguments\n",
	      progName);
      break;
    case WRDE_NOSPACE:
      fprintf(stderr, "%s: wordexp() allocation failed\n",
	      progName);
      break;
    case WRDE_BADVAL:
      fprintf(stderr, "%s: wordexp() bad value\n",
	      progName);
      break;
    case WRDE_SYNTAX:
      fprintf(stderr, "%s: wordexp() bad syntax\n",
	      progName);
      break;
#ifdef WRDE_NOSYS
    case WRDE_NOSYS:
      fprintf(stderr, "%s: wordexp() not supported\n",
	      progName);
      break;
#endif
    default:
      fprintf(stderr, "%s: error expanding arguments\n",
	      progName);
    }
    exit(1);
  }
  return result.we_wordv;
}

/*
//  Print version number and capabilities of this binary.
*/

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


/*
//  Print available command line switches.
*/

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
