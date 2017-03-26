#include "screenhack.h"
#include "colors.h"
#include <math.h>
#include <float.h>

typedef struct _square {
  int x, y, h, s, v;
  int time, duration;
} square;

struct state {
  Display *dpy;
  Window window;
  Pixmap b, ba, bb;
  XColor bg;

  square *squares;
  int subdivisions, delay, duration, colorspeed, nsquares, spacing;
  int h, cc;
  int size, xsq, ysq;
  XWindowAttributes xgwa;
  GC gc;
};

static void
setup_window (void *closure)
{
  struct state *st = (struct state *) closure;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->size = floor(st->xgwa.height / st->subdivisions);
  st->ysq = st->subdivisions;
  st->xsq = floor(st->xgwa.width / st->size);

  st->ba = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
  st->bb = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
  st->b = st->ba;
}

static void
swap_buffer (void *closure)
{
  struct state *st = (struct state *) closure;

  XCopyArea (st->dpy, st->b, st->window, st->gc, 0, 0,
             st->xgwa.width, st->xgwa.height, 0, 0);
  st->b = (st->b == st->ba ? st->bb : st->ba);
}

static void
randomize_square (square *s, void *closure)
{
  int i, count;
  float r;
  struct state *st = (struct state *) closure;

  s->time = 0;
  r = ((float)(random() % (2<<8)) / (2<<8)) * M_PI / 2 - M_PI / 4;
  s->duration = floor(st->duration * cos(r));
  s->h = st->h;

  do {
    s->x = random() % st->xsq;
    s->y = random() % st->ysq;

    for (i = 0; i < st->nsquares; i++) {
      count = 0;
      if (&st->squares[i] != s && st->squares[i].x == s->x && st->squares[i].y == s->y) {
        count++;
      }
    }
  } while (count > 0);
}

static void *
fadesquares_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  int i;

  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Integer");
  st->colorspeed = get_integer_resource (st->dpy, "colorspeed", "Integer");
  st->nsquares = get_integer_resource(st->dpy, "numSquares", "Integer");
  st->subdivisions = get_integer_resource(st->dpy, "subdivisions", "Integer");
  st->spacing = get_integer_resource(st->dpy, "spacing", "Integer");

  st->squares = (square *) calloc (st->nsquares, sizeof(square));

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->bg.red = st->bg.green = st->bg.blue = 0;

  st->h = random() % 360;
  st->cc = 0;

  gcv.background = st->bg.pixel;
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);

  setup_window(st);
  for (i = 0; i < st->nsquares; i++) {
    square *s = (square *) &st->squares[i];
    randomize_square(s, st);
    s->time = random() % s->duration;
  }

  return st;
}

static unsigned long
fadesquares_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int k;
  float v;
  XColor c;

  XSetForeground (st->dpy, st->gc, st->bg.pixel);
  XFillRectangle (st->dpy, (st->b == st->ba ? st->bb : st->ba), st->gc,
                  0, 0, st->xgwa.width, st->xgwa.height);

  for (k = 0; k < st->nsquares; k++) {
    square *s = (square *) &st->squares[k];
    v = sin(M_PI * s->time / s->duration);

    hsv_to_rgb(s->h, 1.0, v, &c.red, &c.green, &c.blue);
    XAllocColor (st->dpy, st->xgwa.colormap, &c);
    XSetForeground (st->dpy, st->gc, c.pixel);
    XFillRectangle (st->dpy, st->b, st->gc,
                    (s->x * st->size + st->spacing),
                    (s->y * st->size + st->spacing),
                    st->size - st->spacing * 2, st->size - st->spacing * 2);
    s->time++;

    if (s->time >= s->duration) {
      randomize_square (s, st);
    }
  }

  if (++st->cc > st->colorspeed) {
    st->h = (st->h + 1) % (2 << 15);
    st->cc = 0;
  }

  XCopyArea (st->dpy, st->b, st->window, st->gc, 0, 0,
             st->xgwa.width, st->xgwa.height, 0, 0);
  st->b = (st->b == st->ba ? st->bb : st->ba);

  return st->delay;
}

static Bool
fadesquares_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
fadesquares_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st->squares);
  free (st);
}

static void
fadesquares_reshape (Display *dpy, Window window, void *closure,
                     unsigned int w, unsigned int h)
{
  setup_window(closure);
  swap_buffer(closure);
}

static const char *fadesquares_defaults [] = {
  "*delay: 25000",
  "*duration: 100",
  "*colorspeed: 3",
  "*numSquares: 36",
  "*subdivisions: 9",
  "*spacing: 2",
  0
};

static XrmOptionDescRec fadesquares_options [] = {
  { "-delay",     ".delay", XrmoptionSepArg, 0 },
  { "-duration",  ".duration", XrmoptionSepArg, 0 },
  { "-colorspeed", ".colorspeed", XrmoptionSepArg, 0 },
  { "-num-squares", ".numSquares", XrmoptionSepArg, 0 },
  { "-spacing", ".spacing", XrmoptionSepArg, 0 },
  { "-subdivisions", ".subdivisions", XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("FadeSquares", fadesquares)
