#ifndef __H_LOG
#define __H_LOG

void log_init(void);
void log_close(void);
void log_println(const char * const msg, ...);

#endif
