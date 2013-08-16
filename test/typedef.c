
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
