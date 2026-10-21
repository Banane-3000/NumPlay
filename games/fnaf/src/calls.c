/* The phone calls of nights 1 to 5, as subtitles: the calculator has no sound, so the Phone Guy's
 * messages are read, each line from the second it is said in the recording. */
#include <stddef.h>
#include "game.h"

typedef struct {
  float at;
  const char *s;
} Line;

static const Line night1[] = {
  {3.4f, "*phone ringing*"},
  {18.9f, "Hello? Hello, hello?"},
  {21.7f, "Uh, I wanted to record a message for you"},
  {24.7f, "to help you get settled in on your first night."},
  {27.5f, "Um, I actually worked in that office before you."},
  {31.1f, "I'm finishing up my last week now, as a matter of fact."},
  {34.2f, "So, I know it can be a bit overwhelming,"},
  {37.8f, "but I'm here to tell you there's nothing to worry about."},
  {40.7f, "Uh, you'll do fine."},
  {42.4f, "So, let's just focus on getting you through your first week."},
  {46.5f, "Okay?"},
  {47.3f, "Uh, let's see, first there's an introductory greeting"},
  {50.0f, "from the company that I'm supposed to read."},
  {53.1f, "Uh, it's kind of a legal thing, you know."},
  {55.8f, "Um, \"Welcome to Freddy Fazbear's Pizza."},
  {58.9f, "A magical place for kids and grown-ups alike,"},
  {61.9f, "where fantasy and fun come to life."},
  {64.6f, "Fazbear Entertainment is not responsible for damage to property or person."},
  {69.1f, "Upon discovering that damage or death has occurred,"},
  {71.5f, "a missing person report will be filed within 90 days,"},
  {74.4f, "or as soon property and premises have been thoroughly cleaned and bleached,"},
  {78.3f, "and the carpets have been replaced.\" Blah blah blah."},
  {81.3f, "Now that might sound bad, I know,"},
  {83.6f, "but there's really nothing to worry about."},
  {86.4f, "Uh, the animatronic characters here do get a bit quirky at night,"},
  {90.9f, "but do I blame them? No."},
  {92.9f, "If I were forced to sing those same stupid songs for twenty years"},
  {96.3f, "and I never got a bath?"},
  {98.5f, "I'd probably be a bit irritable at night too."},
  {101.1f, "So, remember, these characters hold a special place in the hearts of children"},
  {105.2f, "and we need to show them a little respect, right? Okay."},
  {109.1f, "So, just be aware, the characters do tend to wander a bit."},
  {114.0f, "Uh, they're left in some kind of free roaming mode at night."},
  {117.9f, "Uh... Something about their servos locking up"},
  {120.5f, "if they get turned off for too long."},
  {122.6f, "Uh, they used to be allowed to walk around during the day too."},
  {125.5f, "But then there was The Bite of '87. Yeah."},
  {129.8f, "I-It's amazing that the human body can live without the frontal lobe,"},
  {133.5f, "you know?"},
  {134.4f, "Uh, now concerning your safety, the only real risk to you as a night watchman here,"},
  {140.7f, "if any, is the fact that these characters, uh, if they happen to see you after hours"},
  {148.9f, "probably won't recognize you as a person."},
  {150.4f, "They'll p-most likely see you as a metal endoskeleton without its costume on."},
  {155.6f, "Now since that's against the rules here at Freddy Fazbear's Pizza,"},
  {159.4f, "they'll probably try to... forcefully stuff you inside a Freddy Fazbear suit."},
  {163.6f, "Um, now, that wouldn't be so bad if the suits themselves weren't filled with"},
  {168.6f, "crossbeams, wires, and animatronic devices, especially around the facial area."},
  {174.5f, "So, you could imagine how having your head forcefully pressed inside one of those"},
  {178.3f, "could cause a bit of discomfort... and death."},
  {182.4f, "Uh, the only parts of you that would likely see the light of day again"},
  {185.1f, "would be your eyeballs and teeth when they pop out the front of the mask, heh."},
  {189.3f, "Y-Yeah, they don't tell you these things when you sign up."},
  {192.4f, "But hey, first day should be a breeze."},
  {194.4f, "I'll chat with you tomorrow."},
  {195.9f, "Uh, check those cameras, and remember to close the doors only if absolutely necessary."},
  {201.0f, "Gotta conserve power."},
  {202.7f, "Alright, good night."},
  {204.2f, NULL},
};

static const Line night2[] = {
  {3.6f, "*phone ringing*"},
  {18.7f, "Uhh, hello, hello?"},
  {21.0f, "Uh, well if you're hearing this and you made it to day two, uh, congrats!"},
  {25.7f, "I-I won't talk quite as long this time"},
  {28.6f, "since Freddy and his friends tend to become more active as the week progresses."},
  {32.9f, "Uhh, it might be a good idea to peek at those cameras while I talk"},
  {36.7f, "just to make sure everyone's in their proper place, you know."},
  {40.1f, "Uh, interestingly enough, Freddy himself doesn't come off stage very often."},
  {45.1f, "I heard he becomes a lot more active in the dark, though,"},
  {49.5f, "so, hey, I guess that's one more reason not to run out of power, right?"},
  {53.0f, "I-I also want to emphasize the importance of using your door lights."},
  {57.5f, "There are blind spots in your camera views,"},
  {60.8f, "and those blind spots happen to be right outside of your doors."},
  {64.8f, "So if-if you can't find something, or someone, on your cameras,"},
  {69.0f, "be sure to check the door lights."},
  {71.7f, "Uh, you might only have a few seconds to react..."},
  {74.8f, "Uh, not that you would be in any danger, of course. I'm not implying that."},
  {80.3f, "Also, check on the curtain in Pirate Cove from time to time."},
  {85.4f, "The character in there seems unique in that he becomes more active"},
  {88.3f, "if the cameras remain off for long periods of time."},
  {92.4f, "I guess he doesn't like being watched. I don't know."},
  {95.5f, "Anyway, I'm sure you have everything under control."},
  {98.7f, "Uh, talk to you soon."},
  {100.4f, NULL},
};

static const Line night3[] = {
  {5.2f, "*phone ringing*"},
  {19.7f, "Hello, hello?"},
  {21.0f, "Hey, you're doing great! Most people don't last this long."},
  {25.1f, "I mean, you know, they usually move on to other things by now."},
  {29.4f, "I'm not implying that they died. Th-That's not what I meant."},
  {34.0f, "Uh anyway, I better not take up too much of your time."},
  {37.4f, "Things start getting real tonight."},
  {39.7f, "Uh, hey, listen, I had an idea:"},
  {42.7f, "if you happen to get caught and want to avoid getting stuffed into a Freddy Fazbear suit,"},
  {47.7f, "uh, try playing dead! You know, go limp."},
  {51.3f, "Then there's a chance that, uh, maybe they'll think that you're an empty costume instead."},
  {56.3f, "Then again if they think you're an empty costume,"},
  {59.8f, "they-they might try to... stuff a metal endoskeleton through you."},
  {62.3f, "Wonder how that would work."},
  {64.0f, "Uh, yeah, never mind, scratch that. It's best just not to get caught."},
  {68.4f, "Uh, okay, I'll leave you to it."},
  {71.4f, "See you on the flip side."},
  {72.5f, NULL},
};

static const Line night4[] = {
  {3.3f, "*phone ringing*"},
  {17.6f, "Hello, hello? Hey!"},
  {19.4f, "Hey, wow, day four. I knew you could do it."},
  {23.3f, "Uh, hey, listen, I may not be around to send you a message tomorrow."},
  {26.7f, "*banging* It's been a bad night here for me."},
  {30.5f, "Um, I-I'm kinda glad that I recorded my messages for you, uh, when I did."},
  {36.3f, "Uh, hey, do me a favor. Maybe sometime, uh,"},
  {39.9f, "you could check inside those suits in the back room?"},
  {43.7f, "I'm gonna try to hold out until someone checks. Maybe it won't be so bad."},
  {49.1f, "Uh, I-I-I always wondered what was in all those empty heads back there. You know..."},
  {52.7f, "*a music box* Oh, no..."},
  {55.1f, "*moaning*"},
  {56.4f, "Oh, no..."},
  {57.3f, "*banging*"},
  {58.3f, "*an animatronic screams*"},
  {58.8f, "*static*"},
  {60.4f, "*the line goes dead*"},
  {62.0f, NULL},
};

static const Line night5[] = {
  {3.9f, "*phone ringing*"},
  {17.3f, "*static*"},
  {18.8f, "*garbled, deep voices*"},
  {26.7f, "*a scream*"},
  {27.0f, "*garbled, deep voices*"},
  {30.0f, "*a scream*"},
  {30.3f, "*garbled, deep voices*"},
  {33.3f, "*a scream*"},
  {33.6f, "*garbled, deep voices*"},
  {34.6f, "*a high scream*"},
  {35.5f, "*the line goes dead*"},
  {36.3f, NULL},
};

static const Line *const calls[5] = {night1, night2, night3, night4, night5};

const char *call_line(int night, float t) {
  if (night < 1 || night > 5) return NULL;
  const char *s = NULL;
  for (const Line *l = calls[night - 1]; l->at <= t; l++) {
    s = l->s;
    if (!s) break;
  }
  return s;
}

float call_end(int night) {
  if (night < 1 || night > 5) return 0;
  const Line *l = calls[night - 1];
  while (l->s) l++;
  return l->at;
}
