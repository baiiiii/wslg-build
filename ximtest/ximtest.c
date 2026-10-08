#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>

static void dump_xim_servers(Display *d)
{
	Atom prop = XInternAtom(d, "XIM_SERVERS", False);
	Atom type = None;
	int fmt = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;

	printf("XIM_SERVERS atom = 0x%lx\n", (unsigned long)prop);
	if (XGetWindowProperty(d, DefaultRootWindow(d), prop, 0, 64, False,
			       XA_ATOM, &type, &fmt, &n, &after, &data) != Success) {
		printf("  XGetWindowProperty failed\n");
		return;
	}
	printf("  type=0x%lx format=%d nitems=%lu\n", (unsigned long)type, fmt, n);
	if (data) {
		unsigned long i;
		for (i = 0; i < n; i++) {
			Atom a = ((Atom *)data)[i];
			char *nm = XGetAtomName(d, a);
			Window w = XGetSelectionOwner(d, a);
			printf("  atom[%lu]=0x%lx name=%s owner=0x%lx\n",
			       i, (unsigned long)a, nm ? nm : "(null)",
			       (unsigned long)w);
			if (nm)
				XFree(nm);
		}
		XFree(data);
	}
}

static void probe(Display *d, const char *mods)
{
	XIM im;
	XIC ic;

	XSetLocaleModifiers(mods);
	im = XOpenIM(d, NULL, NULL, NULL);
	printf("  modifiers=\"%s\" XOpenIM=%s\n", mods, im ? "OK" : "failed");
	if (!im)
		return;
	ic = XCreateIC(im, XNInputStyle,
		       XIMPreeditNothing | XIMStatusNothing,
		       XNClientWindow, DefaultRootWindow(d),
		       XNFocusWindow, DefaultRootWindow(d), NULL);
	printf("  XCreateIC=%s\n", ic ? "OK" : "failed");
	if (ic)
		XSetICFocus(ic);
	XSync(d, False);
}

int main(void)
{
	Display *d;
	Window o;

	setlocale(LC_ALL, "");
	printf("LANG=%s LC_ALL=%s\n",
	       getenv("LANG") ? getenv("LANG") : "(unset)",
	       getenv("LC_ALL") ? getenv("LC_ALL") : "(unset)");
	printf("setlocale(LC_ALL,\"\") = %s\n", setlocale(LC_ALL, ""));
	printf("XSupportsLocale = %d\n", XSupportsLocale());
	printf("XSetLocaleModifiers(\"\") = %s\n", XSetLocaleModifiers(""));
	printf("XSetLocaleModifiers(@im=wslg-xim) = %s\n",
	       XSetLocaleModifiers("@im=wslg-xim"));
	d = XOpenDisplay(NULL);
	if (!d) {
		printf("XOpenDisplay failed for DISPLAY=%s\n",
		       getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
		return 1;
	}
	printf("XOpenDisplay OK: %s\n", DisplayString(d));
	printf("XMODIFIERS=%s\n",
	       getenv("XMODIFIERS") ? getenv("XMODIFIERS") : "(unset)");
	printf("server vendor=%s release=%d\n",
	       ServerVendor(d), VendorRelease(d));

	o = XGetSelectionOwner(d, XInternAtom(d, "wslg-xim", False));
	printf("XGetSelectionOwner(wslg-xim) = 0x%lx\n", (unsigned long)o);

	dump_xim_servers(d);
	probe(d, "");
	probe(d, "@im=wslg-xim");
	XCloseDisplay(d);
	printf("done\n");
	return 0;
}
