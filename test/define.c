
#define CONSTANT 1234

#define MULTI_LINE_CONSTANT \
    1234 + \
    1234

#define MACRO(x, y)  ((x) + (y))

int function(void)
{
    return MACRO(CONSTANT, 1234);
}
