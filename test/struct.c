
struct strua; /* PROTO */

struct strua /* DEF */
{
    int a;
    int b;
};

typedef struct strua strua; /* REF */

typedef struct
{
    int a;
    int b;
} strub;

typedef struct struc /* DEF */
{
    int a;
    struct struc *b; /* REF */
} struc;

struct strud /* DEF */
{
    int a;
    struct strue /* DEF */
    {
        int a;
    } e;
};

typedef struct struf /* DEF */
{
    int a;
    struct strug /* DEF */
    {
        int a;
    } e;
} struf;

void function(void)
{
    struct strua a1; /* REF */
    strua a2;
    strub b1;
    struct struc c1; /* REF */
    struc c2;
    struct strud d1; /* REF */
    struct strue e1; /* REF */
    struct struf f1; /* REF */
    struf f2;
    struct strug g1; /* REF */
    struct struh {int a; int b;} h1, *h2; /* DEF */
    struct struh h3; /* REF */
    struct {int a; int b;} i1, *i2;
}
