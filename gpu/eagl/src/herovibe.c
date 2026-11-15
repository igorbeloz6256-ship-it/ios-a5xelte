/*
 * Vibration on the Galaxy A50 (in libherosb, SpringBoard): the kernel runs the motor (sysctl kern.a50_vibrate = N ms,
 * bsd/dev/a50_vibrator.c). iOS 16 has no plain vibrator: the iPhone X's Taptic Engine is driven through the audio path
 * (hapticd, SystemSoundServer in mediaserverd), which this port does not have. So the requests are caught where
 * SpringBoard makes them (strings of this root's dyld cache):
 *  - ToneLibrary's alerts (notifications, messages...): +[TLAlertSystemSoundController
 *    _vibrationPatternForAlert:withSound:] gives the alert's pattern (VibePattern: on/off, ms, ...), which is played
 *    here; -[TLAlertController stopPlayingAlerts:withOptions:playbackCompletionType:], -[TLAlert stop] and
 *    -[TLAlert stopWithOptions:] end it;
 *  - UIKit's haptics (UIFeedbackGenerator, _UIFeedback): a short buzz each, at most one per 60 ms.
 * The methods are not replaced: herovibe_tramp.S calls herovibe_note() and goes on to Apple's own implementation with
 * the arguments untouched, whatever they are. Only where kern.a50_vibrate exists; DROP_DIR vibration.off turns it off.
 */
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <mach-o/loader.h>      /* before objc/runtime.h: objc_addLoadImageFunc's struct mach_header */
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

#define DROP_DIR "/private/var/herolte/"
#define TAP_MS 25
#define TAP_GAP_NS (60ull * 1000 * 1000)
#define MAX_STEPS 64
#define MAX_PATTERN_MS 30000

extern void herodrm_log(const char *format, ...);
extern void herovibe_tramp(void);
IMP herovibe_note(id self, SEL cmd, uintptr_t arg2, uintptr_t arg3);

enum { WATCH_PATTERN, WATCH_STOP, WATCH_TAP, WATCH_LOG };

static struct watched {
   const char *cls, *sel;
   bool class_method;
   int kind;
   SEL s;
   IMP orig;
   bool done;
   unsigned calls;
} watched[] = {
   { "TLAlertSystemSoundController", "_vibrationPatternForAlert:withSound:", true, WATCH_PATTERN },
   { "TLAlertController", "stopPlayingAlerts:withOptions:playbackCompletionType:", false, WATCH_STOP },
   { "TLAlert", "stop", false, WATCH_STOP },
   { "TLAlert", "stopWithOptions:", false, WATCH_STOP },
   { "TLAlertController", "playAlert:withCompletionHandler:", false, WATCH_LOG },
   { "UIFeedbackGenerator", "_playFeedback:withMinimumIntervalPassed:since:prefersRegularPace:", false, WATCH_TAP },
   { "_UIFeedback", "play", false, WATCH_TAP },
};
#define NWATCHED (sizeof(watched) / sizeof(watched[0]))

static dispatch_queue_t vibe_queue;
static volatile unsigned vibe_generation;
static uint64_t last_tap;
static mach_timebase_info_data_t timebase;

static void
motor(int ms)
{
   sysctlbyname("kern.a50_vibrate", NULL, NULL, &ms, sizeof(ms));
}

struct steps {
   unsigned generation, n;
   bool on[MAX_STEPS];
   uint32_t ms[MAX_STEPS];
};

/* On the vibration queue: the steps in turn, in 20 ms naps, until done or a newer pattern or a stop comes. */
static void
run_steps(void *context)
{
   struct steps *s = context;
   uint32_t total = 0;
   for (unsigned i = 0; i < s->n && s->generation == vibe_generation && total < MAX_PATTERN_MS; i++) {
      if (s->on[i])
         motor((int)s->ms[i]);
      for (uint32_t left = s->ms[i]; left > 0 && s->generation == vibe_generation;) {
         uint32_t nap = left < 20 ? left : 20;
         usleep(nap * 1000);
         left -= nap;
      }
      total += s->ms[i];
   }
   free(s);
}

static id
msg(id target, const char *selector)
{
   return ((id (*)(id, SEL))objc_msgSend)(target, sel_registerName(selector));
}

static bool
responds(id target, const char *selector)
{
   return target && ((BOOL (*)(id, SEL, SEL))objc_msgSend)(target, sel_registerName("respondsToSelector:"),
      sel_registerName(selector));
}

/* A TLVibrationPattern (or its property list): {VibePattern = (on, ms, on, ms, ...); Intensity = ...}. */
static void
play_pattern(id pattern)
{
   static unsigned lines;
   id dict = responds(pattern, "propertyListRepresentation") ? msg(pattern, "propertyListRepresentation") : pattern;
   id list = responds(dict, "objectForKey:") ? ((id (*)(id, SEL, id))objc_msgSend)(dict, sel_registerName("objectForKey:"),
      ((id (*)(id, SEL, const char *))objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"),
      "VibePattern")) : nil;
   unsigned long count = list ? ((unsigned long (*)(id, SEL))objc_msgSend)(list, sel_registerName("count")) : 0;
   struct steps *s = calloc(1, sizeof(*s));
   if (!s)
      return;
   for (unsigned long i = 0; i + 1 < count && s->n < MAX_STEPS; i += 2) {
      id on = ((id (*)(id, SEL, unsigned long))objc_msgSend)(list, sel_registerName("objectAtIndex:"), i);
      id ms = ((id (*)(id, SEL, unsigned long))objc_msgSend)(list, sel_registerName("objectAtIndex:"), i + 1);
      s->on[s->n] = ((BOOL (*)(id, SEL))objc_msgSend)(on, sel_registerName("boolValue"));
      int value = ((int (*)(id, SEL))objc_msgSend)(ms, sel_registerName("intValue"));
      s->ms[s->n++] = value < 0 ? 0 : (uint32_t)value;
   }
   if (s->n == 0) {
      /* Not understood: one plain buzz rather than nothing. */
      s->on[0] = true;
      s->ms[0] = 400;
      s->n = 1;
   }
   if (lines++ < 20)
      herodrm_log("herosb: vibration: pattern %s, %lu entries -> %u steps, first %s %u ms",
         pattern ? object_getClassName(pattern) : "nil", count, s->n, s->on[0] ? "on" : "off", s->ms[0]);
   s->generation = ++vibe_generation;
   dispatch_async_f(vibe_queue, s, run_steps);
}

static void
stop_pattern(void)
{
   vibe_generation++;
   motor(0);
}

static void
tap(void)
{
   uint64_t now = mach_absolute_time() * timebase.numer / timebase.denom;
   if (now - last_tap < TAP_GAP_NS)
      return;
   last_tap = now;
   motor(TAP_MS);
}

IMP
herovibe_note(id self, SEL cmd, uintptr_t arg2, uintptr_t arg3)
{
   struct watched *w = NULL;
   for (unsigned i = 0; i < NWATCHED && !w; i++) {
      if (watched[i].done && watched[i].orig && watched[i].s == cmd)
         w = &watched[i];
   }
   if (!w)
      abort();          /* cannot happen: only watched selectors come here */
   unsigned calls = ++w->calls;
   switch (w->kind) {
   case WATCH_PATTERN:
      /* Apple's own answer, asked once more here: the pattern the alert is about to play. */
      play_pattern(((id (*)(id, SEL, uintptr_t, uintptr_t))w->orig)(self, cmd, arg2, arg3));
      break;
   case WATCH_STOP:
      stop_pattern();
      break;
   case WATCH_TAP:
      tap();
      break;
   }
   if (calls <= 4)
      herodrm_log("herosb: vibration: %c[%s %s] called (%u)", w->class_method ? '+' : '-', w->cls, w->sel, calls);
   return w->orig;
}

static bool vibe_all_hooked;

static void
hook_all(void)
{
   unsigned left = 0;
   for (unsigned i = 0; i < NWATCHED; i++) {
      struct watched *w = &watched[i];
      if (w->done)
         continue;
      Class cls = objc_getClass(w->cls);
      if (!cls) {
         left++;
         continue;
      }
      Class target = w->class_method ? object_getClass((id)cls) : cls;
      SEL s = sel_registerName(w->sel);
      Method m = class_getInstanceMethod(target, s);
      w->done = true;
      if (!m) {
         herodrm_log("herosb: vibration: %s has no %c%s", w->cls, w->class_method ? '+' : '-', w->sel);
         continue;
      }
      w->s = s;
      w->orig = method_getImplementation(m);
      const char *types = method_getTypeEncoding(m);
      class_replaceMethod(target, s, (IMP)herovibe_tramp, types);
      herodrm_log("herosb: vibration: watching %c[%s %s] (%s)", w->class_method ? '+' : '-', w->cls, w->sel,
         types ? types : "?");
   }
   vibe_all_hooked = left == 0;
}

static volatile int vibe_pending;

static void
try_hooks(void *unused)
{
   vibe_pending = 0;
   hook_all();
}

/* Called by objc with its runtime lock held: no objc here (see herosb.c's flashlight); the main queue looks. */
static void
look_for_classes(const struct mach_header *header)
{
   if (!vibe_all_hooked && __sync_bool_compare_and_swap(&vibe_pending, 0, 1))
      dispatch_async_f(dispatch_get_main_queue(), NULL, try_hooks);
}

__attribute__((constructor)) static void
herovibe_start(void)
{
   int ms = 0;
   size_t size = sizeof(ms);
   if (access(DROP_DIR "vibration.off", F_OK) == 0 || sysctlbyname("kern.a50_vibrate", &ms, &size, NULL, 0) != 0)
      return;
   mach_timebase_info(&timebase);
   vibe_queue = dispatch_queue_create("herosb.vibration", NULL);
   hook_all();
   if (!vibe_all_hooked)
      objc_addLoadImageFunc(look_for_classes);
}
