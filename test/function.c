
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
