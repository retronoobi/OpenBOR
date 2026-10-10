#include "openbor.h"
#include <stdio.h>
#include <string.h>
static s_attack emptyattack;
static s_axis_principal_float default_model_dropv;
static unsigned _time;
static int released;
/* Poison removed nodes while retaining test-owned storage: traversing their
 * next pointer afterwards must not skip the surviving effect. */
static void release_node(void *p) { ++released; memset(p,0,sizeof(s_damage_recursive)); }
void recursive_damage_free_node(s_damage_recursive **list,s_damage_recursive *node) {
    while(*list && *list!=node) list=&(*list)->next;
    if(*list) { *list=node->next; release_node(node); }
}
s_defense *defense_find_current_object(entity *e,s_body *b,e_attack_types t) { return NULL; }
int calculate_force_damage(entity *e,entity *o,s_attack *a,s_defense *d) { return a->attack_force; }
void kill_entity(entity *e,e_kill_entity_trigger t) { e->exists=0; }
void execute_takedamage_script(entity *e,entity *o,s_attack *a) {}
#define free release_node
#include "dot_controller.inc"
#undef free
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void) {
    entity ent={0};
    s_damage_recursive a={0}, b={0}, c={0};
    _time=100; ent.energy_state.health_current=10; ent.energy_state.mp_current=10;
    a.next=&b; b.next=&c; c.time=1000; c.mode=DAMAGE_RECURSIVE_MODE_MP; c.force=3; c.rate=80;
    ent.recursive_damage=&a;
    recursive_damage_update(&ent);
    CHECK(released==2 && ent.recursive_damage==&c && ent.energy_state.mp_current==7);
    recursive_damage_update(&ent); CHECK(ent.energy_state.mp_current==7);
    _time=1001; recursive_damage_update(&ent);
    CHECK(released==3 && ent.recursive_damage==NULL);
    puts("PASS: consecutive expired DOT nodes are removed without skipping active effects.");
    return 0;
}
