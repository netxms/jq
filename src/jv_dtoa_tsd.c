#include <stdlib.h>
#include <stdio.h>

#include "jv_thread.h"
#include "jv_dtoa_tsd.h"
#include "jv_dtoa.h"
#include "jv_alloc.h"

#ifdef _WIN32
static INIT_ONCE dtoa_ctx_once = INIT_ONCE_STATIC_INIT;
static DWORD dtoa_ctx_key;
#else
static pthread_once_t dtoa_ctx_once = PTHREAD_ONCE_INIT;
static pthread_key_t dtoa_ctx_key;
#endif

static void tsd_dtoa_ctx_dtor(void *ctx) {
  if (ctx) {
    jvp_dtoa_context_free((struct dtoa_context *)ctx);
    jv_mem_free(ctx);
  }
}

#ifdef WIN32

static void jv_tsd_dtoa_ctx_fini(void)
{
   struct dtoa_context *ctx = (struct dtoa_context*)TlsGetValue(dtoa_ctx_key);
   tsd_dtoa_ctx_dtor(ctx);
   TlsSetValue(dtoa_ctx_key, NULL);
}

static BOOL WINAPI jv_tsd_dtoa_ctx_init(PINIT_ONCE initOnce, PVOID parameter, PVOID *context)
{
   if ((dtoa_ctx_key = TlsAlloc()) == TLS_OUT_OF_INDEXES)
   {
      fprintf(stderr, "error: cannot create thread specific key");
      abort();
   }
   atexit(jv_tsd_dtoa_ctx_fini);
   return TRUE;
}

#else

static
void jv_tsd_dtoa_ctx_fini() {
  struct dtoa_context *ctx = pthread_getspecific(dtoa_ctx_key);
  tsd_dtoa_ctx_dtor(ctx);
  pthread_setspecific(dtoa_ctx_key, NULL);
}

static
void jv_tsd_dtoa_ctx_init() {
  if (pthread_key_create(&dtoa_ctx_key, tsd_dtoa_ctx_dtor) != 0) {
    fprintf(stderr, "error: cannot create thread specific key");
    abort();
  }
  atexit(jv_tsd_dtoa_ctx_fini);
}

#endif

struct dtoa_context *tsd_dtoa_context_get() {
#ifdef _WIN32
   InitOnceExecuteOnce(&dtoa_ctx_once, jv_tsd_dtoa_ctx_init, NULL, NULL);
   struct dtoa_context *ctx = (struct dtoa_context*)TlsGetValue(dtoa_ctx_key);
#else
  pthread_once(&dtoa_ctx_once, jv_tsd_dtoa_ctx_init); // cannot fail
  struct dtoa_context *ctx = (struct dtoa_context*)pthread_getspecific(dtoa_ctx_key);
#endif
  if (!ctx) {
    ctx = jv_mem_alloc(sizeof(struct dtoa_context));
    jvp_dtoa_context_init(ctx);
#ifdef _WIN32
    if (!TlsSetValue(dtoa_ctx_key, ctx)) {
#else
    if (pthread_setspecific(dtoa_ctx_key, ctx) != 0) {
#endif
      fprintf(stderr, "error: cannot set thread specific data");
      abort();
    }
  }
  return ctx;
}
