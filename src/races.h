#ifndef RACES_H
#define RACES_H
//*****************************************************************************
//
// races.h
//
// contains all of the information assocciated with different races. If you are
// wanting to add new races to the MUD, it is suggested you do so through the
// add_race() function, and make a new module for your MUD's races.
//
//*****************************************************************************

void init_races();
void add_race(const char *name, const char *abbrev, BODY_DATA *body, int pc_ok);
int raceCount();
bool isRace(const char *name);
BODY_DATA *raceCreateBody(const char *name);
bool raceIsForPC(const char *name);
const char *raceGetAbbrev(const char *name);
const char *raceGetList(bool pc_only);
const char *raceDefault(void);

#endif // RACES_H
