/*
 * Copyright 2022 Yonggang Luo
 * SPDX-License-Identifier: MIT
 */

#include "u_call_once.h"

#include <windows.h>

struct util_call_once_context_t
{
   const void *data;
   util_call_once_data_func func;
};

#if 0 /* WINE */

static thread_local struct util_call_once_context_t call_once_context;

static void
util_call_once_data_slow_once(void)
{
   struct util_call_once_context_t *once_context = &call_once_context;
   once_context->func(once_context->data);
}

void
util_call_once_data_slow(once_flag *once, util_call_once_data_func func, const void *data)
{
   struct util_call_once_context_t *once_context = &call_once_context;
   once_context->data = data;
   once_context->func = func;
   call_once(once, util_call_once_data_slow_once);
}

#else /* WINE */

static BOOL CALLBACK util_call_once_data_slow_once(PINIT_ONCE once, void *param, void **context)
{
   struct util_call_once_context_t *once_context = param;
   once_context->func(once_context->data);
   return TRUE;
}

void
util_call_once_data_slow(once_flag *once, util_call_once_data_func func, const void *data)
{
   struct util_call_once_context_t once_context;
   once_context.data = data;
   once_context.func = func;
   InitOnceExecuteOnce((PINIT_ONCE)once, util_call_once_data_slow_once, &once_context, NULL);
}

#endif /* WINE */
