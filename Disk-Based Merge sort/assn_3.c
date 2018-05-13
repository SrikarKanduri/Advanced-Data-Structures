#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#define RUN 0
#define SUPER_RUN 1
#define SELECT 2
#define REPLACE 3
#define B_START 750
#define B_LIMIT 250
#define BUFF_LIMIT 1000

const char* filename;
int initialsize;
int buff[ BUFF_LIMIT ], output_buff[ BUFF_LIMIT ];
int runptr[ BUFF_LIMIT ], finish[ BUFF_LIMIT ], endptr[ BUFF_LIMIT ];
FILE *fpr[ BUFF_LIMIT ];
FILE *fp, *out, *sub_out;

/* Qsort comparator for ascending sort */
int comparator (const void * a, const void * b) {
    return ( *(int*)a - *(int*)b );
}

/* Run number to string file name formatting */
char* getRun(int runs, int mode) {
    int i;
    char *rname;
    char rnum[4] = {'0', '0', '0', '\0'};
    
    for( i = 2; i >= 0; i--, runs /= 10)
        rnum[i] = (runs % 10) + '0';
    
    if(mode == RUN) {
        rname = malloc(strlen(filename) + 4);
        strcpy(rname, filename);
        strcat(rname, ".");
    } else {
        rname = malloc(strlen(filename) + 10);
        strcpy(rname, filename);
        strcat(rname, ".super.");
    }
    strcat(rname, rnum);
    
    return rname;
}

/* Creating presorted runs of size 1000 */
int preSort() {
    int runs = 0, maxsize;
    while( (maxsize = (int) fread(buff, sizeof(int), BUFF_LIMIT, fp)) ) {
        FILE *rp = fopen(getRun(runs, RUN), "wb");
        maxsize = maxsize < BUFF_LIMIT ? maxsize : BUFF_LIMIT;
        initialsize = maxsize;
        qsort(buff, maxsize, sizeof(int), comparator);
        fwrite(buff, sizeof(int), maxsize, rp);
        fclose(rp);
        runs++;
    }
    return runs;
}

/* sift */
void heapify(int n, int i) {
    int smallest = i;
    int l = 2*i + 1;
    int r = 2*i + 2;
    
    if(l < n && buff[l] < buff[smallest]) smallest = l;
    if(r < n && buff[r] < buff[smallest]) smallest = r;
    if(smallest != i) {
        int tmp = buff[i];
        buff[i] = buff[smallest];
        buff[smallest] = tmp;
        heapify(n, smallest);
    }
}

/* Modified heap sort for replacement selection */
int hsort() {
    int i, pos, pheap;
    int runs = 0, sheap = B_START, end = 0;
    int maxsize = (int) fread(buff, sizeof(int), BUFF_LIMIT, fp);
    int shend = (maxsize < BUFF_LIMIT) ? maxsize : BUFF_LIMIT;
    
    do {
        if(maxsize < B_START) {
            initialsize = maxsize;
            pheap = maxsize;
            end = 1;
        }
        else pheap = B_START;
        
        pos = 0;
        FILE *rp = fopen(getRun(runs, RUN), "wb");
        
        for (i = pheap / 2 - 1; i >= 0; i--)
            heapify(pheap, i);
        
        while(pheap) {
            output_buff[pos++] = buff[0];
            if( pos == BUFF_LIMIT ) {
                pos = 0;
                fwrite(output_buff, sizeof(int), BUFF_LIMIT, rp);
            }
            if(!end) {
                if(buff[0] > buff[sheap]) {
                    buff[0] = buff[--pheap];
                    buff[pheap] = buff[sheap];
                }
                else buff[0] = buff[sheap];
                
                /* Exhausted B */
                if(++sheap == shend) {
                    sheap = B_START;
                    int bytes_read = (int) fread(buff + sheap, sizeof(int), B_LIMIT, fp);
                    if( bytes_read != B_LIMIT ) {
                        shend = sheap + bytes_read;
                        if(bytes_read == 0) {
                            end = 1;
                            sheap = pheap;
                        }
                    }
                }
            }
            else buff[0] = buff[--pheap];
            heapify(pheap, 0);
        }
        fwrite(output_buff, sizeof(int), pos, rp);
        fclose(rp);
        runs++;
    } while(!end);
    
    if(runs == 1) return runs;
    
    /* Final heapify */
    pheap = 0;
    pos = 0;
    
    for(i = sheap; i < B_START; i++)
        buff[pheap++] = buff[i];
    
    for (i = pheap / 2 - 1; i >= 0; i--)
        heapify(pheap, i);
    
    for(i = pheap - 1; i >= 0; i--) {
        output_buff[pos++] = buff[0];
        buff[0] = buff[i];
        heapify(i, 0);
    }
    
    FILE *rp = fopen(getRun(runs, RUN), "wb");
    fwrite(output_buff, sizeof(int), pheap, rp);
    fclose(rp);
    
    return (runs + 1);
}

/* Returns true if all runs are exhausted */
int isEnd(int runs) {
    int r;
    for( r = 0; r < runs; r++) {
        if(finish[r] == 0) return 0;
    }
    return 1;
}

/* First fetch for n-way merge */
void initialFetch(int runs, int offset, int blocksize, int mode) {
    int r;
    for( r = 0; r < runs; r++) {
        runptr[r] = r * blocksize;
        endptr[r] = (r + 1) *  blocksize;
        finish[r] = 0;
        
        if(mode == RUN)
            fpr[r] = fopen(getRun(r + offset, RUN), "rb");
        else
            fpr[r] = fopen(getRun(r, SUPER_RUN), "rb");
        
        fread(buff + runptr[r], sizeof(int), blocksize, fpr[r]);
    }
}

/* Merge and write operation */
void merge(int runs, int blocksize, int mode) {
    int pos = 0, r;
    do {
        int minValue = INT_MAX;
        int minIndex = -1;
        for( r = 0; r < runs; r++ ) {
            if(!finish[r] && buff[runptr[r]] < minValue) {
                minValue = buff[runptr[r]];
                minIndex = r;
            }
        }
        output_buff[pos++] = minValue;
        
        /* Fetch another block from the run */
        if( ++runptr[minIndex] == endptr[minIndex] ) {
            if(runs == 1) {
                if(mode == RUN)
                    fwrite(output_buff, sizeof(int), initialsize, out);
                else
                    fwrite(output_buff, sizeof(int), initialsize, sub_out);
                return;
            }
            runptr[minIndex] = minIndex * blocksize;
            int items_read = (int) fread(buff + runptr[minIndex], sizeof(int), blocksize, fpr[minIndex]);
            if(items_read == 0) {
                finish[minIndex] = 1;
            } else if(items_read != blocksize) {
                endptr[minIndex] = runptr[minIndex] + items_read;
            }
        }
        
        if(pos == BUFF_LIMIT) {
            pos = 0;
            if(mode == RUN)
                fwrite(output_buff, sizeof(int), BUFF_LIMIT, out);
            else
                fwrite(output_buff, sizeof(int), BUFF_LIMIT, sub_out);
        }
    } while(!isEnd(runs));
}

/* Prints time taken for execution */
void printTime(struct timeval stime, struct timeval ftime) {
    struct timeval exec_tm;
    exec_tm.tv_sec = ftime.tv_sec - stime.tv_sec;
    exec_tm.tv_usec = ftime.tv_usec - stime.tv_usec;
    if(exec_tm.tv_usec < 0) {
        exec_tm.tv_usec  = 1000000 + exec_tm.tv_usec;
        exec_tm.tv_sec--;
    }
    printf("Time: %ld.%06d", exec_tm.tv_sec, exec_tm.tv_usec );
}

void basicMergeSort() {
    int runs = preSort();
    int r, blocksize = (runs == 1) ? initialsize : BUFF_LIMIT/runs;
    
    initialFetch(runs, 0, blocksize, RUN);
    merge(runs, blocksize, RUN);
    
    for(r = 0; r < runs; r++)
        fclose(fpr[r]);
}

void multiStepMergeSort() {
    int runs = preSort();
    int blocksize, subrange, r, superruns = runs/15;
    int itemcnt = superruns * 15;
    
    /* Produce super runs */
    for(subrange = 0; subrange < runs; subrange += 15) {
        sub_out = fopen(getRun(subrange/15, SUPER_RUN), "wb");
        int subruns = 15;
        if( subrange == itemcnt ) subruns = runs - subrange;
        blocksize = (runs == 1) ? initialsize : BUFF_LIMIT/subruns;
        
        initialFetch(subruns, subrange, blocksize, RUN);
        merge(subruns, blocksize, SUPER_RUN);
        
        fclose(sub_out);
        for(r = 0; r < subruns; r++) fclose(fpr[r]);
    }
    
    /* Merge super runs */
    if(itemcnt < runs) superruns++;
    blocksize = (runs == 1) ? initialsize : BUFF_LIMIT/superruns;
    initialFetch(superruns, 0, blocksize, SUPER_RUN);
    merge(superruns, blocksize, RUN);
}

void replacementMergeSort() {
    int runs = hsort();
    int r, blocksize = (runs == 1) ? initialsize : BUFF_LIMIT/runs;
    
    initialFetch(runs, 0, blocksize, RUN);
    merge(runs, blocksize, RUN);
    
    for(r = 0; r < runs; r++)
        fclose(fpr[r]);
}

int main(int argc, const char * argv[]) {
    if(argc != 4){
        printf("Arguments count mismatch!");
        return 0;
    }
    
    filename = argv[2];
    fp = fopen(filename, "rb");
    if(!fp) {
        printf("Failed to open %s!", filename);
        return 0;
    }
    
    struct timeval stime, ftime;
    const char *method = argv[1];
    out = fopen(argv[3], "wb");
    
    gettimeofday(&stime, NULL );
    if(strcmp(method, "--basic") == 0)
        basicMergeSort();
    else if(strcmp(method, "--multistep") == 0)
        multiStepMergeSort();
    else if(strcmp(method, "--replacement") == 0)
        replacementMergeSort();
    else
        printf("Not a valid merge-method!");
    gettimeofday(&ftime, NULL );
    printTime(stime, ftime);
    
    fclose(fp);
    fclose(out);
    return 0;
}
