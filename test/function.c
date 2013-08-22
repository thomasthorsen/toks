
int functiona(int a, int b);

extern int functionb(int a, int b);

static int functionc(int a, int b);

static int functionc(int a, int b)
{
    return a + b;
}

int functiona(int a, int b)
{
    return a + b;
}

void functiond(void (*a)(void));

/* void (*functione(int a))(void);  BROKEN */

/* void (*functionf(void (*a)(void)))(void);  BROKEN */

int functiong(
    int a,
#if defined(DEFINE)
    int b,
#endif
    int c)
{
    int d = functiona(a, c);
#if defined(DEFINE)
    d += b;
#endif
    return d;
}

#if defined(DEFINE)
int functionh(int a, int b, int c)
#else
int functionh(int a, int c)
#endif
{
    int d = functiona(a, c);
#if defined(DEFINE)
    d += b;
#endif
    return d;
}
