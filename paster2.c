#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include "./starter/png_util/lab_png.h"
#include "./starter/png_util/zutil.h"

#define IMG_URL_1 "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define IMG_URL_2 "http://ece252-1.uwaterloo.ca:2520/image?img=2"
#define IMG_URL_3 "http://ece252-1.uwaterloo.ca:2520/image?img=3"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240 /* 1024*10 = 10K */
#define INFLATED_IDAT_SIZE 10240 * 50
#define SEM_PROC 1

double times[2];
struct timeval tv; // time value struct

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);

void time_start()
{
    if (gettimeofday(&tv, NULL) != 0)
    { // get the current time
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec / 1000000.; // seconds + microseconds
}

void time_end()
{
    if (gettimeofday(&tv, NULL) != 0)
    { // get the current time
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec / 1000000.; // seconds + microseconds
}

int catpng(struct recv_buf_flat *files_data)
{
    const int png_num = 50;

    int total_height = 0, buf_height = 0, buf_width = 0;
    unsigned long buf_uncompressed_data_size_total = 0;

    // 1. find total height
    // 2. find uncompressed total data buf size for IDAT
    for (int i = 0; i < png_num; i++)
    {
        struct data_IHDR *data_IHDR_create = calloc(1, sizeof(struct data_IHDR));
        get_png_data_IHDR(data_IHDR_create, files_data[i]);
        
        buf_width = get_png_width(data_IHDR_create);
        buf_height = get_png_height(data_IHDR_create);
        total_height += buf_height;

        free(data_IHDR_create);
        data_IHDR_create = NULL;
    }
    int location = 0; // This is a coordination of writting umcompressed data

    // create buf stores UNZIPPED TOTAL data for IDAT
    buf_uncompressed_data_size_total = total_height * (buf_width * 4 + 1);            // !!!!!!!!
    U8 *buf_uncompressed_data = calloc(buf_uncompressed_data_size_total, sizeof(U8)); // !!!!!!!!

    // create buf stores zipped TOTAL data size for IDAT
    U64 buf_compressed_data_size_total = 0;

    for (int i = 0; i < png_num; i++)
    {
        struct chunk *chunk_IDAT = calloc(1, sizeof(struct chunk));

        get_chunk_IDAT(chunk_IDAT, files_data[i]);
        buf_compressed_data_size_total += chunk_IDAT->length;

        free(chunk_IDAT->p_data);
        chunk_IDAT->p_data = NULL;
        free(chunk_IDAT);
        chunk_IDAT = NULL;
    }

    for (size_t i = 0; i < 50; i++)
    {
        for (int j = 0; j < files_data[i].uncompressed_data_size; j++)
        {
            buf_uncompressed_data[location] = files_data[i].uncompressed_data[j];
            location++;
        }
    }

    // so far what we've done
    // 1. total height calculated
    // 2. total unzipped data from IDAT stored in buf_uncompressed_data
    // 3. total zipped data size from IDAT stored in buf_compressed_data_size_total

    // create buf stores ZIPPED TOTAL data for IDAT
    U8 *buf_compressed_data = (U8 *)calloc(buf_compressed_data_size_total, sizeof(U8));

    // zipp the unzipped data which calculated in the second loop
    mem_def(buf_compressed_data, &buf_compressed_data_size_total, buf_uncompressed_data, buf_uncompressed_data_size_total, Z_DEFAULT_COMPRESSION);

    // create new header
    U32 header1 = 0x89504E47;
    U32 header2 = 0x0D0A1A0A;
    FILE *all = fopen("all.png", "wb"); // final png file
    // write new header to all.png
    header1 = htonl(header1);
    header2 = htonl(header2);
    fwrite(&header1, 4, 1, all);
    fwrite(&header2, 4, 1, all);

    // create new IHDR chunk from same png file ------------------------------------------------
    struct chunk *chunk_IHDR = NULL;
    chunk_IHDR = calloc(1, sizeof(struct chunk));
    chunk_IHDR->p_data = NULL;
    get_chunk_IHDR(chunk_IHDR, files_data[0]); // get IHDR info for all.png

    // data for IHDR
    U8 bit_depth = 0x08;
    U8 color = 0x06;
    U8 compress = 0x00;
    U8 filter = 0x00;
    U8 interlace = 0x00;
    buf_width = htonl(buf_width);
    total_height = htonl(total_height);
    memcpy(chunk_IHDR->p_data, &buf_width, 4);
    memcpy(chunk_IHDR->p_data + 4, &total_height, 4);
    memcpy(chunk_IHDR->p_data + 8, &bit_depth, 1);
    memcpy(chunk_IHDR->p_data + 9, &color, 1);
    memcpy(chunk_IHDR->p_data + 10, &compress, 1);
    memcpy(chunk_IHDR->p_data + 11, &filter, 1);
    memcpy(chunk_IHDR->p_data + 12, &interlace, 1);

    // calculate crc for new IHDR
    U32 new_crc_IHDR = get_crc(chunk_IHDR);

    // write IHDR length to all.png
    chunk_IHDR->length = htonl(chunk_IHDR->length);
    fwrite(&chunk_IHDR->length, 4, 1, all);

    // write IHDR type to all.png
    fwrite(&chunk_IHDR->type, 4, 1, all);

    // write IHDR data to all.png

    fwrite(chunk_IHDR->p_data, 13, 1, all);

    // write IHDR crc to all.png
    new_crc_IHDR = htonl(new_crc_IHDR);
    memcpy(&chunk_IHDR->crc, &new_crc_IHDR, 4);
    fwrite(&chunk_IHDR->crc, 4, 1, all);

    // printf("good4\n");

    // create new IDAT chunk from same png file ------------------------------------------------
    chunk_p chunk_IDAT = calloc(1, sizeof(struct chunk));
    get_chunk_IDAT(chunk_IDAT, files_data[0]);

    // update legth for IDAT
    memcpy(&chunk_IDAT->length, &buf_compressed_data_size_total, 4);

    // update data for IDAT
    // we have to free p_data as when we generated a new chunk var, it initialized a p_data pointer
    // with randomly pointed data, we need to overwrite that data
    free(chunk_IDAT->p_data);
    chunk_IDAT->p_data = NULL;
    chunk_IDAT->p_data = buf_compressed_data;

    // calculate crc for new IDAT
    U32 new_crc_IDAT = get_crc(chunk_IDAT);

    // write IDAT length in all.png
    chunk_IDAT->length = htonl(chunk_IDAT->length);
    fwrite(&chunk_IDAT->length, 4, 1, all);

    // write IDAT type in all.png
    fwrite(&chunk_IDAT->type, 4, 1, all);

    // write IDAT data in all.png
    for (int i = 0; i < buf_compressed_data_size_total; i++)
    {
        fwrite(&chunk_IDAT->p_data[i], 1, 1, all);
    }

    // write IDAT crc in all.png
    new_crc_IDAT = htonl(new_crc_IDAT);
    memcpy(&chunk_IDAT->crc, &new_crc_IDAT, 4);
    fwrite(&chunk_IDAT->crc, 4, 1, all);

    // create new header
    U32 iend_length = 0x00000000;
    U32 iend_type = 0x49454E44;
    U32 iend_crc = 0xAE426082;
    // write new header to all.png
    iend_length = htonl(iend_length);
    iend_type = htonl(iend_type);
    iend_crc = htonl(iend_crc);
    fwrite(&iend_length, 4, 1, all);
    fwrite(&iend_type, 4, 1, all);
    fwrite(&iend_crc, 4, 1, all);

    free(chunk_IHDR->p_data);
    chunk_IHDR->p_data = NULL;
    free(chunk_IDAT->p_data);
    // free(chunk_IEND->p_data);
    free(chunk_IHDR);
    chunk_IHDR = NULL;
    free(chunk_IDAT);
    // free(chunk_IEND);
    free(buf_uncompressed_data);

    fclose(all);

    return 0;
}

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
        strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {

        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size)
    { /* hope this rarely happens */
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0; // make sure it is null terminated just in case

    return realsize;
}

int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes * 2);
}

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if (ptr == NULL)
    {
        printf("ptr is null\n");
        return 1;
    }

    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1; /* valid seq should be non-negative */
    ptr->uncompressed_data_size = 0;
    memset(ptr->data, 0, nbytes);
    memset(ptr->uncompressed_data, 0, nbytes);

    return 0;
}

int sizeof_shm_recv_inflated_IDAT(size_t nbytes)
{
    return (sizeof(INFLATED_IDAT) + sizeof(char) * nbytes);
}

int shm_recv_inflated_IDAT_init(INFLATED_IDAT *ptr, size_t nbytes)
{
    if (ptr == NULL)
    {
        return 1;
    }

    ptr->buf = (char *)ptr + sizeof(INFLATED_IDAT);
    ptr->size = 0;
    ptr->max_size = nbytes;

    return 0;
}

void is_fifty(int *fifty, int *result)
{
    result[0] = 0;
    result[1] = 0;

    for (size_t i = 0; i < 50; i++)
    {
        if (fifty[i] == -1)
        {
            result[0] = 1;
            result[1] = 1;
        }

        if (fifty[i] != -2)
        {
            result[1] = 1;
        }
    }
    return;
}

int consumer_is_fifty(int *fifty)
{
    for (size_t i = 0; i < 50; i++)
    {
        if (fifty[i] != -2)
        {
            return 1; // need to keep going
        }
    }

    return 0; // clear all consumers
}

int main(int argc, char **argv)
{
    // *************** check for correct number of arguments
    if (argc != 6)
    { // Including the program name, there should be 6 arguments in total
        printf("Usage: %s <B> <P> <C> <X> <N>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int B = atoi(argv[1]); // Buffer size
    int P = atoi(argv[2]); // Number of producers
    int C = atoi(argv[3]); // Number of consumers
    int X = atoi(argv[4]); // Sleep time for consumers
    int N = atoi(argv[5]); // Image number

    int num_p = P; // number of producers
    int num_c = C; // number of consumers

    int sequence_size[P];

    for (size_t i = 0; i < P; i++)
    {
        sequence_size[i] = 0;
    }
    for (size_t i = 0; i < 50; i++)
    {
        int index = i % P;
        sequence_size[index] += 1;
    }

    int *sequence[P];

    for (size_t i = 0; i < P; i++)
    {
        sequence[i] = malloc(sizeof(int) * sequence_size[i]);
        sequence[i][0] = i;
    }
    for (size_t i = 0; i < P; i++)
    {
        for (size_t j = 1; j < sequence_size[i]; j++)
        {
            sequence[i][j] = sequence[i][j - 1] + P;
        }
    }

    pid_t pid = 0;
    pid_t cpids[num_p + num_c]; // array to hold pid's
    // *************** create shared memory for shm_queue ***************
    int shm_size_queue = sizeof_shm_queue(B);

    int shmid_queue = shmget(IPC_PRIVATE, shm_size_queue, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_queue < 0)
    {
        perror("shmid_queue shmget");
        exit(EXIT_FAILURE);
    }

    struct rbuf_queue *queue;
    queue = (struct rbuf_queue *)shmat(shmid_queue, NULL, 0);
    if (queue == (void *)-1)
    {
        perror("queue shmat");
        abort();
    }

    init_shm_queue(queue, B);
    // *************** create shared memory for total_recv_buf ***************
    int shm_total_buffer_size = 50 * sizeof_shm_recv_buf(sizeof(RECV_BUF));

    int shmid_total_buffer = shmget(IPC_PRIVATE, shm_total_buffer_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_total_buffer < 0)
    {
        perror("shmid_total_buffer shmget");
        exit(EXIT_FAILURE);
    }

    struct recv_buf_flat *ordered_files_data = NULL;
    ordered_files_data = (struct recv_buf_flat *)shmat(shmid_total_buffer, NULL, 0);
    if (ordered_files_data == (void *)-1)
    {
        perror("ordered_files_data shmat");
        abort();
    }
    // *************** create shared memory for fifty check ***************
    int shmid_fifty_check = shmget(IPC_PRIVATE, sizeof(int) * 50, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_fifty_check < 0)
    {
        perror("fifty_check shmget");
        exit(EXIT_FAILURE);
    }

    int *fifty_check = NULL;
    fifty_check = (int *)shmat(shmid_fifty_check, NULL, 0);
    if (fifty_check == (void *)-1)
    {
        perror("fifty_check shmat");
        abort();
    }

    for (size_t i = 0; i < 50; i++)
    {
        fifty_check[i] = -1;
    }
    // *************** create four different semaphores ***************
    sem_t *sem_room_items = NULL;    // shared memory region for semaphores
    sem_t *sem_room_inuse = NULL;    // shared memory region for semaphores
    sem_t *sem_room_spaces = NULL;   // shared memory region for semaphores
    sem_t *sem_is_room_fifty = NULL; // shared memory region for semaphores
    int shmid_sem_room_items = shmget(IPC_PRIVATE, sizeof(sem_t) * 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_sem_room_spaces = shmget(IPC_PRIVATE, sizeof(sem_t) * 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_room_inuse = shmget(IPC_PRIVATE, sizeof(sem_t) * 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_sem_is_room_fifty = shmget(IPC_PRIVATE, sizeof(sem_t) * 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    // attach to shared memory regions
    sem_room_items = shmat(shmid_sem_room_items, NULL, 0);
    sem_room_spaces = shmat(shmid_sem_room_spaces, NULL, 0);
    sem_room_inuse = shmat(shmid_room_inuse, NULL, 0);
    sem_is_room_fifty = shmat(shmid_sem_is_room_fifty, NULL, 0);

    // initialize shared memory varaibles
    if (sem_init(sem_room_items, SEM_PROC, 0) != 0)
    { // shmid_sem_room_items can be accessed by different processes, initial value is 1
        perror("sem_init shmid_sem_room_items");
        abort();
    }
    if (sem_init(sem_room_spaces, SEM_PROC, B) != 0)
    { // shmid_sem_room_spaces can be accessed by different processes, initial value is 0
        perror("sem_init shmid_sem_room_spaces");
        abort();
    }
    if (sem_init(sem_room_inuse, SEM_PROC, 1) != 0)
    { // shmid_room_inuse can be accessed by different processes, initial value is 1
        perror("sem_init shmid_room_inuse");
        abort();
    }
    if (sem_init(sem_is_room_fifty, SEM_PROC, 1) != 0)
    { // shmid_sem_is_room_fifty can be accessed by different processes, initial value is 1
        perror("sem_init shmid_sem_is_room_fifty");
        abort();
    }
    // *************** fork producers and consumers ***************
    time_start();
    // create producers
    for (int i = 0; i < num_p; i++)
    {
        pid = fork();

        if (pid > 0)
        {
            cpids[i] = pid;
        }
        else if (pid == 0) // child process
        {
            for (size_t j = 0; j < sequence_size[i]; j++)
            {
                int server = P % 3 + 1;

                char url[55];
                snprintf(url, 55, "http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d", server, N, sequence[i][j]);
                CURL *curl_handle;
                CURLcode res;
                RECV_BUF *temp_recv_buf = NULL;
                temp_recv_buf = calloc(1, sizeof_shm_recv_buf(BUF_SIZE));

                shm_recv_buf_init(temp_recv_buf, BUF_SIZE);
                curl_global_init(CURL_GLOBAL_DEFAULT);
                curl_handle = curl_easy_init();
                if (curl_handle == NULL)
                {
                    fprintf(stderr, "curl_easy_init: returned NULL\n");
                    return 1;
                }

                curl_easy_setopt(curl_handle, CURLOPT_URL, url);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)temp_recv_buf);
                curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
                curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)temp_recv_buf);
                curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

                res = curl_easy_perform(curl_handle);
                if (res != CURLE_OK)
                {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                }

                if (fifty_check[temp_recv_buf->seq] == -1)
                {
                    if (sem_wait(sem_room_spaces) != 0)
                    {
                        perror("sem_wait sem_room_items");
                        abort();
                    }
                    if (sem_wait(sem_room_inuse) != 0)
                    {
                        perror("sem_wait sem_room_items");
                        abort();
                    }

                    fifty_check[temp_recv_buf->seq] = temp_recv_buf->seq;

                    enqueue(queue, *temp_recv_buf);

                    if (sem_post(sem_room_inuse) != 0)
                    {
                        perror("sem_wait sem_room_inuse");
                        abort();
                    }
                    if (sem_post(sem_room_items) != 0)
                    {
                        perror("sem_wait sem_room_items");
                        abort();
                    }
                }

                free(temp_recv_buf);
                temp_recv_buf = NULL;
                curl_easy_cleanup(curl_handle);
                curl_global_cleanup();
            }
            shmdt(queue);
            shmdt(fifty_check);
            shmdt(sem_room_items);
            shmdt(sem_room_spaces);
            shmdt(sem_room_inuse);
            shmdt(sem_is_room_fifty);

            exit(0);
        }
        else
        {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
    }

    // create consumers
    for (int i = num_p; i < num_p + num_c; i++)
    {
        pid = fork();

        if (pid > 0)
        {
            cpids[i] = pid;
        }
        else if (pid == 0) // child process
        {
            // *************** attach to shared memory for total_recv_buf ***************
            while (1)
            {
                if (sem_wait(sem_is_room_fifty) != 0)
                {
                    perror("sem_wait sem_is_room_fifty");
                    abort();
                }
                int result[2] = {0, 0};
                is_fifty(fifty_check, result);
                // printf("consumers: %d:result[0] is %d\n", getpid(), result[1]);
                if (result[1] == 0)
                {

                    if (sem_post(sem_is_room_fifty) != 0)
                    {
                        perror("sem_wait sem_is_room_fifty");
                        abort();
                    }
                    if (sem_post(sem_room_items) != 0)
                    {
                        perror("sem_wait sem_room_items");
                        abort();
                    }

                    break;
                }
                if (sem_post(sem_is_room_fifty) != 0)
                {
                    perror("sem_wait sem_is_room_fifty");
                    abort();
                }

                if (sem_wait(sem_room_items) != 0)
                {
                    perror("sem_wait sem_room_items");
                    abort();
                }
                if (sem_wait(sem_room_inuse) != 0)
                {
                    perror("sem_wait sem_room_inuse");
                    abort();
                }

                int fifty_flag = 0;
                fifty_flag = consumer_is_fifty(fifty_check);

                if (fifty_flag == 1)
                {
                    RECV_BUF temp_recv_buf;
                    dequeue(queue, &temp_recv_buf);
                    fifty_check[temp_recv_buf.seq] = -2;

                    memcpy(ordered_files_data[temp_recv_buf.seq].data, temp_recv_buf.data, temp_recv_buf.size);
                    ordered_files_data[temp_recv_buf.seq].buf = &ordered_files_data[temp_recv_buf.seq].data[0];
                    ordered_files_data[temp_recv_buf.seq].seq = temp_recv_buf.seq;
                    ordered_files_data[temp_recv_buf.seq].size = temp_recv_buf.size;
                    ordered_files_data[temp_recv_buf.seq].max_size = temp_recv_buf.max_size;

                    // *************** uncompressed data ***************

                    int buf_width = 400;
                    unsigned long buf_uncompressed_data_size_each = 0;
                    buf_uncompressed_data_size_each = 6 * (buf_width * 4 + 1);
                    
                    U8 *data_buf_uncompressed = (U8 *)calloc(buf_uncompressed_data_size_each, sizeof(U8));

                    struct chunk *chunk_IDAT = calloc(1, sizeof(struct chunk));
                    get_chunk_IDAT(chunk_IDAT, ordered_files_data[temp_recv_buf.seq]);

                    mem_inf(data_buf_uncompressed, &buf_uncompressed_data_size_each, chunk_IDAT->p_data, chunk_IDAT->length);

                    ordered_files_data[temp_recv_buf.seq].uncompressed_data_size = buf_uncompressed_data_size_each;

                    for (int j = 0; j < buf_uncompressed_data_size_each; j++)
                    {
                        ordered_files_data[temp_recv_buf.seq].uncompressed_data[j] = data_buf_uncompressed[j];
                    }

                    free(chunk_IDAT->p_data);
                    chunk_IDAT->p_data = NULL;
                    free(chunk_IDAT);
                    chunk_IDAT = NULL;
                    free(data_buf_uncompressed);
                    data_buf_uncompressed = NULL;
                }

                if (sem_post(sem_room_inuse) != 0)
                {
                    perror("sem_wait sem_room_inuse");
                    abort();
                }
                if (sem_post(sem_room_spaces) != 0)
                {
                    perror("sem_wait sem_room_spaces");
                    abort();
                }

                usleep(1000 * X);
            }
            shmdt(queue);
            shmdt(ordered_files_data);

            shmdt(sem_room_items);
            shmdt(sem_room_spaces);
            shmdt(sem_room_inuse);
            shmdt(sem_is_room_fifty);

            exit(0);
        }
        else
        {
            // error happened
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
    }

    // parent process should wait for all child processes to finish
    if (pid > 0)
    {
        for (int i = 0; i < num_p + num_c; i++)
        {
            waitpid(cpids[i], NULL, 0);
        }
    }

    shmdt(queue);
    shmctl(shmid_queue, IPC_RMID, NULL);

    catpng(ordered_files_data);

    time_end();
    printf("paster2 execution time:  %.6lf seconds\n", times[1] - times[0]);

    shmdt(ordered_files_data);
    shmctl(shmid_total_buffer, IPC_RMID, NULL);
    shmdt(fifty_check);
    shmctl(shmid_fifty_check, IPC_RMID, NULL);

    sem_destroy(sem_room_items);
    sem_destroy(sem_room_spaces);
    sem_destroy(sem_room_inuse);
    sem_destroy(sem_is_room_fifty);

    shmdt(sem_room_items);
    shmdt(sem_room_spaces);
    shmdt(sem_room_inuse);
    shmdt(sem_is_room_fifty);

    shmctl(shmid_sem_room_items, IPC_RMID, NULL);
    shmctl(shmid_sem_room_spaces, IPC_RMID, NULL);
    shmctl(shmid_room_inuse, IPC_RMID, NULL);
    shmctl(shmid_sem_is_room_fifty, IPC_RMID, NULL);

    for (size_t i = 0; i < P; i++)
    {
        free(sequence[i]);
    }
    
    return (0);
}
