#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

//需要接受的参数：s,E,b，trace文件
typedef struct cache_line{
    int valid;
    int tag;
    int LRU_Counter;//替换策略
}CacheLine;

typedef struct cache_
{
    int S;//set
    int E;//E个cache line
    int B;
    CacheLine ** line;//二维数组
}Cache;

//我们需要考虑的操作：读取，和存储
int hit_count = 0,miss_count = 0,eviction_count = 0;
Cache* cache = NULL;
int verbose = 0;
char t[1000];

void Init_Cache(int s,int E,int b){
    cache = (Cache*)malloc(sizeof(Cache));
    cache->S = 1<<s; //2^s
    cache->E = E;
    cache->B = 1<<b;
    cache->line = (CacheLine**)malloc(sizeof(CacheLine*)*(cache->S));
    for(int i = 0;i<cache->S;i++){
        cache->line[i] = (CacheLine*)malloc(sizeof(CacheLine)*(cache->E));
        for(int j = 0;j<cache->E;j++){
            cache->line[i][j].valid = 0;
            cache->line[i][j].tag = -1;
            cache->line[i][j].LRU_Counter = 0;
        }
    }
}

void Free_Cache(){
    int S = cache->S;
    for(int i = 0;i<S;i++){
        free(cache->line[i]);
    }
    free(cache->line);
    free(cache);
}

//cache搜索策略--cache hit/miss
//grace已经指明了是哪个set，我们现在确定是哪一行里面的
int get_line(int op_s,int op_tag){
    for(int i = 0;i<cache->E;i++){
        if(cache->line[op_s][i].valid == 1 && cache->line[op_s][i].tag == op_tag){
            return i;//cache hit
        }
    }
    return -1;//cache miss
    //这里cache miss需要分为两种情况，第一种是有空的，另一种是全满了,全满就需要替换
}

//在cache miss的情况下判断满没满，如果返回-1,那么就是满了，返回i（空的索引值）就是没满
int is_full(int op_s){
    for(int i = 0;i<cache->E;i++){
        if(cache->line[op_s][i].valid == 0) return i;
    }
    return -1;
}

//cache替换策略：LRU，首先每次填应该更新时间戳，如果cache miss就需要更新，每次应该找最大的进行更新
int find_LRU(int op_s){
    int max_index = -1;
    int max_counter = -1;
    for(int i =0;i<cache->E;i++){
        if(cache->line[op_s][i].LRU_Counter > max_counter){
            max_counter = cache->line[op_s][i].LRU_Counter;
            max_index = i;
        }
    }
    return max_index;
}

//将更新的思路分出去
void update(int op_s,int op_tag,int i){
    cache->line[op_s][i].tag = op_tag;
    cache->line[op_s][i].valid = 1;
    for(int k = 0;k<cache->E;k++){
        if(cache->line[op_s][k].valid == 1)
        cache->line[op_s][k].LRU_Counter++;
    }
    cache->line[op_s][i].LRU_Counter = 0;
}

//更新我们需要的三种状态,还有就是更新对应的cache line的行
void update_line(int op_s,int op_tag){
    int index = get_line(op_s,op_tag);
    if(index == -1){//cache miss
        miss_count++;
        //全满,需要进行替换
        int isFull = is_full(op_s);
        if(isFull == -1){
            eviction_count++;
            int max_index = find_LRU(op_s);
            update(op_s,op_tag,max_index);
        }else{
            //有空的
            update(op_s,op_tag,isFull);
        }
    }else{
        hit_count++;
        update(op_s,op_tag,index);
    }
}

//然后就是一些解析grace的思路了
void get_trace(int s,int E,int b){
    FILE * file;
    file = fopen(t,"r");
    if(file == NULL) exit(-1);
    char operation;
    unsigned address;
    int size;
     while (fscanf(file, " %c %x,%d", &operation, &address, &size) > 0) // I读不进来,忽略---size没啥用
    {
        //想办法先得到标记位和组序号
        int op_tag = address >> (s + b);
        int op_s = (address >> b) & ((unsigned)(-1) >> (8 * sizeof(unsigned) - s));
        switch (operation)
        {
        case 'M': //一次存储一次加载
            update_line(op_s,op_tag);
            update_line(op_s,op_tag);
            break;
        case 'L':
            update_line(op_s,op_tag);
            break;
        case 'S':
            update_line(op_s,op_tag);
            break;
        }
    }
    fclose(file);
}

int main(int argc, char *argv[])
{
    char opt;
    int s, E, b;
    /*
     * s:S=2^s是组的个数
     * E:每组中有多少行
     * b:B=2^b每个缓冲块的字节数
     */
    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:")))
    {
        switch (opt)
        {
        case 'h':
            exit(0);
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            exit(-1);
        }
    }
    Init_Cache(s, E, b); //初始化一个cache
    get_trace(s, E, b);
    Free_Cache();
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}