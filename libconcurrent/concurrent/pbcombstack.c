#include <pbcombstack.h>

static const uint64_t NVMEM_CACHE_LINE_SIZE = 64;
static const uint64_t NEG_NVMEM_CACHE_LINE_SIZE = ~(64 - 1);

inline static RetVal serialPushPop(void *state, ArgVal arg, int pid);
inline static void clPersist_pushed_nodes(void *state);

static const int POP_OP = INT_MIN;
static __thread SynchPoolStruct *pool_node = NULL;
static __thread Node **free_list = NULL;
static __thread uint64_t free_list_size = 0;
static __thread Node **clNewItems = NULL;
static __thread uint64_t clNewItems_size = 0;
static __thread uint64_t *clNewItems_count;
static __thread uint64_t push_counter = 0, pop_counter = 0;


inline static void clPersist_pushed_nodes(void *state) {
    int i;

#ifdef SYNCH_DISABLE_ELIMINATION_ON_STACKS
    for (i = 0; i < clNewItems_size; i++) {
        synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);       
    }
#else
    for (i = 0; i < clNewItems_size; i++) {
        if (clNewItems_count[i] > 0)
            synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);
    }
#endif
}

inline static void after_persist_func(void *state) {
    int i;

    for (i = 0; i < free_list_size; i++) {
        synchRecycleObj(pool_node, free_list[i]);
    }
}

void PBCombStackInit(PBCombStackStruct *stack_object_struct, uint32_t nthreads) {
    PBCombStructInit(&stack_object_struct->object_struct, nthreads, (void *)&stack_object_struct->head, sizeof(Node *));
    stack_object_struct->head = NULL;
    PBCombSetFinalPersist(&stack_object_struct->object_struct, clPersist_pushed_nodes);
    PBCombSetAfterPersist(&stack_object_struct->object_struct, after_persist_func);
    synchStoreFence();
    synchInitPoolPersistent(&stack_object_struct->pool_node, sizeof(Node));   
}

void PBCombStackThreadStateInit(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, int pid) {
    int i;

    PBCombThreadStateInit(&object_struct->object_struct, &lobject_struct->th_state, (int)pid);
    clNewItems = synchGetAlignedMemory(CACHE_LINE_SIZE, object_struct->object_struct.nthreads * sizeof(Node *));
    clNewItems_count = synchGetAlignedMemory(CACHE_LINE_SIZE, object_struct->object_struct.nthreads * sizeof(uint64_t));
    free_list = synchGetAlignedMemory(CACHE_LINE_SIZE, object_struct->object_struct.nthreads * sizeof(Node *));
    for (i = 0; i < object_struct->object_struct.nthreads; i++) {
        clNewItems[i] = NULL;
        clNewItems_count[i] = 0;
        free_list[i] = NULL;
    }
    clNewItems_size = 0;
    free_list_size = 0;
    pool_node = &object_struct->pool_node; 
}

inline static RetVal serialPushPop(void *state, ArgVal arg, int pid) {
    uint64_t new_item_ptr;
    int i;

    if (arg == POP_OP) {
        volatile Node *head = *((Node **)state);
        volatile Node *node = head;

        if (head != NULL) {
            pop_counter++;

            new_item_ptr = ((uint64_t)head) & NEG_NVMEM_CACHE_LINE_SIZE;
            for (i = 0; i < clNewItems_size; i++) {
                if (new_item_ptr == (uint64_t)clNewItems[i]) {
                    clNewItems_count[i]--;
                    break;
                }
            }

            head = head->next;
            free_list[free_list_size] = (void *)node;
            free_list_size++;
            *((volatile Node **)state) = head;
            return node->val;
        } else return -1;
    } else {
        volatile Node *head = *((Node **)state);
        Node *node;
        bool found = false;

        push_counter++;
        node = synchAllocObj(pool_node);
        node->next = head;
        node->val = arg;

        new_item_ptr = ((uint64_t)node) & NEG_NVMEM_CACHE_LINE_SIZE;
        for (i = 0; i < clNewItems_size; i++) {
            if (new_item_ptr == (uint64_t)clNewItems[i]) {
                found = true;
                clNewItems_count[i]++;
                break;
            }
        }
        if (!found) {
            clNewItems[clNewItems_size] = (void *)(new_item_ptr);
            clNewItems_count[clNewItems_size] = 1;
            clNewItems_size++;
        }

        head = node;
        *((volatile Node **)state) = head;
 
        return 0;
    }
}

void PBCombStackPush(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, ArgVal arg, int pid) {
    int i;
    clNewItems_size = 0;
    for (i = 0; i < object_struct->object_struct.nthreads; i++) {
        clNewItems_count[i] = 0;
    }
    free_list_size = 0;
    push_counter = 0;
    pop_counter = 0;
    PBCombApplyOp(&object_struct->object_struct, &lobject_struct->th_state, serialPushPop, (ArgVal) arg, pid);
}

RetVal PBCombStackPop(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, int pid) {
    int i;
    clNewItems_size = 0;
    for (i = 0; i < object_struct->object_struct.nthreads; i++) {
        clNewItems_count[i] = 0;
    }
    free_list_size = 0;
    push_counter = 0;
    pop_counter = 0;
    return PBCombApplyOp(&object_struct->object_struct, &lobject_struct->th_state, serialPushPop, (ArgVal) POP_OP, pid);
}
