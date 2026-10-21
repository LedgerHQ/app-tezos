/* Host stand-in for the Ledger SDK <os.h>, for unit tests only.

   Provides the few symbols app/src/exception.h and app/src/ui/ui_strings.c
   expect from it. The real FUNC_ENTER/FUNC_LEAVE and MIN live in
   app/src/utils.h and app/src/parser/compat.h, which drag in the SDK. */

#pragma once

#include <stdint.h>

typedef uint32_t cx_err_t;
#define CX_OK 0u

#define FUNC_ENTER(_args)
#define FUNC_LEAVE()

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
