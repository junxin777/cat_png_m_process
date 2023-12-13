#include <stddef.h>

typedef struct recv_inflated_IDAT
{
    char *buf;
    size_t size;
    size_t max_size;

} INFLATED_IDAT;

typedef struct recv_buf_flat
{
    // char server[256];
    size_t size;
    size_t max_size;
    int seq;
    char *buf;
    char data[10240];
    unsigned char uncompressed_data[10240];
    int uncompressed_data_size;

} RECV_BUF;

typedef struct rbuf_queue
{
    int max_size; /* the max capacity of the queue */
    int size;
    int head;          /* position of the first item */
    int tail;          /* position of the last item */
    RECV_BUF *buffers; /* queue of stored RECV_BUF structs */

} RBUF_QUEUE;

int sizeof_shm_queue(int size);

int init_shm_queue(RBUF_QUEUE *p, int queue_size);

RBUF_QUEUE *create_queue(int size);

void destroy_queue(RBUF_QUEUE *p);

int is_full(RBUF_QUEUE *p);

int is_empty(RBUF_QUEUE *p);

int enqueue(RBUF_QUEUE *p, RECV_BUF item);

int dequeue(RBUF_QUEUE *p, RECV_BUF *p_item);
