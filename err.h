#ifndef _ERR_
#define _ERR_

/* Wypisuje informacjÄ o bĹÄdnym zakoĹczeniu funkcji systemowej
i koĹczy dziaĹanie programu. */
extern void syserr(const char *fmt, ...);

/* Wypisuje informacjÄ o bĹÄdzie i koĹczy dziaĹanie programu. */
extern void fatal(const char *fmt, ...);

#endif
