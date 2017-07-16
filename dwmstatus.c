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
        long vol;
        long max, min;
        snd_mixer_t *handle;
        snd_mixer_selem_id_t *sid;
        const char *card = "default";
        const char *selem_name = "Master";

        snd_mixer_open(&handle, 0);
        snd_mixer_attach(handle, card);
        snd_mixer_selem_register(handle, NULL, NULL);
        snd_mixer_load(handle);

        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, selem_name);
        snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

        snd_mixer_selem_get_playback_volume(elem, 0, &vol);
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

        snd_mixer_close(handle);
        return (int) ((float )vol / max  * 100);
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
