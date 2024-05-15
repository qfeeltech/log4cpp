//
// Created by zp on 19-6-20.
//
#include <unistd.h>
#include <iostream>
using namespace std;
int main(int argc, char *argv[]) {
    if (argc != 3)
    {
        printf("input rate(hz) and logupload Interval(s)\n");
        return  -1;
    }
    printf("[logAgent][logWebSend]");
    int rate = atoi(argv[1]);
    int time = atoi(argv[2]);
    int i = 0;
    while(true)
    {
        i++;
        printf("%d\n",i);
        if( i % (rate * time)== 0)
        {
            printf("[logAgent][uploadLog]");
        }
        usleep(1000*1000/rate);
    }
    return 0;
}