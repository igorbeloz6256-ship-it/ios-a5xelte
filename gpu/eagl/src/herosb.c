/*
 * libherosb.dylib, in SpringBoard (boot-arg herosb=1, bsd/dev/herolte_gl.c): PosterBoard has no poster providers on this
 * root, so the wallpaper windows stay black. The picture of the drop zone goes into them, and behind the home screen.
 * DROP_DIR wallpaper.off turns it off. On the Galaxy A50 it also answers AVFlashlight with the kernel's torch (below).
 */
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <mach-o/loader.h>      /* before objc/runtime.h: objc_addLoadImageFunc's struct mach_header */
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

#define DROP_DIR "/private/var/herolte/"
#define VIEW_TAG 0x48455230

typedef struct { double x, y, w, h; } rect_t;

extern void herodrm_log(const char *format, ...);

static id
nsstring(const char *text)
{
   return ((id (*)(id, SEL, const char *))objc_msgSend)((id)objc_getClass("NSString"),
      sel_registerName("stringWithUTF8String:"), text);
}

static id
send0(id target, const char *selector)
{
   return ((id (*)(id, SEL))objc_msgSend)(target, sel_registerName(selector));
}

static id picture;
static bool gestures_on;
static int status_bar_kept, status_bar_asked, status_bar_replaced;
static unsigned rounds;

static void
give_picture(id window, bool behind_all)
{
   if (((id (*)(id, SEL, long))objc_msgSend)(window, sel_registerName("viewWithTag:"), VIEW_TAG))
      return;
   id view = ((id (*)(id, SEL, id))objc_msgSend)(send0((id)objc_getClass("UIImageView"), "alloc"),
      sel_registerName("initWithImage:"), picture);
   rect_t bounds = ((rect_t (*)(id, SEL))objc_msgSend)(window, sel_registerName("bounds"));
   ((void (*)(id, SEL, rect_t))objc_msgSend)(view, sel_registerName("setFrame:"), bounds);
   ((void (*)(id, SEL, long))objc_msgSend)(view, sel_registerName("setContentMode:"), 2);          /* aspect fill */
   ((void (*)(id, SEL, unsigned long))objc_msgSend)(view, sel_registerName("setAutoresizingMask:"), 18);
   ((void (*)(id, SEL, long))objc_msgSend)(view, sel_registerName("setTag:"), VIEW_TAG);
   ((void (*)(id, SEL, BOOL))objc_msgSend)(view, sel_registerName("setUserInteractionEnabled:"), NO);
   if (behind_all)
      ((void (*)(id, SEL, id, long))objc_msgSend)(window, sel_registerName("insertSubview:atIndex:"), view, 0);
   else
      ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("addSubview:"), view);
   herodrm_log("herosb: picture in %s, %.0f x %.0f", object_getClassName(window), bounds.w, bounds.h);
}

static void
look_at_windows(void *unused)
{
   rounds++;
   Class windows_class = objc_getClass("UIWindow");
   SEL all = sel_registerName("allWindowsIncludingInternalWindows:onlyVisibleWindows:");
   id windows = windows_class && class_respondsToSelector(object_getClass((id)windows_class), all) ?
      ((id (*)(id, SEL, BOOL, BOOL))objc_msgSend)((id)windows_class, all, YES, NO) : nil;
   if (!picture && objc_getClass("UIImage")) {
      picture = ((id (*)(id, SEL, id))objc_msgSend)((id)objc_getClass("UIImage"), sel_registerName("imageWithContentsOfFile:"),
         nsstring(DROP_DIR "wallpaper.png"));
      if (picture)
         send0(picture, "retain");
   }
   size_t count = windows ? ((size_t (*)(id, SEL))objc_msgSend)(windows, sel_registerName("count")) : 0;
   if (rounds == 1 || rounds == 6)
      herodrm_log("herosb: round %u, %zu windows, picture %d, gestures %d, status bar hooks %d asked %d replaced %d", rounds, count, picture != nil, gestures_on, status_bar_kept, status_bar_asked, status_bar_replaced);
   for (size_t i = 0; picture && i < count; i++) {
      id window = ((id (*)(id, SEL, size_t))objc_msgSend)(windows, sel_registerName("objectAtIndex:"), i);
      const char *name = object_getClassName(window);
      if (rounds == 6)
         herodrm_log("herosb: window %s", name);
      if (strstr(name, "Wallpaper"))
         give_picture(window, false);
      else if (strcmp(name, "SBHomeScreenWindow") == 0)
         give_picture(window, true);
   }
   if (rounds < 60)
      dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), dispatch_get_main_queue(), NULL, look_at_windows);
}

/*
 * The ADT says "home button" (home-button-type 1): UIKit then has a status bar for this 750x1334 screen, where type 2
 * gets the red rdar:45025538 one. SpringBoard alone is told type 2, which is where the home gestures are decided, as
 * the old home-gesture tweaks did. DROP_DIR gestures.off turns it off.
 */
static long
says_gestures(id self, SEL cmd)
{
   return 2;
}

static BOOL
says_yes(id self, SEL cmd)
{
   return YES;
}

/*
 * ...but told type 2, UIKit in SpringBoard asks each provider class in turn, none takes a 750x1334 screen and it ends on
 * the red rdar:45025538 fallback. The plain phone's class takes any screen here, and is put in where the fallback
 * still reaches the status bar.
 *
 * DROP_DIR statusbar.provider (one line, a class name) picks another one: the Galaxy A50 (1080x2340 at @3x, 360x780
 * points, no iPhone has it) has a camera hole in the middle of the top edge, where the plain phone's clock would be,
 * so it asks for an iPhone X style split one. A name that is not a class here keeps the plain phone's.
 */
static Class
chosen_provider(void)
{
   static Class chosen;
   if (chosen)
      return chosen;
   chosen = objc_getClass("_UIStatusBarVisualProvider_Phone");
   char name[96] = { 0 };
   int fd = open(DROP_DIR "statusbar.provider", O_RDONLY);
   if (fd >= 0) {
      ssize_t n = read(fd, name, sizeof(name) - 1);
      close(fd);
      for (ssize_t i = 0; i < n; i++) {
         if (name[i] == '\n' || name[i] == '\r' || name[i] == ' ')
            name[i] = '\0';
      }
      Class wanted = n > 0 ? objc_getClass(name) : Nil;
      herodrm_log("herosb: status bar provider %s %s", name, wanted ? "found" : "NOT found, plain phone's");
      if (wanted)
         chosen = wanted;
   }
   return chosen;
}

static Class
phone_takes_it(id self, SEL cmd, id screen, id info)
{
   status_bar_asked++;
   return chosen_provider();
}

static void (*set_provider_was)(id, SEL, Class);

static void
set_provider(id self, SEL cmd, Class provider)
{
   if (provider == objc_getClass("_UIStatusBarVisualProvider_Fallback")) {
      status_bar_replaced++;
      provider = chosen_provider();
   }
   set_provider_was(self, cmd, provider);
}

static void
keep_status_bar(void)
{
   static const char *const providers[] = { "_UIStatusBarVisualProvider_iOS", "_UIStatusBarVisualProvider_Phone",
      "_UIStatusBarVisualProvider_Fallback" };
   SEL pick = sel_registerName("visualProviderSubclassForScreen:visualProviderInfo:");
   for (size_t i = 0; i < sizeof(providers) / sizeof(providers[0]); i++) {
      Class provider = objc_getClass(providers[i]);
      Method method = provider ? class_getClassMethod(provider, pick) : NULL;
      if (method) {
         method_setImplementation(method, (IMP)phone_takes_it);
         status_bar_kept++;
      }
   }
   Class bar = objc_getClass("_UIStatusBar");
   Method setter = bar ? class_getInstanceMethod(bar, sel_registerName("setVisualProviderClass:")) : NULL;
   if (setter) {
      set_provider_was = (void (*)(id, SEL, Class))method_setImplementation(setter, (IMP)set_provider);
      status_bar_kept += 10;
   }
}

static void
ask_for_gestures(void)
{
   keep_status_bar();
   Class platform = objc_getClass("BSPlatform");
   Method type = platform ? class_getInstanceMethod(platform, sel_registerName("homeButtonType")) : NULL;
   if (type) {
      method_setImplementation(type, (IMP)says_gestures);
      gestures_on = true;
   }
   Class settings = objc_getClass("SBHomeGestureSettings");
   Method enabled = settings ? class_getInstanceMethod(settings, sel_registerName("isHomeGestureEnabled")) : NULL;
   if (enabled)
      method_setImplementation(enabled, (IMP)says_yes);
}

/*
 * SpringBoard's own frame rate caps (from Scion's inject_sb.c, Galaxy S9). Some of its animations ask for fewer frames
 * than the panel has: the crossfade of the page dots and the search pill after every page's slide is
 * CAFrameRateRangeMake(20, 30, 0), and CoreAnimation gives SpringBoard 30 a second for as long as it runs -- on the S9
 * half of all the refreshes missed while swiping. A range whose most is under 60 gets 60 as its most and preferred.
 * DROP_DIR fps.off leaves them as Apple set them.
 */
typedef struct { float minimum, maximum, preferred; } frame_rate_range_t;
static void (*fluid_set_range_was)(id, SEL, frame_rate_range_t, unsigned);
static void (*animation_set_range_was)(id, SEL, frame_rate_range_t, unsigned);
static unsigned ranges_raised;

static frame_rate_range_t
raise_range(frame_rate_range_t range)
{
   if (range.maximum > 0.0f && range.maximum < 60.0f) {
      range.maximum = 60.0f;
      range.preferred = 60.0f;
      ranges_raised++;
   }
   return range;
}

static void
fluid_set_range(id self, SEL cmd, frame_rate_range_t range, unsigned reason)
{
   fluid_set_range_was(self, cmd, raise_range(range), reason);
}

static void
animation_set_range(id self, SEL cmd, frame_rate_range_t range, unsigned reason)
{
   animation_set_range_was(self, cmd, raise_range(range), reason);
}

static void
raise_frame_rates(void)
{
   SEL set = sel_registerName("setFrameRateRange:highFrameRateReason:");
   Class fluid = objc_getClass("SBFFluidBehaviorSettings"), animation = objc_getClass("SBFAnimationSettings");
   Method m = fluid && !fluid_set_range_was ? class_getInstanceMethod(fluid, set) : NULL;
   if (m)
      fluid_set_range_was = (void (*)(id, SEL, frame_rate_range_t, unsigned))method_setImplementation(m, (IMP)fluid_set_range);
   m = animation && !animation_set_range_was ? class_getInstanceMethod(animation, set) : NULL;
   if (m)
      animation_set_range_was = (void (*)(id, SEL, frame_rate_range_t, unsigned))method_setImplementation(m, (IMP)animation_set_range);
   herodrm_log("herosb: frame rate caps raised to 60: fluid settings %s, animation settings %s",
      fluid_set_range_was ? "yes" : "NO", animation_set_range_was ? "yes" : "NO");
}

static void
report_ranges(void *unused)
{
   herodrm_log("herosb: %u frame rate ranges raised to 60 so far", ranges_raised);
}

/*
 * The flashlight (Control Center, lock screen): AVFlashlight asks mediaserverd's camera stack, which this port does not
 * have, so [AVFlashlight hasFlashlight] is NO and its button does nothing. In SpringBoard, where SBUIFlashlightController
 * keeps the one AVFlashlight, it answers itself and gives the level to the kernel: sysctl kern.a50_torch = 0..100
 * (bsd/dev/a50_torch.c, the Galaxy A50's rear LED). Only where that sysctl exists; DROP_DIR flashlight.off turns it off.
 */
static float torch_level;
static bool torch_hooked;
static Class flashlight_base;           /* AVFlashlight's superclass: init and dealloc go on to it */

static BOOL
fl_yes(id self, SEL cmd)
{
   return YES;
}

static BOOL
fl_no(id self, SEL cmd)
{
   return NO;
}

/* No server connection to set up: the superclass's init is all there is. */
static id
fl_init(id self, SEL cmd)
{
   struct objc_super base = { self, flashlight_base };
   return ((id (*)(struct objc_super *, SEL))objc_msgSendSuper)(&base, cmd);
}

/* ...and its dealloc, as nothing of Apple's was set up to be torn down. */
static void
fl_dealloc(id self, SEL cmd)
{
   struct objc_super base = { self, flashlight_base };
   ((void (*)(struct objc_super *, SEL))objc_msgSendSuper)(&base, cmd);
}

static float
fl_level(id self, SEL cmd)
{
   return torch_level;
}

static BOOL
fl_set_level(id self, SEL cmd, float level, id *error)
{
   level = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
   int value = (int)(level * 100.0f + 0.5f);
   if (level > 0.0f && value == 0)
      value = 1;
   int r = sysctlbyname("kern.a50_torch", NULL, NULL, &value, sizeof(value));
   id key = nsstring("flashlightLevel");
   ((void (*)(id, SEL, id))objc_msgSend)(self, sel_registerName("willChangeValueForKey:"), key);
   torch_level = level;
   ((void (*)(id, SEL, id))objc_msgSend)(self, sel_registerName("didChangeValueForKey:"), key);
   herodrm_log("herosb: flashlight level %.2f -> kern.a50_torch %d (%d)", level, value, r);
   if (error)
      *error = nil;
   return r == 0;
}

static BOOL
fl_power_on(id self, SEL cmd, id *error)
{
   if (error)
      *error = nil;
   return YES;
}

static void
fl_power_off(id self, SEL cmd)
{
   if (torch_level > 0.0f)
      fl_set_level(self, sel_registerName("setFlashlightLevel:withError:"), 0.0f, NULL);
}

/* Whether cls itself (not a superclass) has the method. */
static bool
own_method(Class cls, SEL sel)
{
   unsigned n = 0;
   Method *list = class_copyMethodList(cls, &n);
   bool found = false;
   for (unsigned i = 0; list && i < n && !found; i++)
      found = method_getName(list[i]) == sel;
   free(list);
   return found;
}

/*
 * class_replaceMethod puts the method into AVFlashlight itself (its metaclass for +), whether it had its own or inherited
 * one. (xnu18: method_setImplementation on class_getInstanceMethod's answer would change an inherited -init or -dealloc
 * for every class.)
 */
static void
hook(Class cls, const char *selector, IMP imp, const char *types, unsigned *own)
{
   SEL sel = sel_registerName(selector);
   *own = (*own << 1) | own_method(cls, sel);
   class_replaceMethod(cls, sel, imp, types);
}

static void
hook_flashlight(void)
{
   Class flashlight = objc_getClass("AVFlashlight");
   if (torch_hooked || !flashlight)
      return;
   torch_hooked = true;
   flashlight_base = class_getSuperclass(flashlight);
   unsigned own = 0;
   hook(object_getClass((id)flashlight), "hasFlashlight", (IMP)fl_yes, "B16@0:8", &own);
   hook(flashlight, "init", (IMP)fl_init, "@16@0:8", &own);
   hook(flashlight, "dealloc", (IMP)fl_dealloc, "v16@0:8", &own);
   hook(flashlight, "isAvailable", (IMP)fl_yes, "B16@0:8", &own);
   hook(flashlight, "isOverheated", (IMP)fl_no, "B16@0:8", &own);
   hook(flashlight, "flashlightLevel", (IMP)fl_level, "f16@0:8", &own);
   hook(flashlight, "setFlashlightLevel:withError:", (IMP)fl_set_level, "B28@0:8f16^@20", &own);
   hook(flashlight, "turnPowerOnWithError:", (IMP)fl_power_on, "B24@0:8^@16", &own);
   hook(flashlight, "turnPowerOff", (IMP)fl_power_off, "v16@0:8", &own);
   herodrm_log("herosb: flashlight: AVFlashlight (superclass %s) answered here; its own methods of the 9: %#x",
      flashlight_base ? class_getName(flashlight_base) : "none", own);
}

static volatile int flashlight_pending;

static void
try_flashlight(void *unused)
{
   flashlight_pending = 0;
   hook_flashlight();
}

/*
 * A framework loaded later may bring AVFlashlight. objc calls this with its runtime lock held (for the images already
 * there, inside objc_addLoadImageFunc): nothing of objc here -- xnu18's SpringBoard trapped (SIGTRAP, twice) doing that.
 * The main queue looks instead.
 */
static void
look_for_flashlight(const struct mach_header *header)
{
   if (!torch_hooked && __sync_bool_compare_and_swap(&flashlight_pending, 0, 1))
      dispatch_async_f(dispatch_get_main_queue(), NULL, try_flashlight);
}

__attribute__((constructor)) static void
herosb_start(void)
{
   int torch = 0;
   size_t torch_size = sizeof(torch);
   if (access(DROP_DIR "flashlight.off", F_OK) != 0 && sysctlbyname("kern.a50_torch", &torch, &torch_size, NULL, 0) == 0) {
      hook_flashlight();        /* SpringBoard links AVFlashlight's framework: already here, as a rule */
      if (!torch_hooked)
         objc_addLoadImageFunc(look_for_flashlight);
   }
   if (access(DROP_DIR "fps.off", F_OK) != 0) {
      raise_frame_rates();
      dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 120 * NSEC_PER_SEC), dispatch_get_main_queue(), NULL, report_ranges);
   }
   bool gestures = access(DROP_DIR "gestures.off", F_OK) != 0, wallpaper = access(DROP_DIR "wallpaper.off", F_OK) != 0;
   herodrm_log("herosb: in pid %d, status bar and gestures %s, wallpaper %s", getpid(), gestures ? "hooked" : "off",
      wallpaper ? "on" : "off");
   if (gestures)
      ask_for_gestures();
   if (!wallpaper)
      return;
   dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), dispatch_get_main_queue(), NULL, look_at_windows);
}
