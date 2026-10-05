/* Real controller, real engine types; only animation side effects are mocked. */
#include "openbor.h"
#include <stdio.h>
#include <string.h>
static entity actor;
static entity *self = &actor;
static s_player player[MAX_PLAYERS];
static s_anim block_animation, release_animation;
static s_anim *animations[ANI_BLOCKRELEASE + 1];
static int idle_calls, animation_calls;
int set_idle(entity *ent) { ++idle_calls; ent->animnum=ANI_IDLE; return 1; }
void ent_set_anim(entity *ent, int num, int reset)
{
    if(!reset && ent->animnum==num) return;
    ++animation_calls; ent->animnum=num; ent->animating=1;
}
#include "block_controller.inc"
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
static void setup(void)
{
    memset(&actor,0,sizeof(actor)); memset(player,0,sizeof(player));
    memset(animations,0,sizeof(animations));
    block_animation.numframes=12; release_animation.numframes=3;
    animations[ANI_BLOCK]=&block_animation;
    actor.modeldata.animation=animations;
    actor.modeldata.type=TYPE_PLAYER;
    actor.modeldata.block_config_flags=BLOCK_CONFIG_HOLD_IMPACT;
    actor.animnum=ANI_BLOCK; actor.animating=1; actor.blocking=1;
    actor.takeaction=common_block;
    idle_calls=animation_calls=0;
}
int main(void)
{
    setup(); player[0].keys=FLAG_SPECIAL;
    common_block(); CHECK(actor.blocking && idle_calls==0);
    player[0].keys=0;
    common_block(); CHECK(!actor.blocking && !actor.takeaction && idle_calls==1);

    setup(); actor.inpain=IN_PAIN_BLOCK;
    common_block(); CHECK(actor.blocking && idle_calls==0);
    actor.animating=0;
    common_block(); CHECK(!actor.blocking && idle_calls==1);

    setup(); animations[ANI_BLOCKRELEASE]=&release_animation;
    common_block(); CHECK(actor.animnum==ANI_BLOCKRELEASE && animation_calls==1);
    common_block(); CHECK(actor.blocking && animation_calls==1 && idle_calls==0);
    actor.animating=0;
    common_block(); CHECK(!actor.blocking && idle_calls==1);

    setup(); actor.modeldata.block_config_flags=0;
    common_block(); CHECK(actor.blocking && idle_calls==0);
    actor.animating=0;
    common_block(); CHECK(!actor.blocking && idle_calls==1);
    puts("PASS: held block, looping release, blockstun, release animation and non-held block.");
    return 0;
}
