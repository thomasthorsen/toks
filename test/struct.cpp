
struct strua; /* PROTO */

struct strua /* DEF */
{
    int a;
    int b;
    int functionc(int x)
    {
        return a + b + x;
    }
};

int functiona(void)
{
    strua a1; /* REF */

    a1.a = 5;
    a1.b = 10;

    return a1.functionc(7);
}
