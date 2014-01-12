/*
xscrollemu

Copyright(C) 2005 Eric Lathrop <eric@ericlathrop.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

For more details see the file COPYING
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <X11/Xlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <stdio.h>
#include <getopt.h>

int use_key = False;
int scroll_button = 2;
int toggle = False;
int trap = False;
int scroll_vert = True;
int scroll_horz = True;
int invert = False;
int v_init = 10;
int h_init = 10;
int v_threshhold = 10;
int h_threshhold = 50;
int start_x, start_y;

char * prog_name;

Display * rec;
Display * play;
Window * roots;
int nscreens;



void parseargs(int argc, char * argv[])
{
	int c;
	int longindex = 0;

	struct option long_options[] =
	{
		{ "button", 1, NULL, 'b' },
		{ "key", 1, NULL, 'k' },
		{ "threshhold", 1, NULL, 't' },
		{ "hthreshhold", 1, NULL, 'y' },
		{ "init", 1, NULL, 'i' },
		{ "invert", 0, NULL, 'I' },
		{ "toggle", 0, NULL, 'T' },
		{ "trap", 0, NULL, 'r' },
		{ "version", 0, NULL, 'v' },
		{ "help", 0, NULL, 'h' },
		{ 0, 0, 0, 0 }
	};
	
	while ((c = getopt_long(argc, argv, "b:k:t:y:i:ITrvh", long_options, &longindex)) != -1)
	{
		switch (c)
		{
			case 'b':
				use_key = False;
				scroll_button = strtol(optarg, NULL, 0);
				break;
			case 'k':
				use_key = True;
				scroll_button = XKeysymToKeycode(play, strtol(optarg, NULL, 0));
#ifdef DEBUG
				printf("Key: %d 0x%x\n", scroll_button, scroll_button);
#endif
				break;
			case 't':
				v_threshhold = strtol(optarg, NULL, 0);
				break;
			case 'y':
				h_threshhold = strtol(optarg, NULL, 0);
				break;
			case 'i':
				v_init = strtol(optarg, NULL, 0);
				h_init = strtol(optarg, NULL, 0);
				break;
			case 'I':
				invert = True;
				break;
			case 'T':
				toggle = True;
				break;
			case 'r':
				trap = True;
				break;
			case 'v':
				printf("%s %s\n", PACKAGE, VERSION);
				exit(0);
				break;
			case 'h':
			default:
				printf("Usage: %s [-b button | -k key] [-t theshhold] [-y hthreshhold] [-i init]  [-rIT] [-vh]\n", PACKAGE);
				printf("Emulates a scroll wheel when a button is held down.\n\n");
				
				printf(" -b\t--button\tThe mouse button to trigger scrolling.\n");
				printf(" -k\t--key\t\tThe keyboard key to trigger scrolling.\n");
				printf("\t\t\tThe argument is a hexadecimal KeySym which are listed in\n");
				printf("\t\t\t\"/usr/X11R6/include/X11/keysymdef.h\"\n");
				printf(" -t\t--threshhold\tHow many pixels the mouse must move to trigger\n");
				printf("\t\t\ta scroll event.\n");
				printf(" -y\t--hthreshhold\tHow many pixels the mouse must move to trigger\n");
				printf("\t\t\ta horizontal scroll event.\n");
				printf(" -i\t--init\tHow many pixels the mouse must move after the button \n");
				printf("\t\t\tis pressed before scrolling starts in a certain direction.\n");
				printf(" -I\t--invert\tInvert scrolling direction.\n");
				printf(" -T\t--toggle\tMake the button/key toggle scroll mode.\n");
				printf("\t\t\t(The button will not have to be held down)\n");
				printf(" -r\t--trap\tTraps the mouse and prevents movement while the button\n");
				printf("\t\t\tis pressed.\n");
				printf(" -v\t--version\tPrint out version information.\n");
				printf(" -h\t--help\t\tPrint this help message.\n");
				exit(0);
		}
	}
}

Display * open_display()
{
	Display * dpy = XOpenDisplay(NULL);
	
	if (dpy == NULL)
	{
		fprintf(stderr, "%s: could not open display \"%s\", aborting\n", prog_name, XDisplayName(NULL));
		exit(-1);
	}



	int i;
	nscreens = XScreenCount(dpy);
	roots = malloc(sizeof(Window) * nscreens);
	for (i = 0; i < nscreens; i++){
		roots[i] = XRootWindow(dpy, i);
	}


	int xrecord_major;
	int xrecord_minor;
	if (!XRecordQueryVersion(dpy, &xrecord_major, &xrecord_minor))
	{
		fprintf(stderr, "%s: XRecord extension not supported on server \"%s\", aborting\n", prog_name, XDisplayName(NULL));
		XCloseDisplay(dpy);
		exit(-2);
	}
#ifdef DEBUG
	fprintf(stderr, "XRecord for server \"%s\" is version %d.%d\n", XDisplayName(NULL), xrecord_major, xrecord_minor);
#endif
	
	int xtest_event;
	int xtest_error;
	int xtest_major;
	int xtest_minor;
	if (!XTestQueryExtension(dpy, &xtest_event, &xtest_error, &xtest_major, &xtest_minor))
	{
		fprintf(stderr, "%s: XTest extension not supported on server \"%s\", aborting\n", prog_name, XDisplayName(NULL));
		XCloseDisplay(dpy);
		exit(-2);
	}
#ifdef DEBUG
	fprintf(stderr, "XTest for server \"%s\" is version %d.%d\n", XDisplayName(NULL), xtest_major, xtest_minor);
#endif
	XTestGrabControl(dpy, True);
	XSync(dpy, True);
	
	return dpy;
}


void eventCallback(XPointer junk, XRecordInterceptData *d)
{
	unsigned char * ud1;
	unsigned short * ud2;
	short * d2;
	unsigned int * ud4;
	
	unsigned int type;
	unsigned int detail;
	unsigned int rootx;
	unsigned int rooty;
	
	static unsigned int lastx = 0;
	static unsigned int lasty = 0;
	static int togox;
	static int togoy;
	static unsigned int button_state = False;
	
	if (d->category == XRecordFromServer)
	{
		ud1 = (unsigned char *)d->data;
		ud2 = (unsigned short *)d->data;
		d2 = (short *)d->data;
		ud4 = (unsigned int *)d->data;
		
		type = (unsigned int)(ud1[0] & 0x7F);
		detail = (unsigned int)ud1[1];
		rootx = d2[10];
		rooty = d2[11];

		switch (type)
		{
			case ButtonPress:
			case KeyPress:
#ifdef DEBUG
				printf("button %d pressed\n", detail);
#endif
				if (detail == scroll_button)
				{
					start_x = rootx;
					start_y = rooty;
#ifdef DEBUG
					printf("start position (%d,%d)\n", start_x, start_y);
#endif

					if (toggle)
					{
						if (button_state)
						{
							button_state = False;
						} else {
							button_state = True;
						}
					} else {
						button_state = True;
					}
					togox = h_init;
					togoy = v_init;
				}
				break;
			case ButtonRelease:
			case KeyRelease:
#ifdef DEBUG
				printf("button %d released\n", detail);
#endif
				if (detail == scroll_button)
				{
					if (!toggle)
					{
						button_state = False;
					}
				}
				break;
			case MotionNotify:
#ifdef DEBUG
				printf("motion (%04d, %04d)\n", rootx, rooty);
#endif
				


				if (button_state && (rootx != start_x || rooty != start_y))
				{
					if (trap){
						Window root_win;

						root_win = XRootWindow(play, 0);
						XSelectInput(play, root_win, KeyReleaseMask);
						XWarpPointer(play, None, root_win, 0, 0, 0, 0, start_x, start_y);
						XFlush(play);
					}
					if (rooty < lasty)
					{
						togoy -= (lasty - rooty);
						while (togoy < 1)
						{
#ifdef DEBUG
							printf("scroll up\n");
#endif
							togoy += v_threshhold;
							if (scroll_vert)
							{
								if (invert)
								{
									XTestFakeButtonEvent(play, 5, True, 0);
									XTestFakeButtonEvent(play, 5, False, 0);
								}else{
									XTestFakeButtonEvent(play, 4, True, 0);
									XTestFakeButtonEvent(play, 4, False, 0);
								}

								XFlush(play);
							}
						}
					}
					if (rooty > lasty)
					{
						togoy -= (rooty - lasty);
						while (togoy < 1)
						{
#ifdef DEBUG
							printf("scroll down\n");
#endif
							togoy += v_threshhold;
							
							if (scroll_vert)
							{
								if (invert)
								{
									XTestFakeButtonEvent(play, 4, True, 0);
									XTestFakeButtonEvent(play, 4, False, 0);
								}else{
									XTestFakeButtonEvent(play, 5, True, 0);
									XTestFakeButtonEvent(play, 5, False, 0);
								}

								XFlush(play);
							}
						}
					}
					if (rootx < lastx)
					{
						togox -= (lastx - rootx);
						while (togox < 1)
						{
#ifdef DEBUG
							printf("scroll left\n");
#endif
							togox += h_threshhold;
							
							if (scroll_horz)
							{
								if (invert)
								{
									XTestFakeButtonEvent(play, 7, True, 0);
									XTestFakeButtonEvent(play, 7, False, 0);
								}else{
									XTestFakeButtonEvent(play, 6, True, 0);
									XTestFakeButtonEvent(play, 6, False, 0);
								}
								XFlush(play);
							}
						}
					}
					if (rootx > lastx)
					{
						togox -= (rootx - lastx);
						while (togox < 1)
						{
#ifdef DEBUG
							printf("scroll right\n");
#endif
							togox += h_threshhold;
							
							if (scroll_horz)
							{
								if (invert)
								{
									XTestFakeButtonEvent(play, 6, True, 0);
									XTestFakeButtonEvent(play, 6, False, 0);
								}else{
									XTestFakeButtonEvent(play, 7, True, 0);
									XTestFakeButtonEvent(play, 7, False, 0);
								}
								XFlush(play);
							}
						}
					}
				}
				lastx = rootx;
				lasty = rooty;
				break;
		}
	}
	XRecordFreeData(d);
}


int main(int argc, char * argv[])
{
	prog_name = argv[0];
	
	rec = open_display();
	play = open_display();
	
	XTestDiscard(play);

	parseargs(argc, argv);
	
	XRecordRange *rr;
	XRecordClientSpec rcs;
	XRecordContext rc;

	rr = XRecordAllocRange();
	if (rr == NULL)
	{
		fprintf(stderr, "%s: Could not alloc record range, aborting\n", prog_name);
		exit(-3);
	}
	rr->device_events.first = KeyPress;
	rr->device_events.last = MotionNotify;
	
	rcs = XRecordAllClients;
	
	rc = XRecordCreateContext(rec, 0, &rcs, 1, &rr, 1);
	if (!rc)
	{
		fprintf(stderr, "%s: Could not create a record context, aborting\n", prog_name);
		exit(-4);
	}
	
	if (!XRecordEnableContext(rec, rc, eventCallback, NULL))
	{
		fprintf(stderr, "%s: Could not enable the record context, aborting\n", prog_name);
		exit(-5);
	}
	
	while (True)
	{
		XRecordProcessReplies(rec);
	}
	
	if (!XRecordDisableContext(play, rc))
	{
		fprintf(stderr, "XRecordDisableContext failed!\n");
	}
	if (!XRecordFreeContext(play, rc))
	{
		fprintf(stderr, "XRecordFreeContext failed!\n");
	}
	XFree(rr);

	free(roots);

	XCloseDisplay(play);
	XCloseDisplay(rec);
	return 0;
}
