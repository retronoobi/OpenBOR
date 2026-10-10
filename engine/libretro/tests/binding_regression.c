#include "openbor.h"
#include <stdio.h>
#include <string.h>
static int kills, starts;
void execute_on_bind_update_other_to_self(entity *e, entity *o, s_bind *b) {}
void execute_on_bind_update_self_to_other(entity *e, entity *o, s_bind *b) {}
void kill_entity(entity *e, e_kill_entity_trigger t) { e->exists=0; ++kills; }
void ent_set_anim(entity *e, int n, int r) { e->animation=e->modeldata.animation[n]; e->animnum=n; ++starts; }
void update_frame(entity *e, int f) { e->animpos=f; }
e_direction direction_get_adjustment_result(entity *e, const entity *o, e_direction_adjust d) { return o->direction; }
#include "binding_controller.inc"
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void) {
    entity child={0}, parent={0};
    s_anim animation={0};
    s_anim *animations[ANI_ATTACK+1]={0};
    animation.numframes=4;
    animations[ANI_ATTACK]=&animation;
    child.modeldata.animation=animations;
    child.exists=1;
    child.binding.target=&parent;
    child.binding.config=4; /* Legacy removal flag alone must match animation. */
    parent.animnum=ANI_ATTACK;
    adjust_bind(&child);
    CHECK(starts==1 && child.animation==&animation && child.exists && !kills);
    parent.animnum=ANI_IDLE; /* Missing in effect model: remove it. */
    adjust_bind(&child);
    CHECK(kills==1 && !child.exists && !child.binding.target);
    child.exists=1; child.binding.target=&parent; child.animation=NULL;
    child.animnum=ANI_IDLE; animations[ANI_IDLE]=&animation;
    adjust_bind(&child);
    CHECK(starts==2 && child.animation==&animation && child.exists);
    child.binding.config=0; child.animation=NULL; starts=0;
    adjust_bind(&child); CHECK(starts==0 && child.animation==NULL);
    child.binding.config=6; parent.animnum=ANI_ATTACK; parent.animpos=2;
    adjust_bind(&child); CHECK(child.animation==&animation && child.animpos==2);
    puts("PASS: legacy binding initializes matching animations and removes missing ones.");
    return 0;
}
