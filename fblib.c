#include "fblib.h"

#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
		
#define UNUSED(Var) (void)Var

#define Die(Msg, ...) { \
    fprintf (stderr, "fbgrad: " Msg ".\n", __VA_ARGS__); \
    exit(1); \
}\

#define Assumption(Cond, Msg) \
    if (!(Cond)) { \
        fprintf (stderr, "fbgrad: failed assumption: %s\n", Msg);\
        exit(2);\
    }


int ttyfd, fbfd;
Screen s;

void cleanup (int signum) {
    UNUSED(signum);
    
    munmap (s.buffer, s.size);

#ifdef GRAB_TTY
    if (ioctl (ttyfd, KDSETMODE, KD_TEXT) == -1)
        Die ("cannot set tty into text mode on \"%s\"", ttydev);

    close (ttyfd);
#endif

    close (fbfd);
}

int main (int argc, char **argv) {
    signal (SIGINT, cleanup);
    signal (SIGSEGV, cleanup);

#ifdef GRAB_TTY
    ttyfd = open (ttydev, O_RDWR); if (ttyfd < 0)
        Die ("cannot open \"%s\"", ttydev);

    if (ioctl (ttyfd, KDSETMODE, KD_GRAPHICS) == -1)
        Die ("cannot set tty into graphics mode on \"%s\"", ttydev);
#endif

    fbfd = open (fbdev, O_RDWR);
    if (fbfd < 0)
        Die ("cannot open \"%s\"", fbdev);

    struct fb_var_screeninfo vinf;
    struct fb_fix_screeninfo finf;

    if (ioctl (fbfd, FBIOGET_FSCREENINFO, &finf) == -1)
        Die ("cannot open fixed screen info for \"%s\"", fbdev);

    if (ioctl (fbfd, FBIOGET_VSCREENINFO, &vinf) == -1)
        Die ("cannot open variable screen info for \"%s\"", fbdev);

    Assumption ((vinf.red.offset%8) == 0 && (vinf.red.length == 8) &&
                (vinf.green.offset%8) == 0 && (vinf.green.length == 8) &&
                (vinf.blue.offset%8) == 0 && (vinf.blue.length == 8) &&
                vinf.xoffset == 0 && vinf.yoffset == 0 &&
                vinf.red.msb_right == 0 &&
                vinf.green.msb_right == 0 &&
                vinf.blue.msb_right == 0,
                "Color masks are 8bit, byte aligned, little endian."
    );

    s = (Screen) {
        .size            = finf.line_length * vinf.yres,
        .bytes_per_pixel = vinf.bits_per_pixel / 8,
        .bytes_per_line  = finf.line_length,
        .red             = vinf.red.offset/8,
        .green           = vinf.green.offset/8,
        .blue            = vinf.blue.offset/8,
        .width           = vinf.xres,
        .height          = vinf.yres
    };

    s.buffer = mmap (0, s.size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (s.buffer == MAP_FAILED)
        Die ("cannot map frame buffer \"%s\"", fbdev);

    fb_main(s, (Strings){ .count = argc, .vals = argv });

    cleanup(0);

    return EXIT_SUCCESS;
}
