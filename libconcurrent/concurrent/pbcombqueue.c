#include <pbcombqueue.h>

static const uint64_t NVMEM_CACHE_LINE_SIZE = 64;
static const uint64_t NEG_NVMEM_CACHE_LINE_SIZE = ~(64 - 1);

inline static RetVal serialEnqueue(void *state, ArgVal arg, int pid);
inline static RetVal serialDequeue(void *state, ArgVal arg, int pid);
inline static void clPersist_enqueued_nodes(void *state);

static const int GUARD = INT_MIN;
static __thread SynchPoolStruct pool_node CACHE_ALIGN;
static __thread Node **clNewItems;
static __thread uint64_t clNewItems_size = 0;
static __thread Node *Tail = NULL;
static __thread PBCombStruct *enqueue_struct;
static __thread uint64_t enqueue_counter = 0;

inline static void clPersist_enqueued_nodes(void *state) {
    int i;

    for (i = 0; i < clNewItems_size; i++) {
        synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);
    }
}

inline static void updateAuxField(void *state) {
    if (enqueue_counter > 0) {
        ((PBCombStruct *)state)->aux = Tail;
        synchFullFence();
    }
}


void PBCombQueueInit(PBCombQueueStruct *queue_object_struct, uint32_t nthreads) {
    queue_object_struct->guard.val = GUARD;
    queue_object_struct->guard.next = NULL;
    queue_object_struct->first = &queue_object_struct->guard;
    queue_object_struct->last = &queue_object_struct->guard;

    PBCombStructInit(&queue_object_struct->enqueue_struct, nthreads, (void *)&queue_object_struct->last, sizeof(Node *));
    queue_object_struct->enqueue_struct.aux = &queue_object_struct->guard;
    PBCombSetFinalPersist(&queue_object_struct->enqueue_struct, clPersist_enqueued_nodes);
    PBCombSetAfterPersist(&queue_object_struct->enqueue_struct, updateAuxField);

    PBCombStructInit(&queue_object_struct->dequeue_struct, nthreads, (void *)&queue_object_struct->first, sizeof(Node *));
    synchFullFence();
}

void PBCombQueueThreadStateInit(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, int pid) {
    int i;

    PBCombThreadStateInit(&object_struct->enqueue_struct, &lobject_struct->enqueue_thread_state, (int)pid);
    PBCombThreadStateInit(&object_struct->dequeue_struct, &lobject_struct->dequeue_thread_state, (int)pid);
    synchInitPoolPersistent(&pool_node, sizeof(Node));
    clNewItems = synchGetAlignedMemory(CACHE_LINE_SIZE, object_struct->enqueue_struct.nthreads * sizeof(Node **));
    for (i = 0; i < object_struct->enqueue_struct.nthreads; i++) {
        clNewItems[i] = NULL;
    }
    clNewItems_size = 0;
    Tail = NULL;
    enqueue_struct = &object_struct->enqueue_struct;
}

inline static RetVal serialEnqueue(void *state, ArgVal arg, int pid) {
    uint64_t new_item_ptr;
    volatile Node *last = *((Node **)state);
    volatile Node *node = synchAllocObj(&pool_node);
    bool found = false;
    int i;

    enqueue_counter++;
    node->next = NULL;
    node->val = arg;
    Tail = (Node *)node;
    last->next = node;
    new_item_ptr = ((uint64_t)node) & NEG_NVMEM_CACHE_LINE_SIZE;
    for (i = 0; i < clNewItems_size; i++) {
        if (new_item_ptr == (uint64_t)clNewItems[i]) {
            found = true;
            break;
        }
    }
    if (!found) {
        clNewItems[clNewItems_size] = (void *)(new_item_ptr);
        clNewItems_size++;
    }

    *((volatile Node **)state) = node;
    return -1;
}

inline static RetVal serialDequeue(void *state, ArgVal arg, int pid) {
    volatile Node *first = *((Node **)state);
    volatile Node *node;
    RetVal ret = -1;

    if (first != enqueue_struct->aux) {
        node = first;
        first = first->next;
        *((volatile Node **)state) = first;
        ret = first->val;
        synchRecycleObj(&pool_node, (void *)node);
        return ret;
    } else {
        return ret;
    }
}

void PBCombQueueApplyEnqueue(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, ArgVal arg, int pid) {
    clNewItems_size = 0;
    enqueue_counter = 0;
    PBCombApplyOp(&object_struct->enqueue_struct, &lobject_struct->enqueue_thread_state, serialEnqueue, (ArgVal) arg, pid);
}

RetVal PBCombQueueApplyDequeue(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, int pid) {
    clNewItems_size = 0;
    return PBCombApplyOp(&object_struct->dequeue_struct, &lobject_struct->dequeue_thread_state, serialDequeue, (ArgVal) pid, pid);
}
