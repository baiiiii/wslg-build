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

static int err_handler(Display *d, XErrorEvent *e)
{
	char buf[256];
	char msg[128];
	char req[64];
	char num[64];

	(void)d;
	XGetErrorText(d, e->error_code, buf, sizeof buf);
	snprintf(msg, sizeof msg, "%s", buf);
	XGetErrorDatabaseText(d, "XRequest", "nosuch", "?", req, sizeof req);
	snprintf(num, sizeof num, "%d", e->request_code);
	XGetErrorDatabaseText(d, "XRequest", num, req, req, sizeof req);
	printf("  [X ERROR] request=%s(%d) error=%d(%s) resource=0x%lx serial=%lu\n",
	       req, e->request_code, e->error_code, msg,
	       (unsigned long)e->resourceid, e->serial);
	fflush(stdout);
	return 0;
}

static void probe(Display *d, const char *mods)
{
	XIM im;
	XIC ic;
	const char *m;

	m = XSetLocaleModifiers(mods);
	printf("  modifiers=\"%s\" -> \"%s\"\n", mods, m ? m : "(null)");
	im = XOpenIM(d, "ximtest", "Ximtest", NULL);
	printf("  XOpenIM(with res) = %s\n", im ? "OK" : "failed");
	if (!im) {
		im = XOpenIM(d, NULL, NULL, NULL);
		printf("  XOpenIM(NULL res) = %s\n", im ? "OK" : "failed");
	}
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

	{
		XClientMessageEvent ev;
		memset(&ev, 0, sizeof ev);
		ev.type = ClientMessage;
		ev.display = d;
		ev.window = o;
		ev.message_type = XInternAtom(d, "_XIM_XCONNECT", False);
		ev.format = 32;
		ev.data.l[0] = 0;
		printf("sending test ClientMessage to 0x%lx (type=0x%lx)\n",
		       (unsigned long)ev.window, (unsigned long)ev.message_type);
		XSendEvent(d, ev.window, False, NoEventMask, (XEvent *)&ev);
		XSync(d, False);
		printf("test ClientMessage sent\n");
	}

	{
		Window cw = XCreateSimpleWindow(d, DefaultRootWindow(d),
						0, 0, 1, 1, 0, 0, 0);
		Atom xc = XInternAtom(d, "_XIM_XCONNECT", False);
		XClientMessageEvent h;
		int k, seen = 0;

		printf("--- proper handshake: our comm window = 0x%lx ---\n",
		       (unsigned long)cw);
		memset(&h, 0, sizeof h);
		h.type = ClientMessage;
		h.display = d;
		h.window = cw;
		h.message_type = xc;
		h.format = 32;
		h.data.l[0] = (long)cw;
		XSendEvent(d, o, False, NoEventMask, (XEvent *)&h);
		XSync(d, False);
		printf("--- polling for replies (8s) ---\n");
		for (k = 0; k < 160; k++) {
			while (XPending(d)) {
				XEvent e;
				XNextEvent(d, &e);
				if (e.type == ClientMessage) {
					seen++;
					printf("  REPLY win=0x%lx type=0x%lx fmt=%d"
					       " d0=0x%lx d1=0x%lx d4=0x%lx\n",
					       (unsigned long)e.xclient.window,
					       (unsigned long)e.xclient.message_type,
					       e.xclient.format,
					       (unsigned long)e.xclient.data.l[0],
					       (unsigned long)e.xclient.data.l[1],
					       (unsigned long)e.xclient.data.l[4]);
				} else {
					printf("  event type=%d\n", e.type);
				}
			}
			usleep(50000);
		}
		printf("--- replies seen: %d ---\n", seen);
	}

	dump_xim_servers(d);
	printf("--- installing error handler ---\n");
	XSetErrorHandler(err_handler);
	XSync(d, False);
	probe(d, "");
	probe(d, "@im=wslg-xim");

	printf("--- self-server experiment ---\n");
	{
		Window w = XCreateSimpleWindow(d, DefaultRootWindow(d),
					       0, 0, 1, 1, 0, 0, 0);
		Atom nm = XInternAtom(d, "@server=ximtest-srv", False);
		Atom pr = XInternAtom(d, "XIM_SERVERS", False);
		XIM sim;

		XSetSelectionOwner(d, nm, w, CurrentTime);
		XChangeProperty(d, DefaultRootWindow(d), pr, XA_ATOM, 32,
				PropModeReplace, (unsigned char *)&nm, 1);
		XSync(d, False);
		printf("  window=0x%lx atom=0x%lx owner=0x%lx\n",
		       (unsigned long)w, (unsigned long)nm,
		       (unsigned long)XGetSelectionOwner(d, nm));
		XSetLocaleModifiers("@im=ximtest-srv");
		sim = XOpenIM(d, NULL, NULL, NULL);
		printf("  XOpenIM(self-server)=%s\n", sim ? "OK" : "failed");
		if (sim)
			printf("  *** XIM WORKS: the client side is fine ***\n");
	}
	XCloseDisplay(d);
	printf("done\n");
	return 0;
}
