/* Force-include tren x64: UCL 1.01 khong biet LLP64 (long=4, pointer=8). */
#pragma once

#if defined(_M_X64) || defined(_M_AMD64) || defined(_WIN64)

#ifndef SIZEOF_SIZE_T
#define SIZEOF_SIZE_T 8
#endif
#ifndef SIZEOF_CHAR_P
#define SIZEOF_CHAR_P 8
#endif
#ifndef SIZEOF_PTRDIFF_T
#define SIZEOF_PTRDIFF_T 8
#endif

/* ucl_ptr.h se nhan typedef nay thay vi unsigned long (4 byte). */
#if !defined(__UCL_HAVE_PTR_T) && !defined(ucl_ptr_t)
typedef unsigned long long ucl_ptr_t;
typedef long long ucl_sptr_t;
#define __UCL_HAVE_PTR_T 1
#endif

/* UCL_UTYPE_MAX dung 1ul se UB khi b=8; dinh nghia lai sau ucl_conf.h bang wrap. */
#endif
