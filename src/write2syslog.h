#include <stdbool.h>

/**
 *  takes a variable sized string
 *  breaks the string into pieces (lines) separated by \r
 *  cleans the lines from escape sequences and writes it to syslog
 *  keeps remainings (not closed by a newline) in a static area
 * @param oBuffer what to write
 * @param oCharCount how large oBuffer is
 * @param useLinecnt if true, then output line count as a 3 digit counter
 * @param facility syslog facility
 * @param priority syslog priority
 */
void write2syslog(const char *oBuffer, size_t oCharCount, bool const useLinecnt,
                  int const facility, int const priority);
