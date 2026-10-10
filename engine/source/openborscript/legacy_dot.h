/* Compatibility with the old indexed entity "dot" API. The current engine
 * stores effects in a linked list; use their index, not their list position. */
static int legacy_dot_property(ScriptVariant *value)
{
    static const char *names[]={"force","mode","owner","rate","time","type"};
    int i;
    if(value->vt==VT_INTEGER) return value->lVal>=0 && value->lVal<6 ? value->lVal : -1;
    if(value->vt!=VT_STR) return -1;
    for(i=0;i<6;++i) if(!stricmp(StrCache_Get(value->strVal),names[i])) return i;
    return -1;
}

static int legacy_dot_mode(e_damage_recursive_logic mode)
{
    if(mode & DAMAGE_RECURSIVE_MODE_HP)
        return (mode & DAMAGE_RECURSIVE_MODE_NON_LETHAL)
            ? ((mode & DAMAGE_RECURSIVE_MODE_MP) ? 3 : 1)
            : ((mode & DAMAGE_RECURSIVE_MODE_MP) ? 5 : 4);
    return (mode & DAMAGE_RECURSIVE_MODE_MP) ? 2 : 0;
}

static HRESULT legacy_dot_access(entity *ent, ScriptVariant **args,
                                 ScriptVariant **result, int count, int write)
{
    LONG index, number;
    int property, first, last, n;
    int positional=write && count>=4 && args[3]->vt!=VT_STR;
    static const int order[]={4,1,0,3,5,2}; /* time, mode, force, rate, type, owner */
    s_damage_recursive *dot;
    if(count<4 || FAILED(ScriptVariant_IntegerValue(args[2],&index)) || index<0 || index>=10)
        goto error;
    property=positional ? -1 : legacy_dot_property(args[3]);
    if(!positional && (property<0 || (write && count<5))) goto error;
    for(dot=ent->recursive_damage;dot && dot->index!=index;dot=dot->next) {}
    if(!write)
    {
        ScriptVariant_ChangeType(*result,property==2 ? VT_PTR : VT_INTEGER);
        if(property==2) (*result)->ptrVal=dot ? dot->owner : NULL;
        else {
            number=0;
            if(dot) switch(property) {
            case 0: number=dot->force; break;
            case 1: number=legacy_dot_mode(dot->mode); break;
            case 3: number=dot->rate; break;
            case 4: number=dot->time; break;
            case 5: number=dot->type; break;
            }
            (*result)->lVal=number;
        }
        return S_OK;
    }
    first=positional ? 3 : 4;
    last=positional ? (count<9 ? count : 9) : 5;
    /* Validate before allocating or changing any field. Empty positional
     * arguments retain the corresponding existing value. */
    for(n=first;n<last;++n) {
        int field=positional ? order[n-3] : property;
        if(args[n]->vt==VT_EMPTY) continue;
        if(field==2) { if(args[n]->vt!=VT_PTR) goto error; }
        else if(FAILED(ScriptVariant_IntegerValue(args[n],&number)) ||
                (field==1 && (number<0 || number>5))) goto error;
    }
    if(!dot) {
        dot=recursive_damage_allocate_object();
        dot->index=index; dot->type=0; /* Original arrays were zero initialized. */
        dot->next=ent->recursive_damage; ent->recursive_damage=dot;
    }
    for(n=first;n<last;++n) {
        int field=positional ? order[n-3] : property;
        if(field==2) { dot->owner=args[n]->vt==VT_EMPTY ? NULL : args[n]->ptrVal; continue; }
        if(args[n]->vt==VT_EMPTY) continue;
        ScriptVariant_IntegerValue(args[n],&number);
        switch(field) {
        case 0: dot->force=number; break;
        case 1: dot->mode=recursive_damage_get_mode_setup_from_legacy_argument(number); break;
        case 3: dot->rate=number; break;
        case 4: dot->time=number; break; /* Absolute engine time, as in the old API. */
        case 5: dot->type=number; break;
        }
    }
    return S_OK;
error:
    *result=NULL;
    return E_FAIL;
}
