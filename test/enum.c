
enum enua; /* PROTO */

enum enua /* DEF */
{
    ENUA_A,
    ENUA_B,
};

typedef enum enua enua; /* REF */

typedef enum
{
    ENUB_A,
    ENUB_B,
} enub;

typedef enum enuc /* DEF */
{
    ENUC_A,
    ENUC_B,
} enuc;

void function(void)
{
    enum enua a1; /* REF */
    enua a2;
    enub b1;
    enum enuc c1; /* REF */
    enuc c2;

    enum enuh {ENUH_A, ENUH_B} h1, *h2; /* DEF */
    enum enuh h3; /* REF */
    enum {ENUI_A, ENUI_B} i1, *i2;
}

typedef enum enud
{
    ENUD_A,
#if defined(DEFINE)
    ENUD_B1,
#else
    ENUD_B2,
#endif
    ENUD_C,
} enud;
