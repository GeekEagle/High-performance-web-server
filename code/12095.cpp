#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

double time_val(struct timeval t1, struct timeval t2) {
    return (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
}
char* ls[(int)3e5];
int main() {
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    for (int i = 0; i < 300000; ++i)ls[i] = (char*)malloc(sizeof(char) * (i % 1000));
    gettimeofday(&t2, NULL);
    printf("Malloc:\t%.3fms\n", time_val(t1, t2));
    gettimeofday(&t1, NULL);
    for (int i = 0; i < 300000; ++i)free(ls[i]);
    gettimeofday(&t2, NULL);
    printf("Free:\t%.3fms\n", time_val(t1, t2));
    return 0;
}