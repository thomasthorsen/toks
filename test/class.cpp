#include <iostream>

class classa;

class classa
{
public:
    classa();
    ~classa();
    void functiona(int a);
    void functionb(std::ostream &b) const;

private:
    static const int SIZE = 10;
    int *b;
    int c;
};

class classb
{
public:
    classb(): b(5)
    {
        c = b;
    }
    ~classb()
    {
        c = 5;
        b = 3;
    }
private:
    int b;
    int c;
};

classa::classa(): b(NULL), c(0)
{
    int y;
}

classa::~classa()
{
    int x;
}

void classa::functiona(int a)
{
}

void classa::functionb(std::ostream &b) const
{
}

void function(void)
{
    classa a;
    classa *aa = new classa();
    delete aa;
}
