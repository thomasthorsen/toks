
typedef int typea;

typedef union uniona uniona;

typedef union unionb {int a; int b;} unionb;

typedef union {int a; int b;} unionc;

typedef struct structa structa;

typedef struct structb {int a; int b;} structb;

typedef struct {int a; int b;} structc;

typedef enum enuma enuma;

typedef enum enumb {ENUMB_A, ENUMB_B} enuma;

typedef enum {ENUMB_A, ENUMB_B} enumc;

typedef void (*typee)(void);

typedef void (typef)(void);

typedef void typeg(void);

void function(
    typea a,
    uniona uaa,
    union unionb ub,
    unionb ubb,
    struct structa sa,
    structa saa,
    enuma ea,
    enum enumb eaa,
    typee e,
    typef f,
    typeg g)
{
    typea a1;
    uniona uaa1;
    union unionb ub1;
    unionb ubb1;
    struct structa sa1;
    structa saa1;
    enuma ea1,
    enum enumb eaa1,
    typee e1;
    typef f1;
    typeg g1;
}
