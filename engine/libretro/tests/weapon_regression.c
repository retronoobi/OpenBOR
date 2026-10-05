#include "openbor.h"
#include <stdio.h>
#include <string.h>
static entity actor, weapon;
static entity *self=&actor;
static s_player player[MAX_PLAYERS];
static s_level stage;
static s_level *level=&stage;
static unsigned _time;
static int calls, last_weapon;
void set_weapon(entity *e,int wpnum,int anim) { ++calls; last_weapon=wpnum; }
entity *check_platform(float x,float z,entity *exclude) { return NULL; }
int checkwall_index(float x,float z) { return -1; }
void ent_set_anim(entity *e,int num,int reset) {}
void common_lie(void) {}
void runanimal(void) {}
#include "weapon_controller.inc"
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
static void setup(void)
{
    memset(&actor,0,sizeof(actor)); memset(&weapon,0,sizeof(weapon));
    actor.modeldata.type=TYPE_PLAYER;
    /* Legacy "weaploss 3" has an omitted loss index, interpreted as zero. */
    actor.modeldata.weapon_properties.loss_index=0;
    calls=0; last_weapon=-1;
}
int main(void)
{
    setup(); dropweapon(2); CHECK(calls==0);
    actor.modeldata.weapon_properties.loss_index=3;
    dropweapon(2); CHECK(calls==0);
    actor.weapent=&weapon;
    dropweapon(2); CHECK(calls==0 && actor.weapent==NULL);
    setup(); dropweapon(1); CHECK(calls>0 && last_weapon==0);
    setup(); actor.modeldata.weapon_properties.loss_index=3;
    dropweapon(1); CHECK(calls>0 && last_weapon==3);
    setup(); actor.modeldata.weapon_properties.loss_index=MODEL_INDEX_NONE;
    player[0].weapnum=2;
    dropweapon(0); CHECK(calls==1 && last_weapon==2);
    puts("PASS: explicit weapon changes preserve current model; actual drops retain loss rules.");
    return 0;
}
