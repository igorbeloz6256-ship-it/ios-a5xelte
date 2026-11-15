/*
 * a50alsa: a tiny ALSA tool for TWRP on the Galaxy A50 (static Linux arm64 binary, no libc: raw syscalls), to find the
 * speaker's path under Linux before the iOS kernel takes it over (A50_PROGRESS.md, "Звук").
 *
 *   a50alsa list                    every control of card 0: numid, name, type, values (enum items)
 *   a50alsa get NAME                one control's values
 *   a50alsa set NAME VALUE...       an enum by item name, integers/booleans by number (one per channel, or one for all)
 *   a50alsa tone DEV SECONDS LEVEL  a 1 kHz sine on /dev/snd/pcmC0D<DEV>p, 48 kHz, S16_LE, stereo; LEVEL in 1/1000
 *                                   of full scale (the speaker has no protection on this path: keep it low)
 *
 * Structures and ioctl numbers from Linux include/uapi/sound/asound.h (4.14), checked by size below.
 * Build: tools/a50-alsa/build.ps1 (Arm GNU aarch64-none-elf, -nostdlib -static).
 */
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned int u32;
typedef unsigned long long u64;

/* ---- syscalls (Linux arm64) ---- */
static long
sc(long n, long a, long b, long c, long d)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory", "cc");
	return x0;
}
#define SYS_ioctl 29
#define SYS_openat 56
#define SYS_close 57
#define SYS_write 64
#define SYS_exit_group 94
#define SYS_nanosleep 101
#define AT_FDCWD (-100)
#define O_RDWR 2

static void *
memset(void *d, int c, size_t n)
{
	unsigned char *p = d;
	while (n--)
		*p++ = (unsigned char)c;
	return d;
}

static void *
memcpy(void *d, const void *s, size_t n)
{
	unsigned char *p = d;
	const unsigned char *q = s;
	while (n--)
		*p++ = *q++;
	return d;
}

static size_t
slen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

static int
seq(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static void
out(const char *s)
{
	sc(SYS_write, 1, (long)s, (long)slen(s), 0);
}

static void
outn(long v)
{
	char b[24];
	int i = 23;
	int neg = v < 0;
	unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
	b[i] = 0;
	do {
		b[--i] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	if (neg)
		b[--i] = '-';
	out(b + i);
}

static long
num(const char *s)
{
	long v = 0;
	int neg = *s == '-';
	if (neg)
		s++;
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	return neg ? -v : v;
}

__attribute__((noreturn)) static void
quit(int code)
{
	sc(SYS_exit_group, code, 0, 0, 0);
	for (;;) {
	}
}

/* ---- ALSA control ---- */
#define IOC(dir, type, nr, size) (((unsigned long)(dir) << 30) | ((unsigned long)(size) << 16) | ((type) << 8) | (nr))
#define IOWR(type, nr, size) IOC(3, type, nr, size)
#define IOW(type, nr, size) IOC(1, type, nr, size)
#define IO(type, nr) IOC(0, type, nr, 0)

struct ctl_id {
	u32 numid;
	int iface;
	u32 device, subdevice;
	unsigned char name[44];
	u32 index;
};

struct ctl_list {
	u32 offset, space, used, count;
	struct ctl_id *pids;
	unsigned char reserved[50];
};

struct ctl_info {
	struct ctl_id id;
	int type;
	u32 access, count;
	int owner;
	union {
		struct { long min, max, step; } integer;
		struct { long long min, max, step; } integer64;
		struct { u32 items, item; char name[64]; u64 names_ptr; u32 names_length; } enumerated;
		unsigned char reserved[128];
	} value;
	unsigned char reserved[64];
};

struct ctl_value {
	struct ctl_id id;
	u32 indirect;
	union {
		long integer[128];
		long long integer64[64];
		u32 enumerated[128];
		unsigned char bytes[512];
	} value;
	struct { long tv_sec, tv_nsec; } tstamp;
	unsigned char reserved[128 - 16];
};

_Static_assert(sizeof(struct ctl_id) == 64, "ctl_id");
_Static_assert(sizeof(struct ctl_list) == 80, "ctl_list");
_Static_assert(sizeof(struct ctl_info) == 272, "ctl_info");
_Static_assert(sizeof(struct ctl_value) == 1224, "ctl_value");

#define CTL_LIST IOWR('U', 0x10, sizeof(struct ctl_list))
#define CTL_INFO IOWR('U', 0x11, sizeof(struct ctl_info))
#define CTL_READ IOWR('U', 0x12, sizeof(struct ctl_value))
#define CTL_WRITE IOWR('U', 0x13, sizeof(struct ctl_value))
#define TYPE_BOOLEAN 1
#define TYPE_INTEGER 2
#define TYPE_ENUMERATED 3
#define TYPE_BYTES 4
#define TYPE_INTEGER64 6

static int ctl_fd = -1;
static struct ctl_id ids[2048];
static u32 nids;

static void
ctl_open(void)
{
	ctl_fd = (int)sc(SYS_openat, AT_FDCWD, (long)"/dev/snd/controlC0", O_RDWR, 0);
	if (ctl_fd < 0) {
		out("cannot open /dev/snd/controlC0\n");
		quit(1);
	}
	struct ctl_list l;
	memset(&l, 0, sizeof(l));
	if (sc(SYS_ioctl, ctl_fd, CTL_LIST, (long)&l, 0) < 0) {
		out("ELEM_LIST failed\n");
		quit(1);
	}
	nids = l.count > 2048 ? 2048 : l.count;
	memset(&l, 0, sizeof(l));
	l.space = nids;
	l.pids = ids;
	sc(SYS_ioctl, ctl_fd, CTL_LIST, (long)&l, 0);
	nids = l.used;
}

static int
ctl_info(u32 numid, struct ctl_info *info)
{
	memset(info, 0, sizeof(*info));
	info->id.numid = numid;
	return (int)sc(SYS_ioctl, ctl_fd, CTL_INFO, (long)info, 0);
}

static int
ctl_item_name(struct ctl_info *info, u32 item, char *name)
{
	struct ctl_info i;
	memcpy(&i, info, sizeof(i));
	i.value.enumerated.item = item;
	if (sc(SYS_ioctl, ctl_fd, CTL_INFO, (long)&i, 0) < 0)
		return -1;
	memcpy(name, i.value.enumerated.name, 64);
	name[63] = 0;
	return 0;
}

static void
show(u32 n, int with_items)
{
	struct ctl_info info;
	struct ctl_value v;
	if (ctl_info(ids[n].numid, &info) < 0)
		return;
	memset(&v, 0, sizeof(v));
	v.id.numid = ids[n].numid;
	long r = sc(SYS_ioctl, ctl_fd, CTL_READ, (long)&v, 0);
	outn(ids[n].numid);
	out("\t");
	out((const char *)ids[n].name);
	out("\t");
	const char *t = info.type == TYPE_BOOLEAN ? "bool" : info.type == TYPE_INTEGER ? "int" :
	    info.type == TYPE_ENUMERATED ? "enum" : info.type == TYPE_BYTES ? "bytes" : "other";
	out(t);
	out("x");
	outn(info.count);
	out("\t=");
	u32 count = info.count > 8 ? 8 : info.count;
	for (u32 i = 0; r >= 0 && i < count; i++) {
		out(" ");
		if (info.type == TYPE_ENUMERATED) {
			char name[64];
			if (ctl_item_name(&info, v.value.enumerated[i], name) == 0)
				out(name);
			else
				outn(v.value.enumerated[i]);
		} else if (info.type == TYPE_BOOLEAN || info.type == TYPE_INTEGER) {
			outn(v.value.integer[i]);
		} else if (info.type == TYPE_BYTES) {
			outn(v.value.bytes[i]);
		}
	}
	if (info.type == TYPE_INTEGER) {
		out("\t[");
		outn(info.value.integer.min);
		out("..");
		outn(info.value.integer.max);
		out("]");
	}
	if (with_items && info.type == TYPE_ENUMERATED) {
		out("\t{");
		for (u32 i = 0; i < info.value.enumerated.items && i < 64; i++) {
			char name[64];
			if (ctl_item_name(&info, i, name) == 0) {
				out(i ? "|" : "");
				out(name);
			}
		}
		out("}");
	}
	out("\n");
}

static int
find(const char *name)
{
	for (u32 i = 0; i < nids; i++) {
		if (seq((const char *)ids[i].name, name))
			return (int)i;
	}
	out("no control named ");
	out(name);
	out("\n");
	return -1;
}

static int
ctl_set(const char *name, int argc, char **values)
{
	int n = find(name);
	struct ctl_info info;
	struct ctl_value v;
	if (n < 0 || ctl_info(ids[n].numid, &info) < 0)
		return 1;
	memset(&v, 0, sizeof(v));
	v.id.numid = ids[n].numid;
	for (u32 i = 0; i < info.count && i < 128; i++) {
		const char *s = values[(int)i < argc ? (int)i : argc - 1];
		if (info.type == TYPE_ENUMERATED) {
			u32 item = 0xffffffffu;
			for (u32 k = 0; k < info.value.enumerated.items && item == 0xffffffffu; k++) {
				char iname[64];
				if (ctl_item_name(&info, k, iname) == 0 && seq(iname, s))
					item = k;
			}
			if (item == 0xffffffffu) {
				out("no item ");
				out(s);
				out("\n");
				return 1;
			}
			v.value.enumerated[i] = item;
		} else {
			v.value.integer[i] = num(s);
		}
	}
	if (sc(SYS_ioctl, ctl_fd, CTL_WRITE, (long)&v, 0) < 0) {
		out("write failed: ");
		out(name);
		out("\n");
		return 1;
	}
	show((u32)n, 0);
	return 0;
}

/* ---- PCM ---- */
struct snd_interval {
	u32 min, max;
	u32 flags;     /* openmin:1 openmax:1 integer:1 empty:1 */
};

struct hw_params {
	u32 flags;
	u32 masks[3][8];
	u32 mres[5][8];
	struct snd_interval intervals[12];
	struct snd_interval ires[9];
	u32 rmask, cmask, info, msbits, rate_num, rate_den;
	unsigned long fifo_size;
	unsigned char reserved[64];
};
_Static_assert(sizeof(struct hw_params) == 608, "hw_params");
#define PCM_HW_PARAMS IOWR('A', 0x11, sizeof(struct hw_params))
#define PCM_PREPARE IO('A', 0x40)
#define PCM_DRAIN IO('A', 0x44)
#define P_ACCESS 0
#define P_FORMAT 1
#define P_SUBFORMAT 2
#define P_SAMPLE_BITS 8
#define P_FRAME_BITS 9
#define P_CHANNELS 10
#define P_RATE 11
#define P_PERIOD_SIZE 13
#define P_PERIODS 15

static void
set_mask(struct hw_params *p, int param, u32 bit)
{
	for (int i = 0; i < 8; i++)
		p->masks[param][i] = 0;
	p->masks[param][bit >> 5] = 1u << (bit & 31);
}

static void
set_int(struct hw_params *p, int param, u32 value)
{
	p->intervals[param - 8].min = value;
	p->intervals[param - 8].max = value;
	p->intervals[param - 8].flags = 1u << 2;     /* integer */
}

static const short sine[48] = {
	0, 4277, 8481, 12539, 16383, 19947, 23170, 25996, 28377, 30273, 31650, 32487, 32767, 32487, 31650, 30273, 28377,
	25996, 23170, 19947, 16383, 12539, 8481, 4277, 0, -4277, -8481, -12539, -16383, -19947, -23170, -25996, -28377,
	-30273, -31650, -32487, -32767, -32487, -31650, -30273, -28377, -25996, -23170, -19947, -16384, -12539, -8481, -4277,
};

static short frames[960 * 2];

static int
tone(int dev, int seconds, int level)
{
	char path[] = "/dev/snd/pcmC0D00p";
	int i = 15;     /* just after "/dev/snd/pcmC0D" */
	if (dev >= 10)
		path[i++] = (char)('0' + dev / 10);
	path[i++] = (char)('0' + dev % 10);
	path[i++] = 'p';
	path[i] = 0;
	int fd = (int)sc(SYS_openat, AT_FDCWD, (long)path, O_RDWR, 0);
	if (fd < 0) {
		out("cannot open ");
		out(path);
		out(": ");
		outn(fd);
		out("\n");
		return 1;
	}
	struct hw_params p;
	memset(&p, 0, sizeof(p));
	for (int m = 0; m < 3; m++)
		for (int k = 0; k < 8; k++)
			p.masks[m][k] = 0xffffffffu;
	for (int k = 0; k < 12; k++) {
		p.intervals[k].min = 0;
		p.intervals[k].max = 0xffffffffu;
	}
	p.rmask = 0xffffffffu;
	p.info = 0xffffffffu;
	set_mask(&p, P_ACCESS, 3);              /* RW_INTERLEAVED */
	set_mask(&p, P_FORMAT, 2);              /* S16_LE */
	set_mask(&p, P_SUBFORMAT, 0);
	set_int(&p, P_SAMPLE_BITS, 16);
	set_int(&p, P_FRAME_BITS, 32);
	set_int(&p, P_CHANNELS, 2);
	set_int(&p, P_RATE, 48000);
	set_int(&p, P_PERIOD_SIZE, 960);        /* 20 ms */
	set_int(&p, P_PERIODS, 4);
	long r = sc(SYS_ioctl, fd, PCM_HW_PARAMS, (long)&p, 0);
	out("hw_params: ");
	outn(r);
	out("\n");
	if (r < 0)
		return 1;
	r = sc(SYS_ioctl, fd, PCM_PREPARE, 0, 0);
	out("prepare: ");
	outn(r);
	out("\n");
	if (level < 0)
		level = 0;
	if (level > 1000)
		level = 1000;
	for (int f = 0; f < 960; f++) {
		short s = (short)((long)sine[f % 48] * level / 1000);
		frames[2 * f] = s;
		frames[2 * f + 1] = s;
	}
	long written = 0, errors = 0;
	for (int n = 0; n < seconds * 50; n++) {
		long w = sc(SYS_write, fd, (long)frames, sizeof(frames), 0);
		if (w < 0) {
			if (errors++ < 5) {
				out("write: ");
				outn(w);
				out("\n");
			}
			sc(SYS_ioctl, fd, PCM_PREPARE, 0, 0);
		} else {
			written += w;
		}
	}
	sc(SYS_ioctl, fd, PCM_DRAIN, 0, 0);
	out("bytes written: ");
	outn(written);
	out("\n");
	sc(SYS_close, fd, 0, 0, 0);
	return 0;
}

static int
main(int argc, char **argv)
{
	if (argc < 2) {
		out("usage: a50alsa list | get NAME | set NAME VALUE... | tone DEV SECONDS LEVEL\n");
		return 2;
	}
	if (seq(argv[1], "tone") && argc >= 5)
		return tone((int)num(argv[2]), (int)num(argv[3]), (int)num(argv[4]));
	ctl_open();
	if (seq(argv[1], "list")) {
		for (u32 i = 0; i < nids; i++)
			show(i, 1);
		return 0;
	}
	if (seq(argv[1], "get") && argc >= 3) {
		int n = find(argv[2]);
		if (n >= 0)
			show((u32)n, 1);
		return n < 0;
	}
	if (seq(argv[1], "set") && argc >= 4)
		return ctl_set(argv[2], argc - 3, argv + 3);
	out("bad arguments\n");
	return 2;
}

/* Entry: the kernel leaves argc, argv... at sp. */
__attribute__((noreturn, used)) static void
start_c(long *sp)
{
	quit(main((int)sp[0], (char **)(sp + 1)));
}

__asm__(".global _start\n_start:\n\tmov x0, sp\n\tbl start_c\n");
