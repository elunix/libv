#include <libved.h>
#include <libved+.h>

#ifndef STR_FMT
#define STR_FMT(fmt_, ...)                                            \
({                                                                    \
  char buf_[MAXLEN_LINE];                                             \
  snprintf (buf_, MAXLEN_LINE, fmt_, __VA_ARGS__);                    \
  buf_;                                                               \
})
#endif
#ifndef debug_append
#define debug_append(fmt, ...)                            \
({                                                        \
  char *file_ = STR_FMT ("/tmp/%s.debug", __func__);      \
  FILE *fp_ = fopen (file_, "a+");                        \
  if (fp_ isnot NULL) {                                   \
    fprintf (fp_, (fmt), ## __VA_ARGS__);                 \
    fclose (fp_);                                         \
  }                                                       \
})
#endif
typedef struct vtach_t vtach_t;
typedef struct vtach_prop vtach_prop;

typedef int (*PtyOnExecChild_cb) (vtach_t *, int, char **);
typedef void (*EdToplineMethod) (ed_t *, buf_t *);

typedef struct vtach_set_self {
  void (*exec_child_cb) (vtach_t *, PtyOnExecChild_cb);
  vwm_t *(*vwm) (vtach_t *);
} vtach_set_self;

typedef struct vtach_get_self {
  vwm_t *(*vwm) (vtach_t *);
} vtach_get_self;

typedef struct vtach_init_self {
  int
    (*pty) (vtach_t *, char *);

  vwm_term
    *(*term) (vtach_t *, int *, int *);

} vtach_init_self;

typedef struct vtach_pty_self {
  int (*main) (vtach_t *this, int, char **);
} vtach_pty_self;

typedef struct vtach_tty_self {
  int (*main) (vtach_t *this);
} vtach_tty_self;

typedef struct vtach_self {
  vtach_set_self  set;
  vtach_get_self  get;
  vtach_pty_self  pty;
  vtach_tty_self  tty;
  vtach_init_self init;
} vtach_self;

typedef struct vtach_t {
  vtach_self self;
  vtach_prop *prop;
} vtach_t;

public vtach_t *__init_vtach__ (vwm_t *);
public void __deinit_vtach__ (vtach_t **);
