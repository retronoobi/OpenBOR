/* Compile the engine's actual legacy flag converters, pruning unused code. */
#include "../../openbor.c"
#undef printf
int main(void)
{
    for(int fall=FALLDIE_CONFIG_DEATH_INSTANT;fall<=FALLDIE_CONFIG_DEATH_FALL;++fall)
        for(int blink=NODIEBLINK_CONFIG_NONE;blink<=NODIEBLINK_CONFIG_FALL_LIE_CORPSE;++blink)
        {
            e_death_config_flags a=death_config_get_value_from_falldie(DEATH_CONFIG_MACRO_DEFAULT,fall);
            a=death_config_get_value_from_nodieblink(a,blink);
            e_death_config_flags b=death_config_get_value_from_nodieblink(DEATH_CONFIG_MACRO_DEFAULT,blink);
            b=death_config_get_value_from_falldie(b,fall);
            unsigned expected=fall==FALLDIE_CONFIG_DEATH_INSTANT?0:(DEATH_CONFIG_FALL_LAND_AIR|DEATH_CONFIG_FALL_LAND_GROUND);
            if(a!=b || (a & DEATH_CONFIG_MACRO_FALL)!=expected ||
               (a & DEATH_CONFIG_MACRO_DEATH)!=DEATH_CONFIG_MACRO_DEATH) {
                fprintf(stderr,"falldie=%d nodieblink=%d flags=%x/%x\n",fall,blink,a,b);
                return 1;
            }
        }
    puts("PASS: explicit falldie preserved for all nodieblink values in both declaration orders.");
    return 0;
}
