
union unioa; /* PROTO */

union unioa /* DEF */
{
    int a;
    int b;
};

typedef union unioa unioa; /* REF */

typedef union
{
    int a;
    int b;
} uniob;

typedef union unioc /* DEF */
{
    int a;
    union unioc *b; /* REF */
} unioc;

union uniod /* DEF */
{
    int a;
    union unioe /* DEF */
    {
        int a;
    } e;
};

typedef union uniof /* DEF */
{
    int a;
    union uniog /* DEF */
    {
        int a;
    } e;
} uniof;

void function(void)
{
    union unioa a1; /* REF */
    unioa a2;
    uniob b1;
    union unioc c1; /* REF */
    unioc c2;
    union uniod d1; /* REF */
    union unioe e1; /* REF */
    union uniof f1; /* REF */
    uniof f2;
    union uniog g1; /* REF */
    union unioh {int a; int b;} h1, *h2; /* DEF */
    union unioh h3; /* REF */
    union {int a; int b;} i1, *i2;
}
