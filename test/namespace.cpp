
using namespace std; /* REF */

namespace first /* DEF */
{
    int var = 5;

    namespace second /* DEF */
    {
        double var = 3.1416;
    }
}

void function()
{
    int a = first::var;
    double b = second::var;
}
