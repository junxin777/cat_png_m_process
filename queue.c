#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sizeof_shm_queue(int size)
{
    return (sizeof(RBUF_QUEUE) + (sizeof(RECV_BUF) + sizeof(char) * 10240 * 2) * size);
}

int init_shm_queue(RBUF_QUEUE *p, int queue_max_size)
{
    if (p == NULL || queue_max_size == 0)
    {
        perror("init_shm_queue is null or queue_max_size is 0");
        return 1;
    }

    p->max_size = queue_max_size;
    p->size = 0;
    p->head = 0;
    p->tail = 0;
    p->buffers = (RECV_BUF *)((char *)p + sizeof(RBUF_QUEUE));
    return 0;
}

int is_full(RBUF_QUEUE *p)
{
    if (p == NULL)
    {
        return 0;
    }
    return ((p->tail + 1) % p->max_size == p->head && p->size == p->max_size);
}

int is_empty(RBUF_QUEUE *p)
{
    if (p == NULL)
    {
        return 0;
    }
    return (p->head == p->tail && p->size == 0);
}

int enqueue(RBUF_QUEUE *p, RECV_BUF item)
{
    p->size++;

    if (p->size > p->max_size)
    {
        perror("dequeue: size > 0");
    }

    // memcpy(p->buffers[p->tail].buf, item.buf, item.max_size);

    // p->buffers[p->tail].size = item.size;
    // p->buffers[p->tail].max_size = item.max_size;
    // p->buffers[p->tail].seq = item.seq;

    p->buffers[p->tail] = item;
    memcpy(p->buffers[p->tail].data, item.buf, item.size);

    // memcpy(p->buffers[p->tail].buf, item.buf, item.size);
    p->tail = (p->tail + 1) % p->max_size;

    return 0;
}

int dequeue(RBUF_QUEUE *p, RECV_BUF *p_item)
{
    p->size--;

    if (p->size < 0)
    {
        perror("dequeue: size < 0");
    }

    *p_item = p->buffers[p->head];
    p->head = (p->head + 1) % p->max_size;

    return 0;
}