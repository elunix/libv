#ifndef VWM_H
#define VWM_H

#ifndef public
#define public __attribute__((visibility ("default")))
#endif

#ifndef private
#define private __attribute__((visibility ("hidden")))
#endif

typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef signed int utf8;

#ifndef MAX_FRAMES
#define MAX_FRAMES 3
#endif

#ifdef WIN_OPTS_MAX_FRAMES
#undef WIN_OPTS_MAX_FRAMES
#endif

#define WIN_OPTS_MAX_FRAMES 6

#ifndef MAX_ARGS
#define MAX_ARGS    256
#endif

#ifndef MAXLEN_COMMAND
#define MAXLEN_COMMAND 256
#endif

#ifndef MAXLEN_LINE
#define MAXLEN_LINE 4096
#endif

#ifndef MAX_CHAR_LEN
#define MAX_CHAR_LEN 4
#endif

#ifdef BUFIZE
#undef BUFSIZE
#endif

#define BUFSIZE     4096

#define VWM_OBJECT   0
#define VWMED_OBJECT 1
#define VTACH_OBJECT 2
#define V_OBJECT     3
#define U_OBJECT     4
#define NUM_OBJECTS  U_OBJECT + 1

#define DOWN_POS    -1
#define NEXT_POS    -2
#define LAST_POS    -3
#define PREV_POS    -4
#define UP_POS      -5

#define VWM_DONE           (1 << 0)
#define VWM_IGNORE_FOR_NOW (1 << 1)
#define VWM_QUIT           (1 << 2) // this coresponds to EXIT_THIS

#define VFRAME_CLEAR_VIDEO_MEM   (1 << 0)
#define VFRAME_CLEAR_LOG         (1 << 1)
#define VFRAME_ESC_PROCESS_DIGIT (1 << 2)

#ifndef DRAW
#define DRAW        1
#endif

#ifndef DONOT_DRAW
#define DONOT_DRAW  0
#endif

typedef struct vwm_prop vwm_prop;
typedef struct vwm_term vwm_term;
typedef struct vwm_win vwm_win;
typedef struct vwm_frame vwm_frame;
typedef struct vwm_t vwm_t;

typedef void (*FrameProcessOutput_cb) (vwm_frame *, char *, int);
typedef void (*FrameUnimplemented_cb) (vwm_frame *, const char *, int, int);
typedef void (*VwmAtExit_cb) (vwm_t *);
typedef int  (*VwmOnTab_cb) (vwm_t *, vwm_win *, vwm_frame *, void *);
typedef int  (*VwmRLine_cb) (vwm_t *, vwm_win *, vwm_frame *, void *);
typedef int  (*VwmEditFile_cb) (vwm_t *, vwm_frame *, char *, void *);
typedef int  (*FrameAtFork_cb) (vwm_frame *, vwm_t *, vwm_win *);

struct vwm_term {
  struct termios
    orig_mode,
    raw_mode;

  char
    mode,
    *name;

  int
    lines,
    columns,
    out_fd,
    in_fd;
};

typedef struct frame_opts {
  char
    **argv,
    *command,
    *logfile;

  int
    fd,
    fork,
    argc,
    at_frame,
    num_rows,
    num_cols,
    first_row,
    first_col,
    create_fd,
    enable_log,
    remove_log,
    is_visible;

  pid_t pid;

  FrameProcessOutput_cb process_output_cb;
  FrameAtFork_cb        at_fork_cb;

  vwm_win *parent;
} frame_opts;

#define FrameOpts(...) (frame_opts) { \
  .argv = NULL,                       \
  .argc = 0,                          \
  .fork = 1,                          \
  .command = NULL,                    \
  .logfile = NULL,                    \
  .fd = -1,                           \
  .pid = -1,                          \
  .num_rows = -1,                     \
  .num_cols = -1,                     \
  .at_frame = -1,                     \
  .first_row = -1,                    \
  .first_col = -1,                    \
  .create_fd = 0,                     \
  .enable_log = 0,                    \
  .remove_log = 1,                    \
  .is_visible = 1,                    \
  .process_output_cb = NULL,          \
  .at_fork_cb = NULL,                 \
  .parent = NULL,                     \
  __VA_ARGS__ }

typedef struct win_opts {
  int
    draw,
    focus,
    num_rows,
    num_cols,
    first_row,
    first_col,
    num_frames,
    max_frames;

  frame_opts frame_opts[WIN_OPTS_MAX_FRAMES];
} win_opts;

#define WinOpts(...) (win_opts) {  \
  .num_rows = 24,                  \
  .num_cols = 78,                  \
  .focus = 0,                      \
  .first_row = 1,                  \
  .first_col = 1,                  \
  .num_frames = 1,                 \
  .max_frames = 3,                 \
  .draw = DONOT_DRAW,              \
  .frame_opts[0] = FrameOpts(),    \
  .frame_opts[1] = FrameOpts(),    \
  .frame_opts[2] = FrameOpts(),    \
  .frame_opts[3] = FrameOpts(),    \
  .frame_opts[4] = FrameOpts(),    \
  .frame_opts[5] = FrameOpts(),    \
  __VA_ARGS__ }

typedef struct vframe_info {
  char *logfile;

  int
    at_frame,
    num_rows,
    last_row,
    first_row,
    is_current,
    is_visible;

  pid_t pid;

  char *argv[MAX_ARGS];
} vframe_info;

typedef struct vwin_info {
  char *name;

  int
    num_rows,
    num_cols,
    num_frames,
    is_current,
    cur_frame_idx,
    num_visible_frames;

  vframe_info **frames;
} vwin_info;

typedef struct vwm_info {
  char
    *sequences_fname,
    *unimplemented_fname;

  int
    num_win,
    cur_win_idx;

  pid_t pid;
  vwin_info **wins;
} vwm_info;

typedef struct vwm_term_screen_self {
  void
    (*save)    (vwm_term *),
    (*clear)   (vwm_term *),
    (*restore) (vwm_term *);
} vwm_term_screen_self;

typedef struct vwm_term_self {
  vwm_term_screen_self screen;

  void
    (*release)   (vwm_term **),
    (*init_size) (vwm_term *, int *, int *);

  int
    (*raw_mode)  (vwm_term *),
    (*orig_mode) (vwm_term *),
    (*sane_mode) (vwm_term *);

} vwm_term_self;

typedef struct vwm_frame_get_self {
  char
    **(*argv) (vwm_frame *),
     *(*logfile) (vwm_frame *);

  int
    (*fd)  (vwm_frame *),
    (*argc) (vwm_frame *),
    (*logfd) (vwm_frame *),
    (*num_rows) (vwm_frame *),
    (*visibility) (vwm_frame *);

  pid_t (*pid) (vwm_frame *);

  vwm_win *(*parent) (vwm_frame *);
  vwm_t *(*root) (vwm_frame *);

  vframe_info *(*info) (vwm_frame *);
} vwm_frame_get_self;

typedef struct vwm_frame_set_self {
  void
    (*fd) (vwm_frame *, int),
    (*argv) (vwm_frame *, int, char **),
    (*command) (vwm_frame *, char *),
    (*visibility) (vwm_frame *, int),
    (*unimplemented_cb) (vwm_frame *, FrameUnimplemented_cb);

  int (*log) (vwm_frame *, char *,  int);

  FrameProcessOutput_cb (*process_output_cb) (vwm_frame *, FrameProcessOutput_cb);
  FrameAtFork_cb (*at_fork_cb) (vwm_frame *, FrameAtFork_cb);

} vwm_frame_set_self;

typedef struct vwm_frame_self {
  vwm_frame_get_self get;
  vwm_frame_set_self set;

  void
    (*reset) (vwm_frame *),
    (*clear) (vwm_frame *, int),
    (*on_resize) (vwm_frame *, int, int),
    (*reopen_log) (vwm_frame *),
    (*release_log) (vwm_frame *),
    (*release_info) (vframe_info *),
    (*release_argv) (vwm_frame *),
    (*process_output) (vwm_frame *, char *, int);

  int
    (*edit_log) (vwm_frame *),
    (*check_pid) (vwm_frame *),
    (*kill_proc) (vwm_frame *),
    (*create_fd) (vwm_frame *);

  pid_t (*fork) (vwm_frame *);
} vwm_frame_self;

typedef struct vwm_win_set_self {
  void
    (*frame) (vwm_win *, vwm_frame *),
    (*frame_by_idx) (vwm_win *, int);

  int (*separators) (vwm_win *, int);

  vwm_frame
    *(*current_at) (vwm_win *, int),
    *(*frame_as_current) (vwm_win *, vwm_frame *);
} vwm_win_set_self;

typedef struct vwm_win_get_self {
  int
    (*frame_idx) (vwm_win *, vwm_frame *),
    (*num_frames) (vwm_win *),
    (*num_visible_frames) (vwm_win *);

  vwm_frame
    *(*frame_at) (vwm_win *, int),
    *(*current_frame) (vwm_win *);

  vwin_info *(*info) (vwm_win *);
} vwm_win_get_self;

typedef struct vwm_win_frame_self {
  void
    (*set_size)      (vwm_win *, vwm_frame *, int, int),
    (*increase_size) (vwm_win *, vwm_frame *, int, int),
    (*decrease_size) (vwm_win *, vwm_frame *, int, int);

  vwm_frame *(*change) (vwm_win *, vwm_frame *, int, int);
} vwm_win_frame_self;

typedef struct vwm_win_self {
  vwm_win_set_self set;
  vwm_win_get_self get;
  vwm_win_frame_self frame;

  void
    (*draw) (vwm_win *),
    (*on_resize) (vwm_win *, int),
    (*release_info) (vwin_info *),
    (*release_frame_at) (vwm_win *, int);

  int
    (*frame_rows) (vwm_win *, int, int *),
    (*append_frame) (vwm_win *, vwm_frame *),
    (*delete_frame) (vwm_win *, vwm_frame *, int),
    (*insert_frame_at) (vwm_win *, vwm_frame *, int);

  vwm_frame
    *(*init_frame) (vwm_win *, frame_opts),
    *(*new_frame) (vwm_win *, frame_opts),
    *(*add_frame) (vwm_win *, int, char **, int),
    *(*pop_frame_at) (vwm_win *, int);
} vwm_win_self;

typedef struct vwm_get_self {
  void
    *(*object) (vwm_t *, int);

  int
    (*state) (vwm_t *),
    (*lines) (vwm_t *),
    (*columns) (vwm_t *),
    (*win_idx) (vwm_t *, vwm_win *),
    (*num_wins) (vwm_t *);

  char
    *(*shell) (vwm_t *),
    *(*editor) (vwm_t *),
    *(*tmpdir) (vwm_t *),
     (*mode_key) (vwm_t *),
    *(*default_app) (vwm_t *);

  vwm_info *(*info) (vwm_t *);
  vwm_win
     *(*win_at) (vwm_t *, int),
     *(*current_win) (vwm_t *);

  vwm_term *(*term) (vwm_t *);
  vwm_frame *(*current_frame) (vwm_t *);
} vwm_get_self;

typedef struct vwm_unset_debug_self {
  void
    (*sequences) (vwm_t *),
    (*unimplemented) (vwm_t *);
} vwm_unset_debug_self;

typedef struct vwm_unset_self {
  vwm_unset_debug_self debug;
  void (*tmpdir) (vwm_t *);
} vwm_unset_self;

typedef struct vwm_set_debug_self {
  void
    (*sequences) (vwm_t *, char *),
    (*unimplemented) (vwm_t *, char *);
} vwm_set_debug_self;

typedef struct vwm_set_self {
  vwm_set_debug_self debug;

  void
    (*size)   (vwm_t *, int, int, int),
    (*term)   (vwm_t *, vwm_term *),
    (*state)  (vwm_t *, int),
    (*shell)  (vwm_t *, char *),
    (*editor) (vwm_t *, char *),
    (*object) (vwm_t *, void *, int),
    (*mode_key) (vwm_t *, char),
    (*default_app) (vwm_t *, char *),
    (*rline_cb) (vwm_t *, VwmRLine_cb),
    (*on_tab_cb) (vwm_t *, VwmOnTab_cb),
    (*at_exit_cb) (vwm_t *, VwmAtExit_cb),
    (*edit_file_cb) (vwm_t *, VwmEditFile_cb);

  int (*tmpdir) (vwm_t *, char *, size_t);

  vwm_win *(*current_at) (vwm_t *, int);

} vwm_set_self;

typedef struct vwm_new_self {
  vwm_win *(*win) (vwm_t *, char *, win_opts);
  vwm_term *(*term) (vwm_t *);
} vwm_new_self;

typedef struct vwm_self {
   vwm_new_self new;
   vwm_get_self get;
   vwm_set_self set;
   vwm_unset_self unset;

  void
    (*change_win) (vwm_t *, vwm_win *, int, int),
    (*release_win) (vwm_t *, vwm_win *),
    (*release_info) (vwm_t *, vwm_info **);

  int
    (*main) (vwm_t *),
    (*spawn) (vwm_t *, char **),
    (*append_win) (vwm_t *, vwm_win *),
    (*process_input) (vwm_t *, vwm_win *, vwm_frame *, char *);

  utf8 (*getkey) (vwm_t *, int);

  vwm_win *(*pop_win_at) (vwm_t *, int);
} vwm_self;

struct vwm_t {
  vwm_self self;
  vwm_prop *prop;

  vwm_win_self win;
  vwm_frame_self frame;
  vwm_term_self term;
};

public vwm_t *__init_vwm__ (void);
public void __deinit_vwm__ (vwm_t **);

#endif
