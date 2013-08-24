#include <iostream>

class classa;

class classa
{
    public:
    classa();
    void functiona(int a);
    void functionb(std::ostream &b) const;

  private:
    static const int SIZE = 10;
    int *b;
    int c;
};

void function(void)
{
    classa a;
    classa *aa = new classa();
    delete aa;
}