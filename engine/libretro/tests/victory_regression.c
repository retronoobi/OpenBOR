#include "openbor.h"
#include <stdio.h>
#include <string.h>
static entity actor,context;
static entity *self=&context;
static s_player player[MAX_PLAYERS];
static int endgame,level_completed_defeating_boss;
static s_anim victory;
static s_anim *animations[ANI_VICTORY+1];
static int calls;
void ent_set_anim(entity *e,int n,int reset) { ++calls; e->animnum=n; e->animating=1; }
#include "victory_controller.inc"
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
static void setup(void) {
 memset(&actor,0,sizeof(actor)); memset(player,0,sizeof(player));
 memset(animations,0,sizeof(animations)); victory.numframes=3;
 animations[ANI_VICTORY]=&victory; actor.modeldata.animation=animations;
 actor.idling=IDLING_ACTIVE; actor.inpain=IN_PAIN_NONE; actor.animnum=ANI_IDLE;
 player[0].ent=&actor; endgame=1; level_completed_defeating_boss=1; calls=0; self=&context;
}
int main(void) {
 setup(); check_victory_pose();
 CHECK(calls==1 && actor.animnum==ANI_VICTORY && endgame==0 && self==&context);
 endgame=1; check_victory_pose(); CHECK(endgame==0 && calls==1);
 actor.animating=0; endgame=1; check_victory_pose(); CHECK(endgame==1 && calls==1);
 setup(); actor.inpain=IN_PAIN_BLOCK; check_victory_pose(); CHECK(calls==0 && endgame==0);
 actor.inpain=IN_PAIN_NONE; endgame=1; check_victory_pose(); CHECK(calls==1);
 setup(); actor.falling=1; tryvictorypose(&actor); CHECK(calls==0);
 setup(); actor.death_state=DEATH_STATE_DEAD; tryvictorypose(&actor); CHECK(calls==0);
 setup(); actor.rising=1; tryvictorypose(&actor); CHECK(calls==0);
 setup(); actor.position.y=1; tryvictorypose(&actor); CHECK(calls==0);
 setup(); actor.idling=IDLING_NONE; tryvictorypose(&actor); CHECK(calls==0);
 setup(); animations[ANI_VICTORY]=NULL; check_victory_pose(); CHECK(calls==0 && endgame==1);
 setup(); level_completed_defeating_boss=0; check_victory_pose(); CHECK(calls==0 && endgame==1);
 puts("PASS: victory starts when idle, waits for animation, then releases level completion."); return 0;
}
