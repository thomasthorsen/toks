
extern int globvara; /* DECL */

int globvara; /* DEF */

int globvarb = 1234; /* DEF */

int globarraya[1234]; /* DEF */

int globarrayb[1234] = {0}; /* DEF */

int globavarc, *globptvara = NULL; /* DEF */

extern int globavarc, *globptvara; /* DECL */

extern void (*globalfptra)(void); /* DECL */

void (*globalfptrb)(void) = NULL; /* DEF */

void function(int locvara) /* DEF */
{
    int locvarb, locvarc = 0; /* DEF */

    for (int i = 0; /* DEF*/
         i < globvarb; /* REF */
         ++i) /* REF */
        ;
}
