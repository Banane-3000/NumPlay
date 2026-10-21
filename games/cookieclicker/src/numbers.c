/* Numbers as the game writes them (its Beautify, formatEveryThirdPower, sayTime): 123,456, then
 * 1.234 million ... up to the novemnonagintillions; with short numbers off, every digit with commas. */
#include <math.h>
#include <string.h>
#include "game.h"

static const char *const small_names[10] = {" thousand", " million", " billion", " trillion", " quadrillion",
                                            " quintillion", " sextillion", " septillion", " octillion", " nonillion"};
static const char *const prefixes[10] = {"", "un", "duo", "tre", "quattuor", "quin", "sex", "septen", "octo", "novem"};
static const char *const suffixes[9] = {"decillion", "vigintillion", "trigintillion", "quadragintillion",
                                        "quinquagintillion", "sexagintillion", "septuagintillion", "octogintillion",
                                        "nonagintillion"};

static char *cpy(char *o, const char *s) {
  while ((*o = *s++)) o++;
  return o;
}
static char *notation(char *o, int base) {
  if (base < 10) return cpy(o, small_names[base]);
  base -= 10;
  *o++ = ' ';
  return cpy(cpy(o, prefixes[base % 10]), suffixes[base / 10]);
}
/* an integer below 2^53 with commas */
static char *integer(char *o, double v, bool commas) {
  char t[40];
  int n = 0, digits = 0;
  v = floor(v + 0.5);
  do {
    if (commas && digits && digits % 3 == 0) t[n++] = ',';
    double q = floor(v / 10);
    t[n++] = (char)('0' + (int)(v - q * 10));
    v = q, digits++;
  } while (v >= 1 && n < 38);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
/* JavaScript's Math.round(v * 1000) / 1000, written as it writes numbers */
static char *round3(char *o, double v) {
  double r = floor(v * 1000 + 0.5);
  double ip = floor(r / 1000);
  int frac = (int)(r - ip * 1000);
  o = integer(o, ip, false);
  if (frac) {
    *o++ = '.';
    int d = 100;
    while (frac) *o++ = (char)('0' + frac / d), frac %= d, d /= 10;
    *o = 0;
  }
  return o;
}

void beautify(double val, int floats, char *out) {
  char *o = out;
  bool negative = val < 0;
  double a = fabs(val);
  char decimal[8] = "";
  if (isinf(a) || isnan(a)) {
    strcpy(out, "Infinity");
    return;
  }
  if (floats > 0) {
    double p = pow(10, floats);
    double fixed = floor(a * p + 0.5) / p;
    if (a < 1000 && floor(fixed) != fixed) {
      int f = (int)floor((fixed - floor(fixed)) * p + 0.5);
      decimal[0] = '.';
      for (int i = floats - 1; i >= 0; i--) decimal[1 + i] = (char)('0' + f % 10), f /= 10;
      decimal[1 + floats] = 0;
    }
    double fl = floor(a);
    if (fixed == fl + 1) a = fl + 1;
    else a = fl;
  } else
    a = floor(a);
  if (a == 0) negative = false;
  if (negative) *o++ = '-';
  if (S.prefs.format_long) {   /* short numbers off */
    if (a >= 1e21) {
      int e = (int)floor(log10(a));
      double m = a / pow(10, e);
      if (m >= 9.995) m /= 10, e++;
      int c = (int)floor(m * 100 + 0.5);
      *o++ = (char)('0' + c / 100), *o++ = '.', *o++ = (char)('0' + c / 10 % 10), *o++ = (char)('0' + c % 10);
      *o++ = 'e', *o++ = '+';
      o = integer(o, e, false);
    } else
      o = integer(o, a, true);
  } else if (a >= 1000000) {
    double v = a / 1000;
    int base = 0;
    while (floor(v + 0.5) >= 1000) v /= 1000, base++;
    if (base >= 100) {
      strcpy(out, negative ? "-Infinity" : "Infinity");
      return;
    }
    o = notation(round3(o, v), base);
  } else
    o = integer(o, a, true);
  strcpy(o, decimal);
}

void cookies_str(double v, int floats, char *out) {
  beautify(v, floats, out);
  bool one = floats > 0 ? fabs(v - 1) < 1e-9 : floor(fabs(v)) == 1;
  strcat(out, one ? " cookie" : " cookies");
}

static char *unit(char *o, long n, const char *name) {
  char t[32];
  beautify((double)n, 0, t);
  o = cpy(cpy(o, t), " ");
  o = cpy(o, name);
  if (n != 1) *o++ = 's', *o = 0;
  return o;
}

void say_time(double time, int detail, char *out) {
  out[0] = 0;
  if (time <= 0) return;
  long t = (long)floor(time);
  const long D = (long)FPS * 60 * 60 * 24, H = (long)FPS * 60 * 60, M = (long)FPS * 60;
  char *o = out;
  if (detail == -1) {
    long days = t >= D ? t / D : 0, hours = t >= H ? t / H : 0, minutes = t >= M ? t / M : 0;
    long seconds = t >= FPS ? t / FPS : 0;
    hours -= days * 24;
    minutes -= hours * 60 + days * 24 * 60;
    seconds -= minutes * 60 + hours * 60 * 60 + days * 24 * 60 * 60;
    if (days > 10) hours = 0;
    if (days) minutes = 0, seconds = 0;
    if (hours) seconds = 0;
    const long v[4] = {days, hours, minutes, seconds};
    static const char *const names[4] = {"day", "hour", "minute", "second"};
    for (int i = 0; i < 4; i++) {
      if (v[i] <= 0) continue;
      if (o != out) o = cpy(o, ", ");
      o = unit(o, v[i], names[i]);
    }
    if (o == out) strcpy(out, "less than 1 second");
    return;
  }
  if (t >= D && detail < 2) unit(o, t / D, "day");
  else if (t >= H && detail < 3) unit(o, t / H, "hour");
  else if (t >= M && detail < 4) unit(o, t / M, "minute");
  else if (t >= FPS && detail < 5) unit(o, t / FPS, "second");
  else strcpy(out, "less than 1 second");
}
