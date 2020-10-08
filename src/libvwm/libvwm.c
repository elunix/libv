#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pty.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>

#include <errno.h>

#include <libvwm.h>
#include "__libvwm.h"

static vwm_t *VWM;
static VtString_T VtString;

#ifndef SHELL
#define SHELL "zsh"
#endif

#ifndef EDITOR
#define EDITOR "vim"
#endif

#ifndef DEFAULT_APP
#define DEFAULT_APP SHELL
#endif

#ifndef TMPDIR
#define TMPDIR "/tmp"
#endif

#ifndef MODE_KEY
#define MODE_KEY  CTRL ('\\')
#endif

#ifndef CTRL
#define CTRL(X) (X & 037)
#endif

#ifndef DIR_SEP
#define DIR_SEP '/'
#endif

#ifndef IS_DIRSEP
#define IS_DIRSEP(c_) (c_ == DIR_SEP)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096  /* bytes in a path name */
#endif

#ifndef MAX_FRAMES
#define MAX_FRAMES 3
#endif

#define MIN_ROWS 2
#define MAX_CHAR_LEN 4
#define MAX_ARGS    256
#define MAX_TTYNAME 1024
#define MAX_PARAMS  12
#define MAX_SEQ_LEN 16

#define BUFSIZE     4096
#define TABWIDTH    8

#define ISDIGIT(c_)     ('0' <= (c_) && (c_) <= '9')
#define IS_UTF8(c_)     (((c_) & 0xC0) == 0x80)
#define isnotutf8(c_)   (IS_UTF8 (c_) == 0)
#define isnotatty(fd_)  (0 == isatty ((fd_)))

#define STR_FMT(len_, fmt_, ...)                                      \
({                                                                    \
  char buf_[len_];                                                    \
  snprintf (buf_, len_, fmt_, __VA_ARGS__);                           \
  buf_;                                                               \
})

#define NORMAL          0x00
#define BOLD            0x01
#define UNDERLINE       0x02
#define BLINK           0x04
#define REVERSE         0x08
#define ITALIC          0x10
#define SELECTED        0xF0
#define COLOR_FG_NORM   37
#define COLOR_BG_NORM   49

#define NCHARSETS       2
#define G0              0
#define G1              1
#define UK_CHARSET      0x01
#define US_CHARSET      0x02
#define GRAPHICS        0x04

#define TERM_LAST_RIGHT_CORNER      "\033[999C\033[999B"
#define TERM_LAST_RIGHT_CORNER_LEN  12
#define TERM_GET_PTR_POS            "\033[6n"
#define TERM_GET_PTR_POS_LEN        4
#define TERM_SCREEN_SAVE            "\033[?47h"
#define TERM_SCREEN_SAVE_LEN        6
#define TERM_SCREEN_RESTORE        "\033[?47l"
#define TERM_SCREEN_RESTORE_LEN     6
#define TERM_SCREEN_CLEAR           "\033[2J"
#define TERM_SCREEN_CLEAR_LEN       4
#define TERM_SCROLL_RESET           "\033[r"
#define TERM_SCROLL_RESET_LEN       3
#define TERM_GOTO_PTR_POS_FMT       "\033[%d;%dH"
#define TERM_CURSOR_HIDE            "\033[?25l"
#define TERM_CURSOR_HIDE_LEN        6
#define TERM_CURSOR_SHOW            "\033[?25h"
#define TERM_CURSOR_SHOW_LEN        6
#define TERM_AUTOWRAP_ON            "\033[?7h"
#define TERM_AUTOWRAP_ON_LEN        5
#define TERM_AUTOWRAP_OFF           "\033[?7l"
#define TERM_AUTOWRAP_OFF_LEN       5

#define TERM_SEND_ESC_SEQ(seq) fd_write (this->out_fd, seq, seq ## _LEN)

#define COLOR_FG_NORMAL   39

#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"

#define COLOR_FOCUS     COLOR_GREEN
#define COLOR_UNFOCUS   COLOR_RED

#define BACKSPACE_KEY   010
#define ESCAPE_KEY      033
#define ARROW_DOWN_KEY  0402
#define ARROW_UP_KEY    0403
#define ARROW_LEFT_KEY  0404
#define ARROW_RIGHT_KEY 0405
#define HOME_KEY        0406
#define FN_KEY(x)       (x + 0410)
#define DELETE_KEY      0512
#define INSERT_KEY      0513
#define PAGE_DOWN_KEY   0522
#define PAGE_UP_KEY     0523
#define END_KEY         0550

enum vt_keystate {
  norm,
  appl
};

typedef struct dirlist_t dirlist_t;

struct dirlist_t {
  char **list;
  int len;
  size_t size;
  int retval;
  char dir[PATH_MAX];
  void (*free) (dirlist_t *dlist);
};

typedef struct tmpname_t tmpname_t;
struct tmpname_t {
  int fd;
  char fname[PATH_MAX];
 };

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

struct vwm_frame {
  char
    mb_buf[8],
    tty_name[1024],
    *argv[MAX_ARGS],
    *logfile;

  uchar
    charset[2],
    textattr,
    saved_textattr;

  int
    fd,
    logfd,
    status,
    mb_len,
    mb_curlen,
    col_pos,
    row_pos,
    new_rows,
    num_rows,
    num_cols,
    last_row,
    first_row,
    first_col,
    scroll_first_row,
    param_idx,
    remove_log,
    saved_row_pos,
    saved_col_pos,
    old_attribute,
    **videomem,
    **colors,
    *tabstops,
    *esc_param,
    *cur_param;

  utf8 mb_code;

  enum vt_keystate key_state;

  pid_t pid;

  vt_string *output;

  ProcessChar process_char;
  Unimplemented unimplemented;

  vwm_win *parent;
  vwm_t   *root;

  vwm_frame
    *next,
    *prev;
};

struct vwm_win {
  char *name;

  vt_string
    *render,
    *separators_buf;

  int
    saved_row,
    saved_col,
    cur_row,
    cur_col,
    num_rows,
    num_cols,
    first_row,
    first_col,
    last_row,
    max_frames,
    draw_separators,
    num_separators,
    is_initialized;

  vwm_frame
    *head,
    *current,
    *tail,
    *last_frame;

  int
    cur_idx,
    length;

  vwm_t *parent;

  vwm_win
    *next,
    *prev;
};

struct vwm_prop {
  vwm_term  *term;

  char *tmpdir;

  vt_string
    *shell,
    *editor;

  int
    state,
    name_gen,
    num_rows,
    num_cols,
    need_resize,
    first_column;

  uint modes;

  vwm_win
    *head,
    *current,
    *tail,
    *last_win;

  int
    cur_idx,
    length;

  OnTabCallback on_tab_callback;
};

private void vwm_sigwinch_handler (int sig);

private const utf8 offsetsFromUTF8[6] = {
  0x00000000UL, 0x00003080UL, 0x000E2080UL,
  0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

private utf8 ustring_to_code (char *buf, int *idx) {
  if (NULL is buf or 0 > *idx or 0 is buf[*idx])
    return 0;

  utf8 code = 0;
  int sz = 0;

  do {
    code <<= 6;
    code += (uchar) buf[(*idx)++];
    sz++;
  } while (buf[*idx] and IS_UTF8 (buf[*idx]));

  code -= offsetsFromUTF8[sz-1];

  return code;
}

private int ustring_charlen (uchar c) {
  if (c < 0x80) return 1;
  if ((c & 0xe0) == 0xc0) return 2;
  return 3 + ((c & 0xf0) != 0xe0);
}

private char *ustring_character (utf8 c, char *buf, int *len) {
  *len = 1;
  if (c < 0x80) {
    buf[0] = (char) c;
  } else if (c < 0x800) {
    buf[0] = (c >> 6) | 0xC0;
    buf[1] = (c & 0x3F) | 0x80;
    (*len)++;
  } else if (c < 0x10000) {
    buf[0] = (c >> 12) | 0xE0;
    buf[1] = ((c >> 6) & 0x3F) | 0x80;
    buf[2] = (c & 0x3F) | 0x80;
    (*len) += 2;
  } else if (c < 0x110000) {
    buf[0] = (c >> 18) | 0xF0;
    buf[1] = ((c >> 12) & 0x3F) | 0x80;
    buf[2] = ((c >> 6) & 0x3F) | 0x80;
    buf[3] = (c & 0x3F) | 0x80;
    (*len) += 3;
  } else
    return 0;

  buf[*len] = '\0';
  return buf;
}

private int dir_is_directory (const char *name) {
  struct stat st;

  if (-1 is stat (name, &st))
    return 0;

  return S_ISDIR(st.st_mode);
}

private char *path_basename (const char *name) {
  char *p = strchr (name, 0);

  while (p > name and !IS_DIRSEP (p[-1]))
    --p;

  return p;
}

private void dirlist_free (dirlist_t *dlist) {
  if (NULL is dlist->list)
    return;

  for (int i = 0; i < dlist->len; i++)
    free (dlist->list[i]);

  free (dlist->list);

  dlist->list = NULL;
}

private dirlist_t dir_list (char *dir) {
  dirlist_t dlist = {
    .len = 0,
    .size = 32,
    .retval = -1,
    .free = dirlist_free
    };

  ifnot (dir_is_directory (dir))
    return dlist;

  strncpy (dlist.dir, dir, PATH_MAX - 1);

  DIR *dh = NULL;
  struct dirent *dp;
  size_t len;

  dlist.list = Alloc (dlist.size * sizeof (char *));

  ifnull (dh = opendir (dir)) {
    dlist.retval = errno;
    return dlist;
  }

  while (1) {
    errno = 0;

    ifnull (dp = readdir (dh))
      break;

    len = bytelen (dp->d_name);

    if (len < 3 and dp->d_name[0] is '.')
      if (len is 1 or dp->d_name[1] is '.')
        continue;

    if ((size_t) dlist.len is dlist.size) {
      dlist.size = dlist.size * 2;
      dlist.list = Realloc (dlist.list, dlist.size * sizeof (char *));
    }

    dlist.list[dlist.len] = Alloc ((size_t) len + 1);
    strncpy (dlist.list[dlist.len], dp->d_name, len + 1);
    dlist.len++;
  }

  closedir (dh);
  dlist.retval = errno;
  return dlist;
}

private tmpname_t tmpfname (char *dname, char *prefix) {
  static unsigned int see = 12252;
  tmpname_t t;
  t.fd = -1;
  t.fname[0] = '\0';

  ifnot (dir_is_directory (dname))
    return t;

  char bpid[6];
  pid_t pid = getpid ();
  snprintf (bpid, 6, "%d", pid);

  int len = bytelen (dname) + bytelen (bpid) + bytelen (prefix) + 10;

  char name[len];
  snprintf (name, len, "%s/%s-%s.xxxxxx", dname, prefix, bpid);

  srand ((uint) time (NULL) + (uint) pid + see++);

  dirlist_t dlist = dir_list (dname);
  if (NOTOK is dlist.retval) {
    return t;
  }

  int
    found = 0,
    loops = 0,
    max_loops = 1024,
    inner_loops = 0,
    max_inner_loops = 1024;
  char c;

  while (1) {
again:
    found = 0;
    if (++loops is max_loops)
      goto theend;

    for (int i = 0; i < 6; i++) {
      inner_loops = 0;
      while (1) {
        if (++inner_loops is max_inner_loops)
          goto theend;

        c = (char) (rand () % 123);
        if ((c <= 'z' and c >= 'a') or (c >= '0' and c <= '9') or
            (c >= 'A' and c <= 'Z') or c is '_') {
          name[len - i - 2] = c;
          break;
        }
      }
    }

    for (int i = 0; i < dlist.len; i++)
      if (0 is strcmp (name, dlist.list[i]))
        goto again;

    found = 1;
    break;
  }

  ifnot (found)
    goto theend;

  t.fd = open (name, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);

  if (-1 isnot t.fd) {
    if (-1 is fchmod (t.fd, 0600)) {
      close (t.fd);
      t.fd = -1;
      goto theend;
    }

  strncpy (t.fname, name, len);
  }

theend:
  dlist.free (&dlist);
  return t;
}

#define CONTINUE_ON_EXPECTED_ERRNO(fd__)          \
  if (errno == EINTR) continue;                   \
  if (errno == EAGAIN) {                          \
    struct timeval tv;                            \
    fd_set read_fd;                               \
    FD_ZERO(&read_fd);                            \
    FD_SET(fd, &read_fd);                         \
    tv.tv_sec = 0;                                \
    tv.tv_usec = 100000;                          \
    select (fd__ + 1, &read_fd, NULL, NULL, &tv); \
    continue;                                     \
   } do {} while (0)

private int fd_read (int fd, char *buf, size_t len) {
  if (1 > len)
    return NOTOK;

  char *s = buf;
  ssize_t bts;
  int tbts = 0;

  while (1) {
    if (NOTOK == (bts = read (fd, s, len))) {
      CONTINUE_ON_EXPECTED_ERRNO (fd);
      return NOTOK;
    }

    tbts += bts;
    if (tbts == (int) len || bts == 0)
      break;

    s += bts;
  }

  buf[tbts] = '\0';
  return bts;
}

private int fd_write (int fd, char *buf, size_t len) {
  int retval = len;
  int bts;

  while (len > 0) {
    if (NOTOK == (bts = write (fd, buf, len))) {
      CONTINUE_ON_EXPECTED_ERRNO (fd);
      return NOTOK;
    }

    len -= bts;
    buf += bts;
  }

  return retval;
}


private size_t byte_cp (char *dest, const char *src, size_t nelem) {
  const char *sp = src;
  size_t len = 0;

  while (len < nelem and *sp) { // this differs in memcpy()
    dest[len] = *sp++;
    len++;
  }

  return len;
}

private size_t string_align (size_t size) {
  size_t sz = 8 - (size % 8);
  sz = sizeof (char) * (size + (sz < 8 ? sz : 0));
  return sz;
}

/* this is not like realloc(), as size here is the extra size */
private vt_string *vt_string_reallocate (vt_string *this, size_t size) {
  size_t sz = string_align (this->mem_size + size + 1);
  this->bytes = Realloc (this->bytes, sz);
  this->mem_size = sz;
  return this;
}

private vt_string *vt_string_new (size_t size) {
  vt_string *this = Alloc (sizeof (vt_string));
  size_t sz = (size <= 0 ? 8 : string_align (size));
  this->bytes = Alloc (sz);
  this->mem_size = sz;
  this->num_bytes = 0;
  *this->bytes = '\0';
  return this;
}

private vt_string *vt_string_new_with_len (const char *bytes, size_t len) {
  vt_string *new = Alloc (sizeof (vt_string));
  size_t sz = string_align (len + 1);
  char *buf = Alloc (sz);
  byte_cp (buf, bytes, len);
  buf[len] = '\0';
  new->bytes = buf;
  new->num_bytes = len;
  new->mem_size = sz;
  return new;
}

private vt_string *vt_string_new_with (const char *bytes) {
  size_t len = (NULL is bytes ? 0 : bytelen (bytes));
  return vt_string_new_with_len (bytes, len); /* this succeeds even if bytes is NULL */
}

private void vt_string_release (vt_string *this) {
  free (this->bytes);
  free (this);
}

private vt_string *vt_string_clear (vt_string *this) {
  this->bytes[0] = '\0';
  this->num_bytes = 0;
  return this;
}

private vt_string *vt_string_clear_at (vt_string *this, int idx) {
  if (0 > idx) idx += this->num_bytes;
  if (idx < 0) return this;
  if (idx > (int) this->num_bytes) idx = this->num_bytes;
  this->bytes[idx] = '\0';
  this->num_bytes = idx;
  return this;
}

private vt_string *vt_string_append_with_len (vt_string *this, char *bytes, size_t len) {
  size_t bts = this->num_bytes + len;
  if (bts >= this->mem_size)
    this = vt_string_reallocate (this, bts - this->mem_size + 1);

  byte_cp (this->bytes + this->num_bytes, bytes, len);
  this->num_bytes += len;
  this->bytes[this->num_bytes] = '\0';
  return this;
}

private vt_string *vt_string_append (vt_string *this, char *bytes) {
  return vt_string_append_with_len (this, bytes, bytelen (bytes));
}

private vt_string *vt_string_append_byte (vt_string *this, char c) {
  int bts = this->mem_size - (this->num_bytes + 2);
  if (1 > bts) vt_string_reallocate (this, 8);
  this->bytes[this->num_bytes++] = c;
  this->bytes[this->num_bytes] = '\0';
  return this;
}

public VtString_T __init_vt_string__ (void) {
  return (VtString_T) {
    .new = vt_string_new,
    .clear = vt_string_clear,
    .append = vt_string_append,
    .release = vt_string_release,
    .new_with = vt_string_new_with,
    .clear_at = vt_string_clear_at,
    .append_byte = vt_string_append_byte,
    .new_with_len = vt_string_new_with_len,
    .append_with_len = vt_string_append_with_len
  };
}

private vwm_term *vwm_term_new (void) {
  vwm_term *this = Alloc (sizeof (vwm_term));
  this->lines = 24;
  this->columns = 78;
  this->in_fd = STDIN_FILENO;
  this->out_fd = STDOUT_FILENO;
  this->mode = 'o';

  char *term_name = getenv ("TERM");
  if (NULL is term_name) {
    fprintf (stderr, "TERM environment variable isn't set\n");
    this->name = strdup ("vt100");
  } else {
    if (0 is strcmp (term_name, "xterm"))
      this->name = strdup ("xterm");
    else
      this->name = strdup ("vt100");
  }

  return this;
}

private void vwm_term_release (vwm_term **thisp) {
  if (NULL == *thisp) return;
  free ((*thisp)->name);
  free (*thisp);
  *thisp = NULL;
}

private int vwm_term_sane_mode (vwm_term *this) {
  if (this->mode == 's') return OK;
  if (isnotatty (this->in_fd)) return NOTOK;

  struct termios mode;
  while (NOTOK  == tcgetattr (this->in_fd, &mode))
    if (errno == EINTR) return NOTOK;

  mode.c_iflag |= (BRKINT|INLCR|ICRNL|IXON|ISTRIP);
  mode.c_iflag &= ~(IGNBRK|INLCR|IGNCR|IXOFF);
  mode.c_oflag |= (OPOST|ONLCR);
  mode.c_lflag |= (ECHO|ECHOE|ECHOK|ECHOCTL|ISIG|ICANON|IEXTEN);
  mode.c_lflag &= ~(ECHONL|NOFLSH|TOSTOP|ECHOPRT);
  mode.c_cc[VEOF] = 'D'^64; // splitvt
  mode.c_cc[VMIN] = 1;   /* 0 */
  mode.c_cc[VTIME] = 0;  /* 1 */

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &mode))
    if (errno == EINTR) return NOTOK;

  this->mode = 's';
  return OK;
}

private int vwm_term_orig_mode (vwm_term *this) {
  if (this->mode == 'o') return OK;
  if (isnotatty (this->in_fd)) return NOTOK;

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &this->orig_mode))
    ifnot (errno == EINTR) return NOTOK;

  this->mode = 'o';
  return OK;
}

private int vwm_term_raw_mode (vwm_term *this) {
   if (this->mode == 'r') return OK;
   if (isnotatty (this->in_fd)) return NOTOK;

  while (NOTOK == tcgetattr (this->in_fd, &this->orig_mode))
    if (errno == EINTR) return NOTOK;

  this->raw_mode = this->orig_mode;
  this->raw_mode.c_iflag &= ~(INLCR|ICRNL|IXON|ISTRIP);
  this->raw_mode.c_cflag |= (CS8);
  this->raw_mode.c_oflag &= ~(OPOST);
  this->raw_mode.c_lflag &= ~(ECHO|ISIG|ICANON|IEXTEN);
  this->raw_mode.c_lflag &= NOFLSH;
  this->raw_mode.c_cc[VEOF] = 1;
  this->raw_mode.c_cc[VMIN] = 0;   /* 1 */
  this->raw_mode.c_cc[VTIME] = 1;  /* 0 */

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &this->raw_mode))
    ifnot (errno == EINTR) return NOTOK;

  this->mode = 'r';
  return OK;
}

private int vwm_term_cursor_get_ptr_pos (vwm_term *this, int *row, int *col) {
  if (NOTOK == TERM_SEND_ESC_SEQ (TERM_GET_PTR_POS))
    return NOTOK;

  char buf[32];
  uint i = 0;
  int bts;
  while (i < sizeof (buf) - 1) {
    if (NOTOK == (bts = fd_read (this->in_fd, buf + i, 1)) ||
         bts == 0)
      return NOTOK;

    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != ESCAPE_KEY || buf[1] != '[' ||
      2 != sscanf (buf + 2, "%d;%d", row, col))
    return NOTOK;

  return OK;
}

private void vwm_term_cursor_set_ptr_pos (vwm_term *this, int row, int col) {
  char ptr[32];
  snprintf (ptr, 32, TERM_GOTO_PTR_POS_FMT, row, col);
  fd_write (this->out_fd, ptr, bytelen (ptr));
}

private void vwm_term_screen_clear (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCREEN_CLEAR);
}

private void vwm_term_screen_save (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCREEN_SAVE);
}

private void vwm_term_screen_restore (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCROLL_RESET);
  TERM_SEND_ESC_SEQ (TERM_SCREEN_RESTORE);
}

private void vwm_term_init_size (vwm_term *this, int *rows, int *cols) {
  struct winsize wsiz;

  do {
    if (OK == ioctl (this->out_fd, TIOCGWINSZ, &wsiz)) {
      this->lines = (int) wsiz.ws_row;
      this->columns = (int) wsiz.ws_col;
      *rows = this->lines; *cols = this->columns;
      return;
    }
  } while (errno == EINTR);

  int orig_row, orig_col;
  vwm_term_cursor_get_ptr_pos (this, &orig_row, &orig_col);

  TERM_SEND_ESC_SEQ (TERM_LAST_RIGHT_CORNER);
  vwm_term_cursor_get_ptr_pos (this, rows, cols);
  vwm_term_cursor_set_ptr_pos (this, orig_row, orig_col);
}

private void vwm_set_size (vwm_t *this, int rows, int cols, int first_col) {
  $my(num_rows) = rows;
  $my(num_cols) = cols;
  $my(first_column) = first_col;
}

private void vwm_set_state (vwm_t *this, int state) {
  $my(state) = state;
}

private void vwm_set_editor (vwm_t *this, char *editor) {
  if (NULL is editor) return;
  size_t len = bytelen (editor);
  ifnot (len) return;
  VtString.clear ($my(editor));
  VtString.append_with_len ($my(editor), editor, len);
}

private void vwm_set_on_tab_callback (vwm_t *this, OnTabCallback cb) {
  $my(on_tab_callback) = cb;
}

private void vwm_set_shell (vwm_t *this, char *shell) {
  if (NULL is shell) return;
  size_t len = bytelen (shell);
  ifnot (len) return;
  VtString.clear ($my(shell));
  VtString.append_with_len ($my(shell), shell, len);
}

/* This is an extended version of the same function of the kilo editor at:
 * https://github.com/antirez/kilo.git
 *
 * It should work the same, under xterm, rxvt-unicode, st and linux terminals.
 * It also handles UTF8 byte sequences and it should return the integer represantation
 * of such sequence
 */

private utf8 getkey (int infd) {
  char c;
  int n;
  char buf[5];

  while (0 == (n = fd_read (infd, buf, 1)));

  if (n == -1) return -1;

  c = buf[0];

  switch (c) {
    case ESCAPE_KEY:
      if (0 == fd_read (infd, buf, 1))
        return ESCAPE_KEY;

      /* recent (revailed through CTRL-[other than CTRL sequence]) and unused */
      if ('z' >= buf[0] && buf[0] >= 'a')
        return 0;

      if (buf[0] == ESCAPE_KEY /* probably alt->arrow-key */)
        if (0 == fd_read (infd, buf, 1))
          return 0;

      if (buf[0] != '[' && buf[0] != 'O')
        return 0;

      if (0 == fd_read (infd, buf + 1, 1))
        return ESCAPE_KEY;

      if (buf[0] == '[') {
        if ('0' <= buf[1] && buf[1] <= '9') {
          if (0 == fd_read (infd, buf + 2, 1))
            return ESCAPE_KEY;

          if (buf[2] == '~') {
            switch (buf[1]) {
              case '1': return HOME_KEY;
              case '2': return INSERT_KEY;
              case '3': return DELETE_KEY;
              case '4': return END_KEY;
              case '5': return PAGE_UP_KEY;
              case '6': return PAGE_DOWN_KEY;
              case '7': return HOME_KEY;
              case '8': return END_KEY;
              default: return 0;
            }
          } else if (buf[1] == '1') {
            if (fd_read (infd, buf, 1) == 0)
              return ESCAPE_KEY;

            switch (buf[2]) {
              case '1': return FN_KEY(1);
              case '2': return FN_KEY(2);
              case '3': return FN_KEY(3);
              case '4': return FN_KEY(4);
              case '5': return FN_KEY(5);
              case '7': return FN_KEY(6);
              case '8': return FN_KEY(7);
              case '9': return FN_KEY(8);
              default: return 0;
            }
          } else if (buf[1] == '2') {
            if (fd_read (infd, buf, 1) == 0)
              return ESCAPE_KEY;

            switch (buf[2]) {
              case '0': return FN_KEY(9);
              case '1': return FN_KEY(10);
              case '3': return FN_KEY(11);
              case '4': return FN_KEY(12);
              default: return 0;
            }
          } else { /* CTRL_[key other than CTRL sequence] */
                   /* lower case */
            if (buf[2] == 'h')
              return INSERT_KEY; /* sample/test (logically return 0) */
            else
              return 0;
          }
        } else if (buf[1] == '[') {
          if (fd_read (infd, buf, 1) == 0)
            return ESCAPE_KEY;

          switch (buf[0]) {
            case 'A': return FN_KEY(1);
            case 'B': return FN_KEY(2);
            case 'C': return FN_KEY(3);
            case 'D': return FN_KEY(4);
            case 'E': return FN_KEY(5);

            default: return 0;
          }
        } else {
          switch (buf[1]) {
            case 'A': return ARROW_UP_KEY;
            case 'B': return ARROW_DOWN_KEY;
            case 'C': return ARROW_RIGHT_KEY;
            case 'D': return ARROW_LEFT_KEY;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            case 'P': return DELETE_KEY;

            default: return 0;
          }
        }
      } else if (buf[0] == 'O') {
        switch (buf[1]) {
          case 'A': return ARROW_UP_KEY;
          case 'B': return ARROW_DOWN_KEY;
          case 'C': return ARROW_RIGHT_KEY;
          case 'D': return ARROW_LEFT_KEY;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
          case 'P': return FN_KEY(1);
          case 'Q': return FN_KEY(2);
          case 'R': return FN_KEY(3);
          case 'S': return FN_KEY(4);

          default: return 0;
        }
      }
    break;

  default:
    if (c < 0) {
      int len = ustring_charlen ((uchar) c);
      utf8 code = 0;
      code += (uchar) c;

      int idx;
      int invalid = 0;
      char cc;

      for (idx = 0; idx < len - 1; idx++) {
        if (0 >= fd_read (infd, &cc, 1))
          return -1;

        if (isnotutf8 ((uchar) cc)) {
          invalid = 1;
        } else {
          code <<= 6;
          code += (uchar) cc;
        }
      }

      if (invalid)
        return -1;

      code -= offsetsFromUTF8[len-1];
      return code;
    }

    if (127 == c) return BACKSPACE_KEY;

    return c;
  }

  return -1;
}

private void vwm_set_tmpdir (vwm_t *this, char *dir, size_t len) {
  ifnot (NULL is $my(tmpdir))
    free ($my(tmpdir));

  if (NULL is dir) {
    len = bytelen (TMPDIR);
    $my(tmpdir) = Alloc (len + 1);
    strncpy ($my(tmpdir), TMPDIR, len + 1);
    return;
  }

  $my(tmpdir) = Alloc (len + 1);
  strncpy ($my(tmpdir), dir, len + 1);
}

private int vwm_get_lines (vwm_t *this) {
  return $my(term)->lines;
}

private int vwm_get_columns (vwm_t *this) {
  return $my(term)->columns;
}

private vwm_win *vwm_get_current_win (vwm_t *this) {
  return $my(current);
}

private vwm_frame *vwm_get_current_frame (vwm_t *this) {
  return $my(current)->current;
}

private vwm_term *vwm_get_term (vwm_t *this) {
  return $my(term);
}

private int vwm_get_state (vwm_t *this) {
  return $my(state);
}

private int **vwm_alloc_ints (int rows, int cols, int val) {
  int **obj;

  obj = Alloc (rows * sizeof (int *));

  for (int i = 0; i < rows; i++) {
    obj[i] = Alloc (sizeof (int *) * cols);

    for (int j = 0; j < cols; j++)
      obj[i][j] = val;
 }

 return obj;
}

private int vt_video_line_to_str (int *line, char *buf, int len) {
  int idx = 0;
  utf8 c;

  for (int i = 0; i < len; i++) {
    c = line[i];

    ifnot (c) continue;

    if ((c & 0xFF) < 0x80)
      buf[idx++] = c & 0xFF;
    else if (c < 0x800) {
      buf[idx++] = (c >> 6) | 0xC0;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else if (c < 0x10000) {
      buf[idx++] = (c >> 12) | 0xE0;
      buf[idx++] = ((c >> 6) & 0x3F) | 0x80;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else if (c < 0x110000) {
      buf[idx++] = (c >> 18) | 0xF0;
      buf[idx++] = ((c >> 12) & 0x3F) | 0x80;
      buf[idx++] = ((c >> 6) & 0x3F) | 0x80;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else {
      continue;
    }
  }

  buf[idx++] = '\n';

  buf[idx] = '\0';

  return idx;
}

private void vt_video_add_log_lines (vwm_frame *this) {
  if (-1 is this->logfd)
    return;

  struct stat st;
  if (-1 is (fstat (this->logfd, &st)))
    return;

  int size = st.st_size;

  char *mbuf = mmap (0, size, PROT_READ, MAP_SHARED, this->logfd, 0);

  if (NULL is mbuf)
    return;

  char *buf = mbuf + size - 1;

  int lines = this->num_rows;

  for (int i = 0; i < lines; i++)
    for (int j = 0; j < this->num_cols; j++)
      this->videomem[i][j] = 0;

  while (lines isnot 0 and size) {
    char b[BUFSIZE];
    char c;
    int rbts = 0;
    while (--size) {
      c = *--buf;
      if (c is '\n')
        break;
      ifnot (c)
        continue;
      b[rbts++] = c;
    }

    b[rbts] = '\0';

    int blen = bytelen (b);

    char nbuf[blen + 1];
    for (int i = 0; i < blen; i++)
      nbuf[i] = b[blen - i - 1];

    nbuf[blen] = '\0';

    int idx = 0;
    for (int i = 0; i < this->num_cols; i++) {
      if (idx >= blen)
        break;
      this->videomem[lines-1][i] =
         (int) ustring_to_code (nbuf, &idx);
    }

    lines--;
  }

  ftruncate (this->logfd, size);
  lseek (this->logfd, size, SEEK_SET);
  munmap (0, st.st_size);
}

private void vt_video_erase (vwm_frame *frame, int x1, int x2, int y1, int y2) {
  int i, j;

  for (i = x1 - 1; i < x2; ++i)
    for (j = y1 - 1; j < y2; ++j) {
      frame->videomem[i][j] = 0;
      frame->colors  [i][j] = COLOR_FG_NORM;
    }
}

private void vt_frame_video_rshift (vwm_frame *frame, int numcols) {
  for (int i = frame->num_cols - 1; i > frame->col_pos - 1; --i) {
    if (i - numcols >= 0) {
      frame->videomem[frame->row_pos-1][i] = frame->videomem[frame->row_pos-1][i-numcols];
      frame->colors[frame->row_pos-1][i] = frame->colors[frame->row_pos-1][i-numcols];
    } else {
      frame->videomem[frame->row_pos-1][i] = 0;
      frame->colors [frame->row_pos-1][i] = COLOR_FG_NORM;
    }
  }
}

private void vt_video_scroll (vwm_frame *frame, int numlines) {
  int *tmpvideo;
  int *tmpcolors;
  int n;

  for (int i = 0; i < numlines; i++) {
    tmpvideo = frame->videomem[frame->scroll_first_row - 1];
    tmpcolors = frame->colors[frame->scroll_first_row - 1];

    ifnot (NULL is frame->logfile) {
      char buf[(frame->num_cols * 3) + 2];
      int len = vt_video_line_to_str (tmpvideo, buf, frame->num_cols);
      fd_write (frame->logfd, buf, len);
    }

    for (int j = 0; j < frame->num_cols; j++) {
      tmpvideo[j] = 0;
      tmpcolors[j] = COLOR_FG_NORM;
    }

    for (n = frame->scroll_first_row - 1; n < frame->last_row - 1; n++) {
      frame->videomem[n] = frame->videomem[n + 1];
      frame->colors[n] = frame->colors[n + 1];
    }

    frame->videomem[n] = tmpvideo;
    frame->colors[n] = tmpcolors;
  }
}

private void vt_video_scroll_back (vwm_frame *frame, int numlines) {
  if (frame->row_pos < frame->scroll_first_row)
    return;

  int n;
  int *tmpvideo;
  int *tmpcolors;

  for (int i = 0; i < numlines; i++) {
    tmpvideo = frame->videomem[frame->last_row - 1];
    tmpcolors = frame->colors[frame->last_row - 1];

    for (int j = 0; j < frame->num_cols; j++) {
      tmpvideo[j] = 0;
      tmpcolors[j] = COLOR_FG_NORM;
    }

   for (n = frame->last_row - 1; n > frame->scroll_first_row - 1; --n) {
      frame->videomem[n] = frame->videomem[n - 1];
      frame->colors[n] = frame->colors[n - 1];
    }

    frame->videomem[n] = tmpvideo;
    frame->colors[n] = tmpcolors;
  }
}

private void vt_video_add (vwm_frame *frame, utf8 c) {
  frame->videomem[frame->row_pos - 1][frame->col_pos - 1] = c;
  frame->videomem[frame->row_pos - 1][frame->col_pos - 1] |=
      (((int) frame->textattr) << 8);
}

private void vt_write (char *buf, FILE *fp) {
  fprintf (fp, "%s", buf);
  fflush (fp);
}

private vt_string *vt_insline (vt_string *buf, int num) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dL", num));
}

private vt_string *vt_insertchar (vt_string *buf, int numcols) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%d@", numcols));
}

private vt_string *vt_savecursor (vt_string *buf) {
  return VtString.append (buf, "\0337");
}

private vt_string *vt_restcursor (vt_string *buf) {
  return VtString.append (buf, "\0338");
}

private vt_string *vt_clreol (vt_string *buf) {
  return VtString.append (buf, "\033[K");
}

private vt_string *vt_clrbgl (vt_string *buf) {
  return VtString.append (buf, "\033[1K");
}

private vt_string *vt_clrline (vt_string *buf) {
  return VtString.append (buf, "\033[2K");
}

private vt_string *vt_delunder (vt_string *buf, int num) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dP", num));
}

private vt_string *vt_delline (vt_string *buf, int num) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dM", num));
}

private vt_string *vt_attr_reset (vt_string *buf) {
  return VtString.append (buf, "\033[m");
}

private vt_string *vt_reverse (vt_string *buf, int on) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%sm", (on ? "7" : "27")));
}

private vt_string *vt_underline (vt_string *buf, int on) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%sm", (on ? "4" : "24")));
}

private vt_string *vt_bold (vt_string *buf, int on) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%sm", (on ? "1" : "22")));
}

private vt_string *vt_italic (vt_string *buf, int on) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%sm", (on ? "3" : "23")));
}

private vt_string *vt_blink (vt_string *buf, int on) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%sm", (on ? "5" : "25")));
}

private vt_string *vt_bell (vt_string *buf) {
  return VtString.append (buf, "\007");
}

private vt_string *vt_setfg (vt_string *buf, int color) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%d;1m", color));
}

private vt_string *vt_setbg (vt_string *buf, int color) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%d;1m", color));
}

private vt_string *vt_left (vt_string *buf, int count) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dD", count));
}

private vt_string *vt_right (vt_string *buf, int count) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dC", count));
}

private vt_string *vt_up (vt_string *buf, int numrows) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dA", numrows));
}

private vt_string *vt_down (vt_string *buf, int numrows) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%dB", numrows));
}

private vt_string *vt_revscroll (vt_string *buf) {
  return VtString.append (buf, "\033M");
}

private vt_string *vt_setscroll (vt_string *buf, int first, int last) {
  if (0 is first and 0 is last)
    return VtString.append (buf, "\033[r");
  else
    return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%d;%dr", first, last));
}

private vt_string *vt_goto (vt_string *buf, int row, int col) {
  return VtString.append (buf, STR_FMT (MAX_SEQ_LEN, "\033[%d;%dH", row, col));
}

private vt_string *vt_attr_check (vt_string *buf, int pixel, int lastattr, uchar *currattr) {
  uchar
    simplepixel,
    lastpixel,
    change,
    selected,
    reversed;

 /* Set the simplepixel REVERSE bit if SELECTED ^ REVERSE */
  simplepixel = ((pixel  >> 8) & (~SELECTED)  & (~REVERSE));
  selected    = (((pixel >> 8) & (~SELECTED)) ? 1 : 0);
  reversed    = (((pixel >> 8) & (~REVERSE))  ? 1 : 0);

  if (selected ^ reversed)
    simplepixel |= REVERSE;

  /* Set the lastpixel REVERSE bit if SELECTED ^ REVERSE */
  lastpixel = ((lastattr  >> 8) & (~SELECTED)  & (~REVERSE));
  selected  = (((lastattr >> 8) & (~SELECTED)) ? 1 : 0);
  reversed  = (((lastattr >> 8) & (~REVERSE))  ? 1 : 0);

  if (selected ^ reversed)
    lastpixel |= REVERSE;

 /* Thanks to Dan Dorough for the XOR code */
checkchange:
  change = (lastpixel ^ simplepixel);
  if (change) {
    if (change & REVERSE) {
      if ((*currattr) & REVERSE) {
#define GOTO_HACK          /* vt_reverse (0) doesn't work on xterms? */
#ifdef  GOTO_HACK          /* This goto hack resets all current attributes */
        vt_attr_reset (buf);
        *currattr &= ~REVERSE;
        simplepixel = 0;
        lastpixel &= (~REVERSE);
        goto checkchange;
#else
        vt_reverse (buf, 0);
        *currattr &= ~REVERSE;
#endif
      } else {
        vt_reverse (buf, 1);
        *currattr |= REVERSE;
      }
    }

    if (change & BOLD) {
      if ((*currattr) & BOLD) {
        vt_bold (buf, 0);
        *currattr &= ~BOLD;
      } else {
        vt_bold (buf, 1);
        *currattr |= BOLD;
      }
    }

    if (change & ITALIC) {
      if ((*currattr) & ITALIC) {
        vt_italic (buf, 0);
        *currattr &= ~ITALIC;
      } else {
        vt_italic (buf, 1);
        *currattr |= ITALIC;
      }
    }

    if (change & UNDERLINE) {
      if ((*currattr) & UNDERLINE) {
        vt_underline (buf, 0);
        *currattr &= ~UNDERLINE;
      } else {
        vt_underline (buf, 1);
        *currattr |= UNDERLINE;
      }
    }

    if (change & BLINK) {
      if ((*currattr) & BLINK) {
        vt_blink (buf, 0);
        *currattr &= ~BLINK;
      } else {
        vt_blink (buf, 1);
        *currattr |= BLINK;
      }
    }
  }

  return buf;
}

private vt_string *vt_attr_set (vt_string *buf, int textattr) {
  vt_attr_reset (buf);

  if (textattr & BOLD)
    vt_bold (buf, 1);

  if (textattr & UNDERLINE)
    vt_underline (buf, 1);

  if (textattr & BLINK)
    vt_blink (buf, 1);

  if (textattr & REVERSE)
    vt_reverse (buf, 1);

  if (textattr & ITALIC)
    vt_italic (buf, 1);

  return buf;
}

private vt_string *vt_frame_attr_set (vwm_frame *frame, vt_string *buf) {
  uchar on = NORMAL;
  vt_attr_reset (buf);
  return vt_attr_check (buf, 0, frame->textattr, &on);
}

private vt_string *vt_append (vwm_frame *frame, vt_string *buf, utf8 c) {
  if (frame->col_pos > frame->num_cols) {
    if (frame->row_pos < frame->last_row)
      frame->row_pos++;
    else
      vt_video_scroll (frame, 1);

    VtString.append (buf, "\r\n");
    frame->col_pos = 1;
  }

  vt_video_add (frame, c);

  if (frame->mb_len)
    VtString.append_with_len (buf, frame->mb_buf, frame->mb_len);
  else
    VtString.append_byte (buf, c);

  frame->col_pos++;
  return buf;
}

private vt_string *vt_keystate_print (vt_string *buf, int application) {
  if (application)
    return VtString.append (buf, "\033=\033[?1h");

  return VtString.append (buf, "\033>\033[?1l");
}

private vt_string *vt_altcharset (vt_string *buf, int charset, int type) {
  switch (type) {
    case UK_CHARSET:
      VtString.append_with_len (buf, STR_FMT (4, "\033%cA", (charset is G0 ? '(' : ')')), 3);
      break;

    case US_CHARSET:
      VtString.append_with_len (buf, STR_FMT (4, "\033%cB", (charset is G0 ? '(' : ')')), 3);
      break;

    case GRAPHICS:
      VtString.append_with_len (buf, STR_FMT (4, "\033%c0", (charset is G0 ? '(' : ')')), 3);
      break;

    default:  break;
  }
  return buf;
}

private vt_string *vt_esc_scan (vwm_frame *, vt_string *, int);

private void vt_frame_esc_set (vwm_frame *frame) {
  frame->process_char = vt_esc_scan;

  for (int i = 0; i < MAX_PARAMS; i++)
    frame->esc_param[i] = 0;

  frame->param_idx = 0;
  frame->cur_param = &frame->esc_param[frame->param_idx];
}

private void vt_frame_reset (vwm_frame *frame) {
  frame->row_pos = 1;
  frame->col_pos = 1;
  frame->saved_row_pos = 1;
  frame->saved_col_pos = 1;
  frame->scroll_first_row = 1;
  frame->last_row = frame->num_rows;
  frame->key_state = norm;
  frame->textattr = NORMAL;
  frame->saved_textattr = NORMAL;
  frame->charset[G0] = US_CHARSET;
  frame->charset[G1] = US_CHARSET;
  vt_frame_esc_set (frame);
}

private vt_string *vt_esc_brace_q (vwm_frame *frame, vt_string *buf, int c) {
  if (ISDIGIT (c)) {
    *frame->cur_param *= 10;
    *frame->cur_param += (c - '0');
    return buf;
  }

  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032': vt_frame_esc_set (frame);
    return buf;

    case 'h': /* Set modes */
      switch (frame->esc_param[0]) {
        case 1: /* Cursorkeys in application mode */
          frame->key_state = appl;
          vt_keystate_print (buf, frame->key_state);
          break;

        case 7:
          VtString.append_with_len (buf, TERM_AUTOWRAP_ON, TERM_AUTOWRAP_ON_LEN);
          break;

        case 25:
          VtString.append_with_len (buf, TERM_CURSOR_SHOW, TERM_CURSOR_SHOW_LEN);
          break;

        case 47:
          VtString.append_with_len (buf, TERM_SCREEN_SAVE, TERM_SCREEN_SAVE_LEN);
          break;

        case 2: /* Set ansi mode */
        case 3: /* 132 char/row */
        case 4: /* Set jump scroll */
        case 5: /* Set reverse screen */
        case 6: /* Set relative coordinates */
        case 8: /* Set auto repeat on */
        default:
          frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

  case 'l': /* Reset modes */
    switch (frame->esc_param[0]) {
      case 1: /* Cursorkeys in normal mode */
        frame->key_state = norm;
        vt_keystate_print (buf, frame->key_state);
        break;

        case 7:
          VtString.append_with_len (buf, TERM_AUTOWRAP_OFF, TERM_AUTOWRAP_OFF_LEN);
          break;

      case 25:
        VtString.append_with_len (buf, TERM_CURSOR_HIDE, TERM_CURSOR_HIDE_LEN);
        break;

      case 47:
        VtString.append_with_len (buf, TERM_SCREEN_RESTORE, TERM_SCREEN_RESTORE_LEN);
        break;

      case 2: /* Set VT52 mode */
      case 3:
      case 4: /* Set smooth scroll */
      case 5: /* Set non-reversed (normal) screen */
      case 6: /* Set absolute coordinates */
      case 8: /* Set auto repeat off */
      default:
        frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
        break;
    }
    break;

    default:
      break;
   }

  vt_frame_esc_set (frame);
  return buf;
}

private vt_string *vt_esc_lparen (vwm_frame *frame, vt_string *buf, int c) {
  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      vt_frame_esc_set (frame);
      break;

    /* Select character sets */
    case 'A': /* UK as G0 */
      frame->charset[G0] = UK_CHARSET;
      vt_altcharset (buf, G0, UK_CHARSET);
      break;

    case 'B': /* US as G0 */
      frame->charset[G0] = US_CHARSET;
      vt_altcharset (buf, G0, US_CHARSET);
      break;

    case '0': /* Special character set as G0 */
      frame->charset[G0] = GRAPHICS;
      vt_altcharset (buf, G0, GRAPHICS);
      break;

    case '1': /* Alternate ROM as G0 */
    case '2': /* Alternate ROM special character set as G0 */
    default:
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

private vt_string *vt_esc_rparen (vwm_frame *frame, vt_string *buf, int c) {
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    /* Select character sets */
    case 'A':
      frame->charset[G1] = UK_CHARSET;
      vt_altcharset (buf, G1, UK_CHARSET);
      break;

    case 'B':
      frame->charset[G1] = US_CHARSET;
      vt_altcharset (buf, G1, US_CHARSET);
      break;

    case '0':
      frame->charset[G1] = GRAPHICS;
      vt_altcharset (buf, G1, GRAPHICS);
      break;

    case '1': /* Alternate ROM as G1 */
    case '2': /* Alternate ROM special character set as G1 */
    default:
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

private vt_string *vt_esc_pound (vwm_frame *frame, vt_string *buf, int c) {
  switch (c)   /* Line attributes not supported */ {
    case '3':  /* Double height (top half) */
    case '4':  /* Double height (bottom half) */
    case '5':  /* Single width, single height */
    case '6':  /* Double width */
    default:
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      vt_frame_esc_set (frame);
      break;
  }

  return buf;
}

private vt_string *vt_process_m (vwm_frame *frame, vt_string *buf, int c) {
  int idx;
  switch (c) {
    case 0: /* Turn all attributes off */
      frame->textattr = NORMAL;
      vt_attr_reset (buf);
      idx = frame->num_cols - frame->col_pos - 1;
      if (0 <= idx)
        for (int i = 0; i < idx; i++)
          frame->colors[frame->row_pos -1][frame->col_pos - 1 + i] = COLOR_FG_NORM;
      break;

    case 1:
      frame->textattr |= BOLD;
      vt_bold (buf, 1);
      break;

    case 2: /* Half brightness */
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;

    case 3:
      frame->textattr |= ITALIC;
      vt_italic (buf, 1);
      break;

    case 4:
      frame->textattr |= UNDERLINE;
      vt_underline (buf, 1);
      break;

    case 5:
      frame->textattr |= BLINK;
      vt_blink (buf, 1);
      break;

    case 7:
      frame->textattr |= REVERSE;
      vt_reverse (buf, 1);
      break;

    case 21: /* Normal brightness */
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;

    case 22:
      frame->textattr &= ~BOLD;
      vt_bold (buf, 0);
      break;

    case 23:
      frame->textattr &= ~ITALIC;
      vt_italic (buf, 0);
      break;

    case 24:
      frame->textattr &= ~UNDERLINE;
      vt_underline (buf, 0);
      break;

    case 25:
      frame->textattr &= ~BLINK;
      vt_blink (buf, 0);
      break;

    case 27:
      frame->textattr &= ~REVERSE;
      vt_reverse (buf, 0);
      break;

    case 30 ... 37:
      vt_setfg (buf, c);
      idx = frame->num_cols - frame->col_pos + 1;
      if (0 < idx)
        for (int i = 0; i < idx; i++)
          frame->colors[frame->row_pos - 1][frame->col_pos - 1 + i] = c;

      break;

    case 40 ... 47:
      vt_setbg (buf, c);
      break;

    default: /* Unknown escape */
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  return buf;
}

private vt_string *vt_esc_brace (vwm_frame *frame, vt_string *buf, int c) {
  int
    i,
    newx,
    newy;

  char reply[128];

  if (ISDIGIT (c)) {
    *frame->cur_param *= 10;
    *frame->cur_param += (c - '0');
    return buf;
  }

   /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    case '?': /* Format should be \E[?<n> */
      if (*frame->cur_param) {
        vt_frame_esc_set (frame);
      } else {
        frame->process_char = vt_esc_brace_q;
      }

      return buf;

    case ';':
      if (++frame->param_idx < MAX_PARAMS)
        frame->cur_param = &frame->esc_param[frame->param_idx];
      return buf;

    case 'h': /* Set modes */
      switch (frame->esc_param[0]) {
        case 2:  /* Lock keyboard */
        case 4:  /* Character insert mode */
        case 12: /* Local echo on */
        case 20: /* <Return> = CR */
        default:
          frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'l': /* Reset modes */
      switch (frame->esc_param[0]) {
        case 2:  /* Unlock keyboard */
        case 4:  /* Character overstrike mode */
        case 12: /* Local echo off */
        case 20: /* <Return> = CR-LF */
        default:
          frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'r': /* Set scroll region */
      if (!frame->esc_param[0] and !frame->esc_param[1]) {
        frame->scroll_first_row = 1;
        frame->last_row = frame->num_rows;
      } else {
        /* Check parameters: VERY important. :) */
        if (frame->esc_param[0] < 1) /* Not needed */
          frame->scroll_first_row = 1;
        else
          frame->scroll_first_row = frame->esc_param[0];

        if (frame->esc_param[1] > frame->num_rows)
          frame->last_row = frame->num_rows;
        else
           frame->last_row = frame->esc_param[1];

        if (frame->scroll_first_row > frame->last_row) {
          /* Reset scroll region */
          frame->scroll_first_row = 1;
          frame->last_row = frame->num_rows;
        }
      }

      frame->row_pos = 1;
      frame->col_pos = 1;

      vt_setscroll (buf, frame->scroll_first_row + frame->first_row - 1,
        frame->last_row + frame->first_row - 1);
      vt_goto (buf, frame->row_pos + frame->first_row - 1, 1);
      break;

      case 'A': /* Cursor UP */
        if (frame->row_pos is frame->first_row)
          break;

        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        newx = (frame->row_pos - frame->esc_param[0]);

        if (newx > frame->scroll_first_row) {
          frame->row_pos = newx;
          vt_up (buf, frame->esc_param[0]);
        } else {
          frame->row_pos = frame->scroll_first_row;
          vt_goto (buf, frame->row_pos + frame->first_row - 1,
            frame->col_pos);
        }
        break;

      case 'B': /* Cursor DOWN */
        if (frame->row_pos is frame->last_row)
          break;

        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        newx = frame->row_pos + frame->esc_param[0];

        if (newx <= frame->last_row) {
          frame->row_pos = newx;
          vt_down (buf, frame->esc_param[0]);
        } else {
          frame->row_pos = frame->last_row;
          vt_goto (buf, frame->row_pos + frame->first_row - 1,
            frame->col_pos);
        }
        break;

      case 'C': /* Cursor RIGHT */
        if (frame->col_pos is frame->num_cols)
          break;

        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        newy = (frame->col_pos + frame->esc_param[0]);

        if (newy < frame->num_cols) {
          frame->col_pos = newy;

          vt_right (buf, frame->esc_param[0]);
        } else {
          frame->col_pos = frame->num_cols;
          vt_goto (buf, frame->row_pos + frame->first_row - 1,
            frame->col_pos);
        }
        break;

      case 'D': /* Cursor LEFT */
        if (frame->col_pos is 1)
          break;

        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        newy = (frame->col_pos - frame->esc_param[0]);

        if (newy > 1) {
          frame->col_pos = newy;
          vt_left (buf, frame->esc_param[0]);
        } else {
          frame->col_pos = 1;
          VtString.append (buf, "\r");
        }

        break;

      case 'f':
      case 'H': /* Move cursor to coordinates */
        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        ifnot (frame->esc_param[1])
          frame->esc_param[1] = 1;

        if ((frame->row_pos = frame->esc_param[0]) >
            frame->num_rows)
          frame->row_pos = frame->num_rows;

        if ((frame->col_pos = frame->esc_param[1]) >
            frame->num_cols)
          frame->col_pos = frame->num_cols;

        vt_goto (buf, frame->row_pos + frame->first_row - 1,
          frame->col_pos);
        break;

      case 'g': /* Clear tabstops */
        switch (frame->esc_param[0]) {
          case 0: /* Clear a tabstop */
            frame->tabstops[frame->col_pos-1] = 0;
            break;

          case 3: /* Clear all tabstops */
            for (newy = 0; newy < frame->num_cols; ++newy)
              frame->tabstops[newy] = 0;
            break;

          default:
            frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
            break;
        }
        break;

      case 'm': /* Set terminal attributes */
        vt_process_m (frame, buf, frame->esc_param[0]);
        for (i = 1; frame->esc_param[i] and i < MAX_PARAMS; i++)
          vt_process_m (frame, buf, frame->esc_param[i]);
        break;

      case 'J': /* Clear screen */
        switch (frame->esc_param[0]) {
          case 0: /* Clear from cursor down */
            vt_video_erase (frame, frame->row_pos,
              frame->num_rows, 1, frame->num_cols);

            newx = frame->row_pos;
            vt_savecursor (buf);
            VtString.append (buf, "\r");

            while (newx++ < frame->num_rows) {
              vt_clreol (buf);
              VtString.append (buf, "\n");
            }

            vt_clreol (buf);
            vt_restcursor (buf);
            break;

          case 1: /* Clear from cursor up */
            vt_video_erase (frame, 1, frame->row_pos,
              1, frame->num_cols);

            newx = frame->row_pos;
            vt_savecursor (buf);
            VtString.append (buf, "\r");

            while (--newx > 0) {
              vt_clreol (buf);
              vt_up (buf, 1);
            }

            vt_clreol (buf);
            vt_restcursor (buf);
            break;

          case 2: /* Clear whole screen */
            vt_video_erase (frame, 1, frame->num_rows,
              1, frame->num_cols);

            vt_goto (buf, frame->first_row + 1 - 1, 1);
            frame->row_pos = 1;
            frame->col_pos = 1;
            newx = frame->row_pos;
            vt_savecursor (buf);
            VtString.append (buf, "\r");

            while (newx++ < frame->num_rows) {
              vt_clreol (buf);
              VtString.append (buf, "\n");
            }

            vt_clreol (buf);
            vt_restcursor (buf);
            break;

          default:
            frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
            break;
        }
        break;

      case 'K': /* Clear line */
        switch (frame->esc_param[0]) {
          case 0: /* Clear to end of line */
            vt_video_erase (frame, frame->row_pos,
              frame->row_pos, frame->col_pos, frame->num_cols);

            vt_clreol (buf);
          break;

          case 1: /* Clear to beginning of line */
            vt_video_erase (frame, frame->row_pos,
              frame->row_pos, 1, frame->col_pos);

            vt_clrbgl (buf);
            break;

          case 2: /* Clear whole line */
            vt_video_erase (frame, frame->row_pos,
              frame->row_pos, 1, frame->num_cols);

            vt_clrline (buf);
            break;
          }
        break;

      case 'P': /* Delete under cursor */
        vt_video_erase (frame, frame->row_pos,
          frame->row_pos, frame->col_pos, frame->col_pos);

        vt_delunder (buf, frame->esc_param[0]);
        break;

      case 'M': /* Delete lines */
        vt_video_scroll_back (frame, 1);
        vt_delline (buf, frame->esc_param[0]);
        break;

      case 'L': /* Insert lines */
        vt_insline (buf, frame->esc_param[0]);
        break;

      case '@': /* Insert characters */
        ifnot (frame->esc_param[0])
          frame->esc_param[0] = 1;

        vt_insertchar (buf, frame->esc_param[0]);
        vt_frame_video_rshift (frame, frame->esc_param[0]);
        break;

      case 'i': /* Printing */
        frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
        break;

      case 'n': /* Device status request */
        switch (frame->esc_param[0]) {
          case 5: /* Status report request */
            /* Say we're just fine. */
            write (frame->fd, "\033[0n", 4);
            break;

          case 6: /* Cursor position request */
            sprintf (reply, "\033[%d;%dR", frame->row_pos,
                frame->col_pos);

            write (frame->fd, reply, bytelen (reply));
            break;
          }
          break;

      case 'c': /* Request terminal identification string */
        /* Respond with "I am a vt102" */
        write (frame->fd, "\033[?6c", 5);
        break;

      default:
        frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
        break;
    }

  vt_frame_esc_set (frame);
  return buf;
}

private vt_string *vt_esc_e (vwm_frame *frame, vt_string *buf, int c) {
  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    case '[':
      frame->process_char = vt_esc_brace;
      return buf;

    case '(':
      frame->process_char = vt_esc_lparen;
      return buf;

    case ')':
      frame->process_char = vt_esc_rparen;
      return buf;

    case '#':
      frame->process_char = vt_esc_pound;
      return buf;

    case 'D': /* Cursor down with scroll up at margin */
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_video_scroll (frame, 1);

      VtString.append (buf, "\n");
      break;

    case 'M': /* Reverse scroll (move up; scroll at top) */
      if (frame->row_pos > frame->scroll_first_row)
        --frame->row_pos;
      else
        vt_video_scroll_back (frame, 1);

      vt_revscroll (buf);
      break;

    case 'E': /* Next line (CR-LF) */
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_video_scroll (frame, 1);

      frame->col_pos = 1;
      VtString.append (buf, "\r\n");
      break;

    case '7': /* Save cursor and attribute */
      frame->saved_row_pos = frame->row_pos;
      frame->saved_col_pos = frame->col_pos;
      frame->saved_textattr = frame->textattr;
      break;

    case '8': /* Restore saved cursor and attribute */
      frame->row_pos = frame->saved_row_pos;
      frame->col_pos = frame->col_pos;
      if (frame->row_pos > frame->num_rows)
        frame->row_pos = frame->num_rows;

      if (frame->col_pos > frame->num_cols)
        frame->col_pos = frame->num_cols;

      vt_goto (buf, frame->row_pos + frame->first_row - 1,
        frame->col_pos);

      frame->textattr = frame->saved_textattr;
      vt_frame_attr_set (frame, buf);
      break;

    case '=': /* Set application keypad mode */
      frame->key_state = appl;
      vt_keystate_print (buf, frame->key_state);
      break;

    case '>': /* Set numeric keypad mode */
      frame->key_state = norm;
      vt_keystate_print (buf, frame->key_state);
      break;

    case 'N': /* Select charset G2 for one character */
    case 'O': /* Select charset G3 for one character */
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;

    case 'H': /* Set horizontal tab */
      frame->tabstops[frame->col_pos - 1] = 1;
      break;

    case 'Z': /* Request terminal identification string */
      /* Respond with "I am a vt102" */
      write (frame->fd, "\033[?6c", 5);
      break;

    case 'c': /* Terminal reset */
      vt_frame_reset (frame);
      break;

    default:
      frame->unimplemented (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

private vt_string *vt_esc_scan (vwm_frame *frame, vt_string *buf, int c) {
  switch (c) {
    case '\000': /* NULL (fill character) */
      break;

    case '\003': /* EXT  (half duplex turnaround) */
    case '\004': /* EOT  (can be disconnect char) */
    case '\005': /* ENQ  (generate answerback) */
      VtString.append_byte (buf, c);
      break;

    case '\007': /* BEL  (sound terminal bell) */
      vt_bell (buf);
      break;

    case '\b': /* Backspace; move left one character */
      ifnot (1 is frame->col_pos) {
        --frame->col_pos;
        vt_left (buf, frame->esc_param[0]);
      }
      break;

    case '\t': /* Tab.  Handle with direct motion (Buggy) */
      {
        int i = frame->col_pos;
        do {
          ++frame->col_pos;
        } while (0 is frame->tabstops[frame->col_pos - 1]
            and (frame->col_pos < frame->num_cols));

        vt_right (buf, frame->col_pos - i);
      }
      break;

    case '\013': /* Processed as linefeeds */
    case '\014': /* Don't let the cursor move below winow or scrolling region */
    case '\n':
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_video_scroll (frame, 1);

      VtString.append (buf, "\n");
      break;

    case '\r': /* Move cursor to left margin */
      frame->col_pos = 1;
      VtString.append (buf, "\r");
      break;

    case '\016': /* S0 (selects G1 charset) */
    case '\017': /* S1 (selects G0 charset) */
    case '\021': /* XON (continue transmission) */
    case '\022': /* XOFF (stop transmission) */
      VtString.append_byte (buf, c);
      break;

    case '\030': /* Processed as escape cancel */
    case '\032':
      vt_frame_esc_set (frame);
      break;

    case '\033':
      frame->process_char = vt_esc_e;
      break;

    default:
      if (c >= 0x80 or frame->mb_len) {
        if (frame->mb_len > 0) {
          frame->mb_buf[frame->mb_curlen++] = c;
          frame->mb_code <<= 6;
          frame->mb_code += c;

          if (frame->mb_curlen isnot frame->mb_len)
            return buf;

          frame->mb_code -= offsetsFromUTF8[frame->mb_len-1];

          vt_append (frame, buf, frame->mb_code);
          frame->mb_buf[0] = '\0';
          frame->mb_curlen = frame->mb_len = frame->mb_code = 0;
          return buf;
        } else {
          frame->mb_code = c;
          frame->mb_len = ({
            uchar uc = 0;
            if ((c & 0xe0) is 0xc0)
              uc = 2;
            else if ((c & 0xf0) is 0xe0)
              uc = 3;
            else if ((c & 0xf8) is 0xf0)
              uc = 4;
            else
              uc = -1;

            uc;
            });
          frame->mb_buf[0] = c;
          frame->mb_curlen = 1;
          return buf;
        }
      } else
        vt_append (frame, buf, c);
  }

  return buf;
}

private void vwm_win_set_frame (vwm_win *this, vwm_frame *frame) {
  VtString.clear (frame->output);

  vt_setscroll (frame->output, frame->scroll_first_row + frame->first_row - 1,
      frame->last_row + frame->first_row - 1);

  if (this->draw_separators) {
    this->draw_separators = 0;
    VtString.append_with_len (frame->output, this->separators_buf->bytes, this->separators_buf->num_bytes);
  }

  vt_goto (frame->output, frame->row_pos + frame->first_row - 1, frame->col_pos);

  ifnot (NULL is this->last_frame) {
    ifnot (frame->key_state is this->last_frame->key_state)
      vt_keystate_print (frame->output, frame->key_state);

    ifnot (frame->textattr is this->last_frame->textattr)
      vt_attr_set (frame->output, frame->textattr);

    for (int i = 0; i < NCHARSETS; i++)
      if (frame->charset[i] isnot this->last_frame->charset[i])
        vt_altcharset (frame->output, i, frame->charset[i]);
  }

  vt_write (frame->output->bytes, stdout);
}

private void vwm_frame_set_by_idx (vwm_win *this, int idx) {
  if (idx isnot this->cur_idx) {
    this->last_frame = this->current;
    DListSetCurrent (this, idx);
  }

  vwm_frame *frame = this->current;
  vwm_win_set_frame (this, frame);
}

private void vt_process_output (vwm_frame *frame, char *buf, int len) {
  VtString.clear (frame->output);

  while (len--)
    frame->process_char (frame, frame->output, (uchar) *buf++);

  vt_write (frame->output->bytes, stdout);
}

private void vwm_frame_set_argv (vwm_frame *this, int argc, char **argv) {
  if (argc >= MAX_ARGS - 1) argc = MAX_ARGS - 1;
  for (int i = 0; i < argc; i++)
    this->argv[i] = argv[i];
  this->argv[argc] = NULL;
}

private void vwm_frame_set_fd (vwm_frame *this, int fd) {
  this->fd = fd;
}

private int vwm_frame_get_fd (vwm_frame *this) {
  return this->fd;
}

private void vwm_frame_kill_proc (vwm_frame *frame) {
  if (frame->pid is -1) return;

  kill (frame->pid, SIGHUP);
  waitpid (frame->pid, NULL, 0);
  frame->pid = -1;
  frame->fd = -1;
}

private void vwm_frame_check_pid (vwm_frame *frame) {
  ifnot (0 is waitpid (frame->pid, &frame->status, WNOHANG)) {
    frame->pid = -1;
    frame->fd = -1;
  }
}

private void vwm_frame_set_unimplemented (vwm_frame *this, Unimplemented cb) {
  this->unimplemented = cb;
}

private int vwm_frame_set_log (vwm_frame *this, char *fname, int remove_log) {
  if (NULL is fname) {
    tmpname_t t = tmpfname (this->root->prop->tmpdir, "libvwm");
    if (-1 is t.fd)
      return NOTOK;

    this->logfd = t.fd;
    this->logfile = strdup (t.fname);
    this->remove_log = remove_log;
    return this->logfd;
  }

  this->logfd = open (fname, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);

  if (this->logfd is NOTOK) return NOTOK;

  if (-1 is fchmod (this->logfd, 0600)) return NOTOK;

  this->logfile = strdup (fname);
  this->remove_log = remove_log;
  return this->logfd;
}

FILE *UNFP = NULL;
private void def_unimplemented (vwm_frame *this, const char *fun, int c, int param) {
  (void) this;
  if (NULL is UNFP)
    UNFP = fopen ("/tmp/unimplemented_seq", "w");

  fprintf (UNFP, "|%s| %c %d| param: %d\n", fun, c, c, param);
  fflush (UNFP);
}

private vwm_frame *vwm_win_frame_new (vwm_win *this, int rows, int first_row) {
  vwm_frame *frame = Alloc (sizeof (vwm_frame));
  DListAppend (this, frame);

  frame->pid = -1;
  frame->argv[0] = NULL;
  frame->logfd = -1;
  frame->logfile = NULL;
  frame->remove_log = 0;
  frame->num_rows = rows;
  frame->num_cols = this->num_cols;
  frame->first_row = first_row;
  frame->first_col = this->first_col;
  frame->mb_buf[0] = '\0';
  frame->mb_curlen = frame->mb_len = frame->mb_code = 0;
  frame->output = vt_string_new (2048);

  frame->videomem = vwm_alloc_ints (frame->num_rows, frame->num_cols, 0);
  frame->colors = vwm_alloc_ints (frame->num_rows, frame->num_cols, COLOR_FG_NORMAL);
  frame->esc_param = Alloc (sizeof (int) * MAX_PARAMS);
  for (int i = 0; i < MAX_PARAMS; i++) frame->esc_param[i] = 0;
  frame->tabstops = Alloc (sizeof (int) * frame->num_cols);
  for (int i = 0; i < frame->num_cols; i++) {
    ifnot ((int) i % TABWIDTH)
      frame->tabstops[i] = 1;
    else
      frame->tabstops[i] = 0;
  }

  vt_frame_reset (frame);

  frame->unimplemented = def_unimplemented;

  frame->parent = this;
  frame->root = this->parent;

  return frame;
}

private vwm_frame *vwm_win_add_frame (vwm_t *this, vwm_win *win, int argc, char **argv, int draw) {
  if (win->length is win->max_frames) return NULL;

  win->num_separators = win->length;
  int frame_rows = 0;
  int mod = my.win.frame_rows (win, win->length + 1, &frame_rows);
  int
    num_rows = frame_rows + mod,
    first_row = win->first_row;

  vwm_frame *frame = win->head;
  while (frame) {
    frame->new_rows = num_rows;
    first_row += num_rows + 1;
    num_rows = frame_rows;
    frame = frame->next;
  }

  frame = my.frame.new (win, num_rows, first_row);
  frame->new_rows = num_rows;

  if (NULL isnot argv and NULL isnot argv[0]) {
    my.frame.set.argv (frame, argc, argv);
    my.frame.fork (this, frame);
  }

  DListSetCurrent (win, win->length - 1);
  vwm_win_on_resize (win, draw);
  return frame;
}

private int vwm_win_delete_frame (vwm_t *this, vwm_win *win, vwm_frame *frame, int draw) {
  ifnot (win->length)
    return OK;

  win->num_separators--;

  if (1 is win->length) {
    my.frame.kill_proc (win->head);
    my.frame.release (win, 0);
  } else {
    int is_last_frame = win->last_frame is frame;
    int idx = DListGetIdx (win, vwm_frame, frame);
    my.frame.kill_proc (frame);
    my.frame.release (win, idx);

    int frame_rows = 0;
    int mod = my.win.frame_rows (win, win->length, &frame_rows);
    int
      num_rows = frame_rows + mod,
      first_row = win->first_row;

    frame = win->head;
    while (frame) {
      frame->new_rows = num_rows;
      first_row += num_rows + 1;
      num_rows = frame_rows;
      frame = frame->next;
    }

    if (is_last_frame)
      win->last_frame = win->current;

    vwm_win_on_resize (win, draw);
  }

  return OK;
}

private void vwm_win_frame_release (vwm_win *this, int idx) {
  vwm_frame *frame = DListPopAt (this, vwm_frame, idx);

  if (NULL is frame) return;

  ifnot (NULL is frame->logfile) {
    if (frame->remove_log)
      unlink (frame->logfile);

    free (frame->logfile);
  }

  for (int i = 0; i < frame->num_rows; i++)
    free (frame->videomem[i]);
  free (frame->videomem);

  for (int i = 0; i < frame->num_rows; i++)
    free (frame->colors[i]);
  free (frame->colors);

  free (frame->tabstops);
  free (frame->esc_param);

  vt_string_release (frame->output);

  free (frame);
}

private void vwm_frame_on_resize (vwm_frame *this, int rows, int cols) {
  int **videomem = vwm_alloc_ints (rows, cols, 0);
  int **colors = vwm_alloc_ints (rows, cols, COLOR_FG_NORMAL);
  int row_pos = 0;
  int i, j, nj, ni;

  for (i = this->num_rows, ni = rows; i and ni; i--, ni--) {
    if (this->row_pos is i)
      if ((row_pos = i + rows - this->num_rows) < 1)
        row_pos = 1;

    for (j = 0, nj = 0; (j < this->num_cols) and (nj < cols); j++, nj++) {
      videomem[ni-1][nj] = this->videomem[i-1][j];
      colors[ni-1][nj] = this->colors[i-1][j];
    }
  }

  ifnot (row_pos) /* We never reached the old cursor */
    row_pos = 1;

  this->row_pos = row_pos;
  this->col_pos = (this->col_pos > cols ? cols : this->col_pos);

  for (i = 0; i < this->num_rows; i++)
    free (this->videomem[i]);
  free (this->videomem);

  for (i = 0; i < this->num_rows; i++)
    free (this->colors[i]);
  free (this->colors);

  this->videomem = videomem;
  this->colors = colors;
}

private void vwm_make_separator (vt_string *render, char *color, int cells, int row, int col) {
  vt_goto (render, row, col);
  VtString.append (render, color);
  for (int i = 0; i < cells; i++)
    VtString.append_with_len (render, "—", 3);

  vt_attr_reset (render);
}

private int vwm_win_set_separators (vwm_win *this, int draw) {
  ifnot (this->num_separators) return NOTOK;

  VtString.clear (this->separators_buf);

  vwm_frame *frame = this->head->next;
  for (int i = 0; i < this->num_separators; i++) {
    vwm_make_separator (this->separators_buf,
       (frame->prev is this->current ? COLOR_FOCUS : COLOR_UNFOCUS),
        frame->num_cols, frame->prev->first_row + frame->prev->last_row, frame->first_col);

    frame = frame->next;
  }

  if (DRAW is draw)
    vt_write (this->separators_buf->bytes, stdout);

  return OK;
}

private void vwm_win_draw (vwm_win *this) {
  char buf[8];

  int
    len = 0,
    oldattr = 0,
    oldclr = COLOR_FG_NORM,
    clr = COLOR_FG_NORM;

  uchar on = NORMAL;
  utf8 chr = 0;

  vt_string *render = this->render;
  VtString.clear (render);
  VtString.append (render, TERM_SCREEN_CLEAR);
  vt_setscroll (render, 0, 0);
  vt_attr_reset (render);
  vt_setbg (render, COLOR_BG_NORM);
  vt_setfg (render, COLOR_FG_NORM);

  vwm_frame *frame = this->head;
  while (frame) {
    vt_goto (render, frame->first_row, 1);

    for (int i = 0; i < frame->num_rows; i++) {
      for (int j = 0; j < frame->num_cols; j++) {
        chr = frame->videomem[i][j];
        clr = frame->colors[i][j];

        ifnot (clr is oldclr) {
          vt_setfg (render, clr);
          oldclr = clr;
        }

        if (chr & 0xFF) {
          vt_attr_check (render, chr & 0xFF, oldattr, &on);
          oldattr = (chr & 0xFF);

          if (oldattr >= 0x80) {
            ustring_character (chr, buf, &len);
            VtString.append_with_len (render, buf, len);
          } else
            VtString.append_byte (render, chr & 0xFF);
        } else {
          oldattr = 0;

          ifnot (on is NORMAL) {
            vt_attr_reset (render);
            on = NORMAL;
          }

          VtString.append_byte (render, ' ');
        }
      }

      VtString.append (render, "\r\n");
    }

    // clear the last newline, otherwise it scrolls one line more
    VtString.clear_at (render, -1); // this is visible when there is one frame

    /*  un-needed?
    vt_setscroll (render, frame->scroll_first_row + frame->first_row - 1,
        frame->last_row + frame->first_row - 1);
    vt_goto (render, frame->row_pos + frame->first_row - 1, frame->col_pos);
    */

    frame = frame->next;
  }

  vwm_win_set_separators (this, DONOT_DRAW);
  VtString.append_with_len (render, this->separators_buf->bytes, this->separators_buf->num_bytes);

  frame = this->current;
  vt_goto (render, frame->row_pos + frame->first_row - 1, frame->col_pos);

  vt_write (render->bytes, stdout);
}

private void vwm_win_on_resize (vwm_win *win, int draw) {
  int frow = 1;
  vwm_frame *it = win->head;
  while (it) {
    vwm_frame_on_resize (it, it->new_rows, it->num_cols);
    it->num_rows = it->new_rows;
    it->last_row = it->num_rows;
    it->first_row = frow;
    frow += it->num_rows + 1;
    if (it->argv[0] and it->pid isnot -1) {
      struct winsize ws = {.ws_row = it->num_rows, .ws_col = it->num_cols};
      ioctl (it->fd, TIOCSWINSZ, &ws);
      kill (it->pid, SIGWINCH);
    }

    it = it->next;
  }

  if (draw)
    vwm_win_draw (win);
}

private int vwm_win_min_rows (vwm_win *win) {
  return win->num_rows - (win->length - win->num_separators - (win->length * 2));
}

private void vwm_win_frame_increase_size (vwm_win *win, vwm_frame *frame, int param, int draw) {
  if (win->length is 1)
    return;

  int min_rows = vwm_win_min_rows (win);

  ifnot (param) param = 1;
  if (param > min_rows) param = min_rows;

  int num_lines;
  int mod;

  if (param is 1 or param is win->length - 1) {
    num_lines = 1;
    mod = 0;
  } else if (param > win->length - 1) {
    num_lines = param / (win->length - 1);
    mod = param % (win->length - 1);
  } else {
    num_lines = (win->length - 1) / param;
    mod = (win->length - 1) % param;
  }

  int orig_param = param;

  vwm_frame *fr = win->head;
  while (fr) {
    fr->new_rows = fr->num_rows;
    fr = fr->next;
  }

  fr = win->head;
  int iter = 1;
  while (fr and param and iter++ < ((win->length - 1) * 3)) {
    if (frame is fr)
      goto next_frame;

    int num = num_lines;
    while (fr->new_rows > 2 and num--) {
      fr->new_rows = fr->new_rows - 1;
      param--;
    }

    if (fr->new_rows is 2)
      goto next_frame;

    if (mod) {
      fr->new_rows--;
      param--;
      mod--;
    }

    next_frame:
      if (fr->next is NULL)
        fr = win->head;
      else
        fr = fr->next;
  }

  frame->new_rows = frame->new_rows + (orig_param - param);
  vwm_win_on_resize (win, draw);
}

private void vwm_win_frame_decrease_size (vwm_win *win, vwm_frame *frame, int param, int draw) {
  if (1 is win->length or frame->num_rows <= MIN_ROWS)
    return;

  if (frame->num_rows - param <= 2)
    param = frame->num_rows - 2;

  if (0 >= param)
    param = 1;

  int num_lines;
  int mod;
  if (param is 1 or param is win->length - 1) {
    num_lines = 1;
    mod = 0;
  } else if (param > win->length - 1) {
    num_lines = param / (win->length - 1);
    mod = param % (win->length - 1);
  } else {
    num_lines = (win->length - 1) / param;
    mod = (win->length - 1) % param;
  }

  vwm_frame *fr = win->head;
  while (fr) {
    fr->new_rows = fr->num_rows;
    fr = fr->next;
  }

  int orig_param = param;

  fr = win->head;
  int iter = 1;
  while (fr and param and iter++ < ((win->length - 1) * 3)) {
    if (frame is fr)
      goto next_frame;

    fr->new_rows = fr->num_rows + num_lines;

    param -= num_lines;

    if (mod) {
      fr->new_rows++;
      param--;
      mod--;
    }

  next_frame:
    if (fr->next is NULL)
      fr = win->head;
    else
      fr = fr->next;
  }

  frame->new_rows = frame->new_rows - (orig_param - param);
  vwm_win_on_resize (win, draw);
}

private void vwm_win_frame_set_size (vwm_win *win, vwm_frame *frame, int param, int draw) {
  if (param is frame->num_rows or 0 >= param)
    return;

  if (param > frame->num_rows)
    vwm_win_frame_increase_size (win, frame, param - frame->num_rows, draw);
  else
    vwm_win_frame_decrease_size (win, frame, frame->num_rows - param, draw);
}

private void vwm_win_frame_change (vwm_win *win, vwm_frame *frame, int dir, int draw) {
  if (NULL is frame->next and NULL is frame->prev)
    return;

  int idx = -1;

  if (dir is DOWN_POS) {
    ifnot (NULL is frame->next)
      idx = DListGetIdx (win, vwm_frame, frame->next);
    else
      idx = 0;
  } else {
    ifnot (NULL is frame->prev)
      idx = DListGetIdx (win, vwm_frame, frame->prev);
    else
      idx = win->length - 1;
  }

  win->last_frame = frame;
  DListSetCurrent (win, idx);
  if (OK is vwm_win_set_separators (win, draw))
    if (draw is DONOT_DRAW)
      win->draw_separators = 1;
}

private void vwm_win_change (vwm_t *this, vwm_win *win, int dir, int draw) {
  if ($my(length) is 1)
    return;

  int idx = -1;
  switch (dir) {
    case LAST_POS:
      if ($my(last_win) is win)
        return;

      idx = DListGetIdx ($myprop, vwm_win, $my(last_win));
      break;

    case NEXT_POS:
      ifnot (NULL is win->next)
        idx = DListGetIdx ($myprop, vwm_win, win->next);
      else
        idx = 0;
      break;

    case PREV_POS:
      ifnot (NULL is win->prev)
        idx = DListGetIdx ($myprop, vwm_win, win->prev);
      else
        idx = $my(length) - 1;
      break;

    default:
      return;
  }

  $my(last_win) = win;
  DListSetCurrent ($myprop, idx);
  my.term.screen.clear ($my(term));
  win = $my(current);

  if (draw) {
    if (win->is_initialized)
      my.win.draw (win);
    else
      vwm_win_set_separators (win, DRAW);
  } else
    vwm_win_set_separators (win, DONOT_DRAW);
}

private int vwm_win_frame_edit_log (vwm_t *this, vwm_win *win, vwm_frame *frame) {
  if (frame->logfile is NULL)
    return NOTOK;

  char *argv[] = {$my(editor)->bytes, frame->logfile, NULL};
  int len;

  for (int i = 0; i < frame->num_rows; i++) {
    char buf[(frame->num_cols * 3) + 2];
    len = vt_video_line_to_str (frame->videomem[i], buf,
    frame->num_cols);
    write (frame->logfd, buf, len);
  }

  vwm_spawn (this, argv);
  vt_video_add_log_lines (frame);
  my.win.draw (win);
  return OK;
}

private char *vwm_name_gen (int *name_gen, char *prefix, size_t prelen) {
  size_t num = (*name_gen / 26) + prelen;
  char *name = Alloc (num * sizeof (char *) + 1);
  size_t i = 0;
  for (; i < prelen; i++) name[i] = prefix[i];
  for (; i < num; i++) name[i] = 'a' + ((*name_gen)++ % 26);
  name[num] = '\0';
  return name;
}

private vwm_frame *vwm_win_get_frame_at (vwm_win *this, int idx) {
  return DListGetAt (this, vwm_frame, idx);
}

private int vwm_win_frame_rows (vwm_win *this, int num_frames, int *frame_rows) {
  int avail_rows = this->num_rows - this->num_separators;
  *frame_rows = avail_rows / num_frames;
  return avail_rows % num_frames;
}

private vwm_win *vwm_win_new (vwm_t *this, char *name, win_opts opts) {
  vwm_win *win = Alloc (sizeof (vwm_win));
  DListAppend ($myprop, win);

  win->parent = this;

  if (NULL is name)
    win->name = vwm_name_gen (&$my(name_gen), "win:", 4);
  else
    win->name = strdup (name);

  int num_frames = opts.num_frames;

  win->max_frames = opts.max_frames;
  win->first_row = opts.first_row;
  win->first_col = opts.first_col;
  win->num_rows = opts.rows;
  win->num_cols = opts.cols;

  if (num_frames > win->max_frames) num_frames = win->max_frames;
  if (num_frames < 0) num_frames = 1;

  if (win->num_rows >= $my(num_rows))
    win->num_rows = $my(num_rows);

  win->num_separators = num_frames - 1;
  win->separators_buf = VtString.new ((win->num_rows * win->num_cols) + 32);
  win->render = VtString.new (4096);
  win->last_row = win->num_rows;

  if (win->first_col <= 0) win->first_col = 1;
  if (win->first_row <= 0) win->first_row = 1;
  if (win->first_row >= win->num_rows - win->num_separators + num_frames)
    win->first_row = win->num_rows - win->num_separators - num_frames;

  win->cur_row = win->first_row;
  win->cur_col = win->first_col;

  win->length = 0;
  win->cur_idx = -1;
  win->head = win->current = win->tail = NULL;

  int frame_rows = 0;
  int mod = my.win.frame_rows (win, num_frames, &frame_rows);

  int
    num_rows = frame_rows + mod,
    first_row = win->first_row;

  for (int i = 0; i < num_frames; i++) {
    my.frame.new (win, num_rows, first_row);
    first_row += num_rows + 1;
    num_rows = frame_rows;
  }

  win->last_frame = win->head;

  if (opts.focus)
    DListSetCurrent ($myprop, $my(length) - 1);

  return win;
}

private void vwm_win_release (vwm_t *this, vwm_win *win) {
  int idx = DListGetIdx ($myprop, vwm_win, win);
  if (idx is INDEX_ERROR) return;
  vwm_win *w = DListPopAt ($myprop, vwm_win, idx);
  int len = w->length;
  for (int i = 0; i < len; i++)
    my.frame.release (w, 0);

  VtString.release (win->separators_buf);
  VtString.release (win->render);

  free (w->name);
  free (w);
}

private void fd_set_size (int fd, int rows, int cols) {
  struct winsize wsiz;
  wsiz.ws_row = rows;
  wsiz.ws_col = cols;
  wsiz.ws_xpixel = 0;
  wsiz.ws_ypixel = 0;
  ioctl (fd, TIOCSWINSZ, &wsiz);
}

private int vwm_spawn (vwm_t *this, char **argv) {
  int status = NOTOK;
  pid_t pid;

  my.term.orig_mode ($my(term));

  if (-1 is (pid = fork ())) goto theend;

  ifnot (pid) {
    char lrows[4], lcols[4];
    snprintf (lrows, 4, "%d", $my(num_rows));
    snprintf (lcols, 4, "%d", $my(num_cols));

    setenv ("TERM", $my(term)->name, 1);
    setenv ("LINES", lrows, 1);
    setenv ("COLUMNS", lcols, 1);
    execvp (argv[0], argv);
    fprintf (stderr, "execvp failed\n");
    _exit (1);
  }

  if (-1 is waitpid (pid, &status, 0)) {
    status = -1;
    goto theend;
  }

  ifnot (WIFEXITED (status)) {
    status = -1;
    fprintf (stderr, "Failed to invoke %s\n", argv[0]);
    goto theend;
  }

  ifnot (status is WEXITSTATUS (status))
    fprintf (stderr, "Proc %s terminated with exit status: %d", argv[0], status);

theend:
  my.term.raw_mode ($my(term));
  return status;
}

private pid_t vwm_frame_fork (vwm_t *this, vwm_frame *frame) {
  if (frame->pid isnot -1)
    return frame->pid;

  signal (SIGWINCH, SIG_IGN);

  frame->pid = -1;
  frame->fd = -1;
  int fd =  -1;
  if (-1 is (fd = posix_openpt (O_RDWR|O_NOCTTY|O_CLOEXEC))) goto theerror;
  if (-1 is grantpt (fd)) goto theerror;
  if (-1 is unlockpt (fd)) goto theerror;
  char *name = ptsname (fd); ifnull (name) goto theerror;
  strncpy (frame->tty_name, name, MAX_TTYNAME - 1);
  if (-1 is (frame->pid = fork ())) goto theerror;

  ifnot (frame->pid) {
    int slave_fd;

    setsid ();

    if (-1 is (slave_fd = open (frame->tty_name, O_RDWR|O_CLOEXEC|O_NOCTTY)))
      goto theerror;

    ioctl (slave_fd, TIOCSCTTY, 0);

    dup2 (slave_fd, 0);
    dup2 (slave_fd, 1);
    dup2 (slave_fd, 2);

    fd_set_size (slave_fd, frame->num_rows, frame->num_cols);

    close (slave_fd);
    close (fd);

    int maxfd;
#ifdef OPEN_MAX
    maxfd = OPEN_MAX;
#else
    maxfd = sysconf (_SC_OPEN_MAX);
#endif
    for (int fda = 3; fda < maxfd; fda++)
      if (close (fda) is -1 and errno is EBADF)
        break;

    sigset_t emptyset;
    sigemptyset (&emptyset);
    sigprocmask (SIG_SETMASK, &emptyset, NULL);

    char rows[4]; snprintf (rows, 4, "%d", frame->num_rows);
    char cols[4]; snprintf (cols, 4, "%d", frame->num_cols);
    setenv ("TERM",  $my(term)->name, 1);
    setenv ("LINES", rows, 1);
    setenv ("COLUMNS", cols, 1);

    execvp (frame->argv[0], frame->argv);
    fprintf (stderr, "execvp() failed for command: '%s'\n", frame->argv[0]);
    _exit (1);
  }

  frame->fd = fd;
  signal (SIGWINCH, vwm_sigwinch_handler);
  return frame->pid;

theerror:
  ifnot (-1 is frame->pid)
    waitpid (frame->pid, NULL, 0);

  ifnot (-1 is fd) close (fd);

  signal (SIGWINCH, vwm_sigwinch_handler);
  return frame->pid;
}

private void vwm_sigwinch_handler (int sig) {
  signal (sig, vwm_sigwinch_handler);
  vwm_t *this = VWM;
  $my(need_resize) = 1;
}

private void vwm_handle_sigwinch (vwm_t *this) {
  int rows; int cols;
  my.term.init_size ($my(term), &rows, &cols);
  my.set.size (this, rows, cols, 1);

  vwm_win *win = $my(head);
  while (win) {
    win->num_rows = $my(num_rows);
    win->num_cols = $my(num_cols);
    win->last_row = win->num_rows;

    int frame_rows = 0;
    int mod = my.win.frame_rows (win, win->length, &frame_rows);

    int
      num_rows = frame_rows + mod,
      first_row = win->first_row;

    vwm_frame *frame = win->head;
    while (frame) {
      vwm_frame_on_resize (frame, num_rows, win->num_cols);
      frame->first_row = first_row;
      frame->num_rows = num_rows;
      frame->last_row = frame->num_rows;
      first_row += num_rows + 1;
      num_rows = frame_rows;
      if (frame->argv[0] and frame->pid isnot -1) {
        struct winsize ws = {.ws_row = frame->num_rows, .ws_col = frame->num_cols};
        ioctl (frame->fd, TIOCSWINSZ, &ws);
        kill (frame->pid, SIGWINCH);
      }

      frame = frame->next;
    }
    win = win->next;
  }

  win = $my(current);

  my.win.draw (win);
  $my(need_resize) = 0;
}

private void vwm_exit_signal (int sig) {
  __deinit_vwm__ (&VWM);
  exit (sig);
}

private int vwm_main (vwm_t *this) {
  ifnot ($my(length)) return OK;
  if (NULL is $my(current))
    $my(current) = $my(head);

  setbuf (stdin, NULL);

  signal (SIGHUP,   vwm_exit_signal);
  signal (SIGINT,   vwm_exit_signal);
  signal (SIGQUIT,  vwm_exit_signal);
  signal (SIGTERM,  vwm_exit_signal);
  signal (SIGSEGV,  vwm_exit_signal);
  signal (SIGBUS,   vwm_exit_signal);
  signal (SIGWINCH, vwm_sigwinch_handler);

  fd_set read_mask;
  struct timeval *tv = NULL;

  char
    input_buf[MAX_CHAR_LEN],
    output_buf[BUFSIZE];

  int
    maxfd,
    numready,
    output_len,
    retval = NOTOK;

  vwm_win *win = $my(current);

  vwm_win_set_separators (win, DRAW);

  vwm_frame *frame = win->head;
  while (frame) {
    if (frame->argv[0] isnot NULL and frame->pid is -1)
      my.frame.fork (this, frame);

    frame = frame->next;
  }

  for (;;) {
    win = $my(current);
    if (NULL is win) {
      retval = OK;
      break;
    }

    ifnot (win->length) {
      if (1 isnot $my(length))
        my.win.change (this, win, PREV_POS, DRAW);

      my.win.release (this, win);
      continue;
    }

    if ($my(need_resize))
      vwm_handle_sigwinch (this);

    my.win.set.frame (win, win->current);

    maxfd = 1;

    FD_ZERO (&read_mask);
    FD_SET (STDIN_FILENO, &read_mask);

    frame = win->head;
    while (frame) {
      if (frame->pid isnot -1)
        vwm_frame_check_pid (frame);

      if (frame->fd isnot -1) {
        FD_SET (frame->fd, &read_mask);
        if (maxfd <= frame->fd)
          maxfd = frame->fd + 1;
      }

      frame = frame->next;
    }

    if (0 >= (numready = select (maxfd, &read_mask, NULL, NULL, tv))) {
      switch (errno) {
        case EIO:
        case EINTR:
        default:
          break;
      }

      continue;
    }

    frame = win->current;

    for (int i = 0; i < MAX_CHAR_LEN; i++) input_buf[i] = '\0';

    if (FD_ISSET (STDIN_FILENO, &read_mask)) {
      if (0 < fd_read (STDIN_FILENO, input_buf, 1))
        if (VWM_QUIT is my.process_input (this, win, frame, input_buf)) {
          retval = OK;
          break;
        }
    }

    win = $my(current);

    frame = win->head;
    while (frame) {
      if (frame->fd is -1)
        goto next_frame;

      output_buf[0] = '\0';
      if (FD_ISSET (frame->fd, &read_mask)) {
        if (0 > (output_len = read (frame->fd, output_buf, BUFSIZE))) {
          switch (errno) {
            case EIO:
            default:
              if (-1 isnot frame->pid)
                vwm_frame_check_pid (frame);
              goto next_frame;
          }
        }

        output_buf[output_len] = '\0';

        my.win.set.frame (win, frame);

        my.process_output (frame, output_buf, output_len);
      }

      next_frame:
        frame = frame->next;
    }

    win->is_initialized = 1;
  }

  ifnot (NULL is UNFP)
    fclose (UNFP);

  if (retval is 1 or retval is OK) return OK;

  return NOTOK;
}

private int vwm_default_on_tab_callback (vwm_t *this, vwm_win *win, vwm_frame *frame) {
  (void) this; (void) win; (void) frame;
  return OK;
}

private int vwm_process_input (vwm_t *this, vwm_win *win, vwm_frame *frame, char *input_buf) {
  if (input_buf[0] isnot MODE_KEY) {
    fd_write (frame->fd, input_buf, 1);
    return OK;
  }

  int param = 0;

  utf8 c;

getc_again:
  c = my.getkey (STDIN_FILENO);

  if (-1 is c) return OK;

  switch (c) {
    case ESCAPE_KEY:
      break;

    case MODE_KEY:
      input_buf[0] = MODE_KEY; input_buf[1] = '\0';
      fd_write (frame->fd, input_buf, 1);
      break;

    case '\t': {
        int retval = $my(on_tab_callback) (this, win, frame);
        if (retval is VWM_QUIT or ($my(state) & VWM_QUIT))
          return VWM_QUIT;
      }
      break;

    case 'q':
      return VWM_QUIT;

    case '!':
    case 'c':
      if (frame->pid isnot -1)
        break;

      if (c is '!') {
        frame->argv[0] = $my(shell)->bytes;
        frame->argv[1] = NULL;
      } else {
        if (NULL is frame->argv[0]) {
          frame->argv[0] = DEFAULT_APP;
          frame->argv[1] = NULL;
        }
      }

      my.frame.fork (this, frame);
      break;

    case 'n':
      ifnot (param)
        param = 1;
      else
        if (param > MAX_FRAMES)
          param = MAX_FRAMES;

      my.win.new (this, NULL, WinNewOpts (
          .rows = $my(num_rows),
          .cols = $my(num_cols),
          .num_frames = param,
          .max_frames = MAX_FRAMES));

      $my(last_win) = win;
      DListSetCurrent ($myprop, $my(length) - 1);
      win = $my(current);
      my.win.draw (win);

      break;

    case 'k':
      vwm_frame_kill_proc (frame);
      break;

    case 'd':
      my.win.delete_frame (this, win, frame, DRAW);
      break;

    case 's':
      my.win.add_frame (this, win, 0, NULL, DRAW);
      break;

    case 'S': {
        utf8 w = my.getkey (STDIN_FILENO);
        char *argv[] = {NULL, NULL};
        int argc = 1;

        switch (w) {
          case '!':
            argv[0] = $my(shell)->bytes;
            break;

          case 'c':
            argv[0] = DEFAULT_APP;
            break;

          case 'e':
            argv[0] = $my(editor)->bytes;
            break;

          default:
            break;
        }
        my.win.add_frame (this, win, argc, argv, DRAW);
      }

      break;

    case ARROW_DOWN_KEY:
    case ARROW_UP_KEY:
    case 'w':
      my.win.frame.change (win, frame, (c is 'w' or c is ARROW_DOWN_KEY) ? DOWN_POS : UP_POS, DONOT_DRAW);
      break;

    case ARROW_LEFT_KEY:
    case ARROW_RIGHT_KEY:
    case '`':
      my.win.change (this, win,
          (c is ARROW_RIGHT_KEY) ? NEXT_POS :
          (c is '`') ? LAST_POS : PREV_POS, DRAW);
      break;

    case PAGE_UP_KEY:
    case 'E':
      my.win.frame.edit_log (this, win, frame);
      break;

    case '+':
      my.win.frame.increase_size (win, frame, param, DRAW);
      break;

    case '-':
      my.win.frame.decrease_size (win, frame, param, DRAW);
      break;

    case '=':
      my.win.frame.set_size (win, frame, param, DRAW);
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      param *= 10;
      param += (c - '0');
      goto getc_again;
  }

  return OK;
}

mutable public void __alloc_error_handler__ (int err, size_t size,
                           char *file, const char *func, int line) {
  fprintf (stderr, "MEMORY_ALLOCATION_ERROR\n");
  fprintf (stderr, "File: %s\nFunction: %s\nLine: %d\n", file, func, line);
  fprintf (stderr, "Size: %zd\n", size);

  if (err is INTEGEROVERFLOW_ERROR)
    fprintf (stderr, "Error: Integer Overflow Error\n");
  else
    fprintf (stderr, "Error: Not Enouch Memory\n");

  __deinit_vwm__ (&VWM);

  exit (1);
}


public vwm_t *__init_vwm__ (void) {
  AllocErrorHandler = __alloc_error_handler__;
  VtString = __init_vt_string__ ();

  vwm_t *this = Alloc (sizeof (vwm_t));
  vwm_prop *prop = Alloc (sizeof (vwm_prop));

  *this =  (vwm_t) {
    .self = (vwm_self) {
      .main = vwm_main,
      .process_input = vwm_process_input,
      .process_output = vt_process_output,
      .getkey = getkey,
      .term = (vwm_term_self) {
        .new = vwm_term_new,
        .release = vwm_term_release,
        .raw_mode =  vwm_term_raw_mode,
        .sane_mode = vwm_term_sane_mode,
        .orig_mode = vwm_term_orig_mode,
        .init_size = vwm_term_init_size,
        .screen = (vwm_term_screen_self) {
          .save = vwm_term_screen_save,
          .clear = vwm_term_screen_clear,
          .restore = vwm_term_screen_restore
         }
      },
      .win = (vwm_win_self) {
        .new = vwm_win_new,
        .draw = vwm_win_draw,
        .change = vwm_win_change,
        .release = vwm_win_release,
        .add_frame = vwm_win_add_frame,
        .frame_rows = vwm_win_frame_rows,
        .delete_frame = vwm_win_delete_frame,
        .set = (vwm_win_set_self) {
          .frame = vwm_win_set_frame
        },
        .get = (vwm_win_get_self) {
          .frame_at = vwm_win_get_frame_at
        },
        .frame = (vwm_win_frame_self) {
          .change = vwm_win_frame_change,
          .edit_log = vwm_win_frame_edit_log,
          .set_size = vwm_win_frame_set_size,
          .increase_size = vwm_win_frame_increase_size,
          .decrease_size = vwm_win_frame_decrease_size
        }
      },
      .frame = (vwm_frame_self) {
        .new = vwm_win_frame_new,
        .fork = vwm_frame_fork,
        .release = vwm_win_frame_release,
        .kill_proc = vwm_frame_kill_proc,
        .get = (vwm_frame_get_self) {
          .fd = vwm_frame_get_fd
        },
        .set = (vwm_frame_set_self) {
          .fd = vwm_frame_set_fd,
          .log = vwm_frame_set_log,
          .argv = vwm_frame_set_argv,
          .unimplemented = vwm_frame_set_unimplemented
        }
      },
      .get = (vwm_get_self) {
        .term = vwm_get_term,
        .state = vwm_get_state,
        .lines = vwm_get_lines,
        .columns = vwm_get_columns,
        .current_win = vwm_get_current_win,
        .current_frame = vwm_get_current_frame
      },
      .set = (vwm_set_self) {
        .size = vwm_set_size,
        .state = vwm_set_state,
        .shell =  vwm_set_shell,
        .editor = vwm_set_editor,
        .tmpdir = vwm_set_tmpdir,
        .on_tab_callback = vwm_set_on_tab_callback
      }
    },
    .prop = prop
  };

  $my(tmpdir) = NULL;
  $my(editor) = VtString.new_with (EDITOR);
  $my(shell) = VtString.new_with (SHELL);
  $my(term) = my.term.new ();
  $my(length) = 0;
  $my(cur_idx) = -1;
  $my(head) = $my(tail) = $my(current) = NULL;
  $my(name_gen) = ('z' - 'a') + 1;

  my.set.on_tab_callback (this, vwm_default_on_tab_callback);
  my.set.tmpdir (this, NULL, 0);

  VWM = this;
  return this;
}

public void __deinit_vwm__ (vwm_t **thisp) {
  if (NULL == *thisp) return;
  vwm_t *this = *thisp;

  my.term.orig_mode ($my(term));
  my.term.release (&$my(term));

  vwm_win *win = $my(head);
  while (win) {
    vwm_win *tmp = win->next;
    my.win.release (this, win);
    win = tmp;
  }

  free ($my(tmpdir));

  VtString.release ($my(editor));
  VtString.release ($my(shell));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
