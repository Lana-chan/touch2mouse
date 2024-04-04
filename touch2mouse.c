#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <linux/input.h>
#include <linux/uinput.h>

void emit(int fd, int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	/* timestamp values below are ignored */
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	write(fd, &ie, sizeof(ie));
}

/* Change it to your dev file for the touch screen */
#define EVENT_DEVICE    "/dev/input/event1"
#define UINPUT_DEVICE   "/dev/uinput"

static int infd = -1;
static int outfd = -1;
static int old_x = -1;
static int old_y = -1;
static volatile int running = 1;

void intHandler(int dummy) {
	running = 0;
}

int main(int argc, char *argv[])
{
	struct uinput_setup usetup;
	struct input_event ev;
	struct input_absinfo abs_feat;
	char name[256] = "Unknown";
	fd_set readfds;

	/* /dev/input/event* files are only readable by root:input */
	if ((getuid()) != 0) {
		fprintf(stderr, "You must be root to run this program\n");
		return EXIT_FAILURE;
	}

	/* Open device for non-blocking read */
	infd = open(EVENT_DEVICE, O_RDONLY | O_NONBLOCK);
	if (infd == -1) {
		fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE);
		goto err;
	}

	/* Open uinput for non-blocking write */
	outfd = open(UINPUT_DEVICE, O_WRONLY | O_NONBLOCK);
	if (outfd == -1) {
		fprintf(stderr, "Failed to open uinput\n");
		goto err;
	}

	/* Print input device name */
	ioctl(infd, EVIOCGNAME(sizeof(name)), name);
	printf("Reading from:\n");
	printf("  - device file: %s\n", EVENT_DEVICE);
	printf("  - device name: %s\n", name);

	/* Prepare for select(): zero and set the fd into fd_set */
	FD_ZERO(&readfds);
	FD_SET(infd, &readfds);

	/* exclusive access */
	//ioctl(infd, EVIOCGRAB, (void *)1);

	/* enable output mouse button left */
	ioctl(outfd, UI_SET_EVBIT, EV_SYN);
	ioctl(outfd, UI_SET_EVBIT, EV_KEY);
	ioctl(outfd, UI_SET_KEYBIT, BTN_LEFT);

	/* set up relative xy */
	ioctl(outfd, UI_SET_EVBIT, EV_REL);
	ioctl(outfd, UI_SET_RELBIT, REL_X);
	ioctl(outfd, UI_SET_RELBIT, REL_Y);

	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234; /* sample vendor */
	usetup.id.product = 0x5678; /* sample product */
	strcpy(usetup.name, "Example device");

	ioctl(outfd, UI_DEV_SETUP, &usetup);
	ioctl(outfd, UI_DEV_CREATE);
	
	signal(SIGINT, intHandler);
	
	// let system enumerate our virtual device
	sleep(1);

	/* Press Ctrl-C to stop the program */
	while (running) {
		const size_t ev_size = sizeof(struct input_event);
		ssize_t size;
		int ret;
		/* struct timeval timeout = { 30, 0 }; */

		/*
		* select(): no-timeout version, just stop on errors or
		* interrupts.
		*/
		ret = select(infd + 1, &readfds, NULL, NULL, NULL);
		/*
		* select(): timeout-enabled version, stop if no event has
		* occurred until timeout; might be useful e.g. for background
		* tasks.
		*/
		/* ret = select(fd + 1, &readfds, NULL, NULL, &timeout); */
		if (ret == -1) {
			if (!running) break;
			perror("Error: select() failed");
			goto err;
		} else if (ret == 0) {
			fprintf(stderr, "Error: select() timeout\n");
			continue;
		}

		size = read(infd, &ev, ev_size);
		if (size < ev_size) {
			fprintf(stderr, "Error: Wrong size when reading\n");
			goto err;
		}

		#ifdef DEBUG
		printf("touch: %d %d %d\n", ev.type, ev.code, ev.value);
		#endif
		if (ev.type == EV_KEY) {
			if (ev.code == BTN_TOUCH) {
				//printf("send click: %d\n", ev.value);
				emit(outfd, ev.type, BTN_LEFT, ev.value);
				old_x = -1;
				old_y = -1;
			}
		} else if (ev.type == EV_ABS) {
			// this is a hack to make mouse dragging work, EV_ABS also doesn't update
			// the first touch updates absolute position, and subsequent ones are ignored
			// so we assume a 16:9 screen and use absolute deltas to move relative
			if (ev.code == ABS_X) {
				if (old_x != -1) emit(outfd, EV_REL, REL_X, (old_x - ev.value) / 16);
				old_x = ev.value;
			} else {
				if (old_y != -1) emit(outfd, EV_REL, REL_Y, (old_y - ev.value) / 9);
				old_y = ev.value;
			}
		} else {
			emit(outfd, ev.type, ev.code, ev.value);
		}
	}

	ioctl(outfd, UI_DEV_DESTROY);
	close(infd);
	close(outfd);
	return EXIT_SUCCESS;

err:
	ioctl(outfd, UI_DEV_DESTROY);
	close(infd);
	close(outfd);
	return EXIT_FAILURE;
}