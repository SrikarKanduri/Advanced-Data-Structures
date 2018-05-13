#include <stdio.h>
#include <string.h>
#include <sys/time.h>

unsigned int keysize, seeksize;
struct timeval start, finish, exec_tm;
void print(int[], int[]);

void memLin(int hit[], int S[], FILE* keydb) {
    int i, j;
    int K[keysize];
    
    gettimeofday(&start, NULL);
    fread(K, sizeof(int), keysize, keydb);   //Read key.db into memory
    for(i = 0; i < seeksize; i++) {
        hit[i] = 0;
        for(j = 0; j < keysize; j++) {
            if(S[i] == K[j]) {
                hit[i] = 1;
                break;
            }
        }
    }
    gettimeofday(&finish, NULL);
    exec_tm.tv_sec += finish.tv_sec - start.tv_sec;
    exec_tm.tv_usec += finish.tv_usec - start.tv_usec;

    print(hit, S);
}

int memBinHelper(int low, int high, int seek, int K[]) {
    if(low <= high) {
        int mid = (low + high) / 2;
        if(K[mid] == seek)
            return 1;
        else if(K[mid] < seek)
            return memBinHelper(mid + 1, high, seek, K);
        else
            return memBinHelper(low, mid - 1, seek, K);
    }
    return 0;
}

void memBin(int hit[], int S[], FILE* keydb) {
    int K[keysize];
    int i;
    
    gettimeofday(&start, NULL);
    fread(K, sizeof(int), keysize, keydb);   //Read key.db into memory
   
    for(i = 0; i < seeksize; i++)
        hit[i] = memBinHelper(0, keysize - 1, S[i], K);
    
    gettimeofday(&finish, NULL);
    exec_tm.tv_sec += finish.tv_sec - start.tv_sec;
    exec_tm.tv_usec += finish.tv_usec - start.tv_usec;
    
    print(hit, S);
}

void diskLin(int hit[], int S[], FILE *keydb) {
    int i, j, key;
    
    gettimeofday(&start, NULL);
    for(i = 0; i < seeksize; i++) {
        fseek(keydb, 0, SEEK_SET);
        hit[i] = 0;
        
        for(j = 0; j < keysize; j++) {
            fread(&key, sizeof(int), 1, keydb);
            
            if(S[i] == key) {
                hit[i] = 1;
                break;
            }
        }
    }
    gettimeofday(&finish, NULL);
    exec_tm.tv_sec = finish.tv_sec - start.tv_sec;
    exec_tm.tv_usec = finish.tv_usec - start.tv_usec;
    
    print(hit, S);
}

int diskBinHelper(int low, int high, int seek, FILE *keydb) {
    int key;
    if(low <= high) {
        int mid = (low + high) / 2;
        fseek(keydb, mid * sizeof(int), SEEK_SET);
        fread(&key, sizeof(int), 1, keydb);
        if(key == seek)
            return 1;
        else if(key < seek)
            return diskBinHelper(mid + 1, high, seek, keydb);
        else
            return diskBinHelper(low, mid - 1, seek, keydb);
    }
    return 0;
}

void diskBin(int hit[], int S[], FILE *keydb) {
    int i;
    
    gettimeofday(&start, NULL);
    for(i = 0; i < seeksize; i++)
        hit[i] = diskBinHelper(0, keysize - 1, S[i], keydb);

    gettimeofday(&finish, NULL);
    exec_tm.tv_sec = finish.tv_sec - start.tv_sec;
    exec_tm.tv_usec = finish.tv_usec - start.tv_usec;
    
    print(hit, S);
}

void print(int hit[], int S[]) {
    int i;
    for(i = 0; i < seeksize; i++){
        if(hit[i] == 1)
            printf("%12d: Yes\n", S[i]);
        else
            printf("%12d: No\n", S[i]);
    }
    
    if(exec_tm.tv_usec < 0) {
        exec_tm.tv_usec  = 1000000 + exec_tm.tv_usec;
        exec_tm.tv_sec--;
    }
    
    printf( "Time: %ld.%06ld\n", exec_tm.tv_sec,  exec_tm.tv_usec);
}

int main(int argc, const char * argv[]) {
    
    if(argc != 4){
        printf("Arguments count mismatch!");
        return 0;
    }
    
    FILE *keydb, *seekdb;
    
    //Opening key.db
    gettimeofday(&start, NULL);
    keydb = fopen(argv[2], "rb");
    if(!keydb) {
        printf("Failed to open key.db!");
        return 0;
    }
    gettimeofday(&finish, NULL);
    exec_tm.tv_sec = finish.tv_sec - start.tv_sec;
    exec_tm.tv_usec = finish.tv_usec - start.tv_usec;
    
    //Size of key.db
    fseek(keydb, 0, SEEK_END);
    keysize = (int) ftell(keydb)/sizeof(int);
    rewind(keydb);
    
    //Opening seek.db
    seekdb = fopen(argv[3], "rb");
    if(!seekdb) {
        printf("Failed to open seek.db!");
        return 0;
    }
    
    //Read seek.db into memory
    fseek(seekdb, 0, SEEK_END);
    seeksize = (int) ftell(seekdb)/sizeof(int);
    int S[seeksize];
    rewind(seekdb);
    fread(S, sizeof(int), seeksize, seekdb);
    
    int hit[seeksize];
    
    const char *mode = argv[1];
    if(strcmp(mode, "--mem-lin") == 0)  //In-memory linear search
        memLin(hit, S, keydb);
    else if(strcmp(mode, "--mem-bin") == 0)  //In-memory binary search
        memBin(hit, S, keydb);
    else if(strcmp(mode, "--disk-lin") == 0) //On-disk linear search
        diskLin(hit, S, keydb);
    else if(strcmp(mode, "--disk-bin") == 0) //On-disk binary search
        diskBin(hit, S, keydb);
    else
        printf("Not a valid search mode!");
    
    fclose(keydb);
    fclose(seekdb);
    return 0;
}
