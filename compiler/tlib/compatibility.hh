/************************************************************************
 ************************************************************************
    FAUST compiler
	Copyright (C) 2003-2004 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/
 
#ifndef __COMPATIBILITY__
#define __COMPATIBILITY__

#define LLVM_BUILD (defined(LLVM_30) || defined(LLVM_31) || defined(LLVM_32) || defined(LLVM_33) || defined(LLVM_34))

#ifdef _WIN32
#include <time.h>
#include <assert.h>

#define PATH_MAX MAX_PATH

//#define int64_t __int64
#define YY_NO_UNISTD_H 1

struct timezone 
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

#define alarm(x)
#define strdup _strdup
#define isatty _isatty
#define fileno _fileno
#define snprintf _snprintf
//double  rint(double nr);
int		gettimeofday(struct timeval *tv, struct timezone *tz);
bool	chdir(const char* path);
int		mkdir(const char* path, unsigned int attribute);
char*	getcwd(char* str, unsigned int size);
int		isatty(int file);
void	getFaustPathname(char* str, unsigned int size);
void	getFaustPathname(char* str, unsigned int size);
char*   realpath(const char *path, char resolved_path[MAX_PATH]);

#ifdef  NDEBUG
#undef assert
#define assert(_Expression) do { bool bTest = (_Expression) != 0; } while (0)
#endif

//#define sprintf sprintf_s
#define snprintf _snprintf
//#define rintf(x) floor((x)+(((x) < 0 ) ? -0.5f :0.5f))
#define FAUST_PATH_MAX 1024

#if !defined(__MINGW32__)
#if (_MSC_VER<=1700)
	double	remainder(double numerator, double denominator);
#endif
	#define S_IRWXU 0
#endif

#define S_IRWXG 0
#define S_IROTH 0
#define S_IXOTH 0
#define DIRSEP '\\'

#undef min
#undef max

#else

#include <unistd.h>
#define DIRSEP '/'
#define FAUST_PATH_MAX 1024

void getFaustPathname(char* str, unsigned int size);

#if defined(WIN32) && ! defined(__MINGW32__)
/* missing on Windows : see http://bugs.mysql.com/bug.php?id=15936 */
inline double rint(double nr)
{
    double f = floor(nr);
    double c = ceil(nr);
    return (((c -nr) >= (nr - f)) ? f : c);
}
#endif

#endif

#endif
