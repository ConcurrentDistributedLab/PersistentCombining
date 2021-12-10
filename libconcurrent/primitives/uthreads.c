#include <uthreads.h>

#define FIBER_STACK 65536

typedef struct Fiber {
    ucontext_t context; /* Stores the current context */
    jmp_buf jmp;
    bool active;
} Fiber;

typedef struct FiberData {
    void *(*func)(void *);
    jmp_buf *cur;
    ucontext_t *prev;
    long arg;
} FiberData;

inline static void switch_to_fiber(Fiber *prev, Fiber *cur);
inline static void fiber_start_func(FiberData *context);

static volatile __thread int N_FIBERS = 1;
static __thread Fiber *FIBER_LIST = NULL;
static __thread Fiber *FIBER_RECYCLE = NULL;
static __thread int MAX_FIBERS = 1;
static __thread int currentFiber = 0;

void synchInitFibers(int max) {
    int i;

    MAX_FIBERS = max;
    FIBER_LIST = synchGetMemory(MAX_FIBERS * sizeof(Fiber));
    getcontext(&(FIBER_LIST[0].context));
    FIBER_LIST[0].active = true;
    FIBER_LIST[0].context.uc_link = NULL;
    for (i = 1; i < MAX_FIBERS; i++)
        FIBER_LIST[i].active = false;
}

inline static void switch_to_fiber(Fiber *prev, Fiber *cur) {
    if (_setjmp(prev->jmp) == 0) {
        _longjmp(cur->jmp, 1);
    }
}

void synchFiberYield(void) {
    int prev_fiber;

    if (N_FIBERS == 1)
        return;
    // Saved the state so call the next fiber
    prev_fiber = currentFiber;
    do {
        currentFiber = (currentFiber + 1) % MAX_FIBERS;
    } while (FIBER_LIST[currentFiber].active == false);
    switch_to_fiber(&FIBER_LIST[prev_fiber], &FIBER_LIST[currentFiber]);
    if (FIBER_RECYCLE != NULL) {
        synchFreeMemory(FIBER_RECYCLE, sizeof(Fiber));
        FIBER_RECYCLE = NULL;
    }
}

static void fiber_start_func(FiberData *context) {
    ucontext_t tmp;
    void *(*func)(void *);
    long arg;
    int prev_fiber;

    func = context->func;
    arg = context->arg;
    if (_setjmp(*(context->cur)) == 0)
        swapcontext(&tmp, context->prev);

    func((void *)arg); // Execute fiber with function func
    if (N_FIBERS == 1) // This check is not useful, since
        return;        // main thread never calls fiber_start_func
    FIBER_RECYCLE = FIBER_LIST[currentFiber].context.uc_stack.ss_sp;
    FIBER_LIST[currentFiber].active = false;
    N_FIBERS--;
    prev_fiber = currentFiber;
    do {
        currentFiber = (currentFiber + 1) % MAX_FIBERS;
    } while (FIBER_LIST[currentFiber].active == false);
    switch_to_fiber(&FIBER_LIST[prev_fiber], &FIBER_LIST[currentFiber]);
}

int synchSpawnFiber(void *(*func)(void *), long arg) {
    ucontext_t tmp;
    FiberData context;

    if (N_FIBERS == MAX_FIBERS)
        return -1;

    getcontext(&(FIBER_LIST[N_FIBERS].context));
    FIBER_LIST[N_FIBERS].active = true;
    FIBER_LIST[N_FIBERS].context.uc_link = NULL;
    FIBER_LIST[N_FIBERS].context.uc_stack.ss_sp = synchGetMemory(FIBER_STACK);
    FIBER_LIST[N_FIBERS].context.uc_stack.ss_size = FIBER_STACK;
    FIBER_LIST[N_FIBERS].context.uc_stack.ss_flags = 0;

    context.func = func;
    context.arg = arg;
    context.cur = &FIBER_LIST[N_FIBERS].jmp;
    context.prev = &tmp;
    makecontext(&FIBER_LIST[N_FIBERS].context, (void (*)())fiber_start_func, 1, &context);
    swapcontext(&tmp, &FIBER_LIST[N_FIBERS].context);
    N_FIBERS++;
    return 1;
}

void synchWaitForAllFibers(void) {  // Execute the fibers until they quit
    while (N_FIBERS > 1)
        synchFiberYield();
}
