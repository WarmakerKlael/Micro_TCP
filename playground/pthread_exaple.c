#include <stdio.h>
#include <pthread.h>
void *do_something(void *p)
{
        printf("Hello from %s thread %lu\n", (char *)p, pthread_self());
        return NULL;
}
int main()
{
        pthread_t tid0 = 30;
        printf("tid == %lu\n", tid0);
        char *msg1 = "parent", *msg2 = "child";
        pthread_create(&tid0, NULL, do_something, msg2);
        printf("tid == %lu\n", tid0);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        pthread_join(tid0, NULL);
        return 0;
}