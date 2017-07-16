#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <X11/Xlib.h>

char *tzutc = "UTC";
char *tztunisia = "Africa/Tunis";



////////////////
// Battery stuff
///////////////

#define BATT_NOW        "/sys/class/power_supply/BAT0/energy_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/energy_full"
#define BATT_STATUS     "/sys/class/power_supply/BAT0/status"


char *
smprintf1(char *fmt, ...)
{
	va_list fmtargs;
	char *buf = NULL;

	va_start(fmtargs, fmt);
	if (vasprintf(&buf, fmt, fmtargs) == -1){
		fprintf(stderr, "malloc vasprintf\n");
		exit(1);
    }
	va_end(fmtargs);

	return buf;
}

char *
getbattery(){
    long lnum1, lnum2 = 0;
    char *status = malloc(sizeof(char)*12);
    char s = '?';
    FILE *fp = NULL;
    if ((fp = fopen(BATT_NOW, "r"))) {
        fscanf(fp, "%ld\n", &lnum1);
        fclose(fp);
        fp = fopen(BATT_FULL, "r");
        fscanf(fp, "%ld\n", &lnum2);
        fclose(fp);
        fp = fopen(BATT_STATUS, "r");
        fscanf(fp, "%s\n", status);
        fclose(fp);
        if (strcmp(status,"Charging") == 0)
            s = '+';
        if (strcmp(status,"Discharging") == 0)
            s = '-';
        if (strcmp(status,"Full") == 0)
            s = '=';
        return smprintf1("%c%ld%%", s,(lnum1/(lnum2/100)));
    }
    else return smprintf1("");
}





static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}


static int
get_vol(void)
{
        int vol;
        snd_hctl_t *hctl;
        snd_ctl_elem_id_t *id;
        snd_ctl_elem_value_t *control;

        // To find card and subdevice: /proc/asound/, aplay -L, amixer controls
        snd_hctl_open(&hctl, "hw:0", 0);
        snd_hctl_load(hctl);

        snd_ctl_elem_id_alloca(&id);
        snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

        // amixer controls
        snd_ctl_elem_id_set_name(id, "Master Playback Volume");

        snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);

        snd_ctl_elem_value_alloca(&control);
        snd_ctl_elem_value_set_id(control, id);

        snd_hctl_elem_read(elem, control);
        vol = (int)snd_ctl_elem_value_get_integer(control,0);

        snd_hctl_close(hctl);
        return vol;
}

int
main(void)
{
	char *status;
	char *avgs;
	char *tmbln;
        char *battery;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		avgs = loadavg();
		tmbln = mktimes("%a %d %b %H:%M:%S %Z %Y", tztunisia);
                battery = getbattery();
		status = smprintf("%d% L:%s %s %s",
                                  get_vol(), avgs, tmbln, battery);
		setstatus(status);
		free(avgs);
                free(battery);
                free(tmbln);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
