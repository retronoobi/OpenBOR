#include "../../openbor.c"
#undef printf
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void)
{
    s_level_entry entries[4] = {0};
    s_set_entry set = {0};
    set.levelorder=entries; set.numlevels=4;
    entries[0].type=entries[1].type=LE_TYPE_CUT_SCENE;
    entries[2].type=LE_TYPE_SKIP_SELECT; entries[2].noselect=1;
    entries[3].type=LE_TYPE_SELECT_SCREEN;
    CHECK(initial_selection_entry(&set,0)==entries+2);
    CHECK(initial_selection_entry(&set,0)->noselect==1);
    CHECK(initial_selection_entry(&set,2)==entries+2);
    entries[2].type=LE_TYPE_SELECT_SCREEN;
    CHECK(initial_selection_entry(&set,0)==entries+2);
    /* A gameplay entry must stop the scan, even if select appears later. */
    entries[1].type=LE_TYPE_NORMAL;
    CHECK(initial_selection_entry(&set,0)==entries+1);
    entries[0].type=LE_TYPE_NORMAL;
    CHECK(initial_selection_entry(&set,0)==entries);
    entries[3].type=LE_TYPE_CUT_SCENE;
    CHECK(initial_selection_entry(&set,3)==entries+3);
    puts("PASS: scenes precede explicit selection; gameplay and end-of-list stop the scan.");
    return 0;
}
