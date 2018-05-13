#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FIRSTFIT 1
#define BESTFIT 2
#define WORSTFIT 3

FILE *fp; /* Input/output stream */
FILE *out; /* Output file stream */
FILE *avl; /* Availability list file stream */
int indexsize, availsize;
int mode;

typedef struct {
    int key; /* Record's key */
    long off; /* Record's offset in file */
} index_S;

typedef struct {
    int siz; /* Hole's size */
    long off; /* Hole's offset in file */
} avail_S;

index_S prim[ 10000 ]; /* Primary key index */
avail_S avail[ 10000 ]; /* Availability index */

/* Descending key comparator for index.bin */
int primcomparator(const void *p, const void *q)
{
    int l = ((index_S *)p)->key;
    int r = ((index_S *)q)->key;
    return (r - l);
}

/* Descending size comparator for avail.bin */
int availcomparator(const void *p, const void *q)
{
    int l = ((avail_S *)p)->siz;
    int r = ((avail_S *)q)->siz;
    if ( l == r ) {
        if( mode == WORSTFIT )
            return (int)(((avail_S *)p)->off - ((avail_S *)q)->off);
        return (int)(((avail_S *)q)->off - ((avail_S *)p)->off);
    }
    return (r - l);
}

int binSearch(int target, index_S prim[], int low, int high) {
    if(low <= high) {
        int mid = (low + high) / 2;
        if(prim[mid].key == target)
            return mid;
        else if(prim[mid].key > target)
            return binSearch(target, prim, mid + 1, high);
        else
            return binSearch(target, prim, low, mid - 1);
    }
    return -1;
}

/* Inserts new hole at the last */
void firstfit(int pos) {
    int i;
    avail[availsize] = avail[pos];
    for(i = pos+1; i <= availsize; i++) {
        avail[i-1] = avail[i];
    }
}

/* Descending sort - for Best and Worst fit */
void sortedfit() {
    qsort(avail, availsize, sizeof(avail_S), availcomparator);
}

/* Deletes record from availability list */
long delfromavail(int i, int size) {
    int rec_siz = avail[i].siz - size;
    long off =avail[i].off;
    avail[i].siz = rec_siz;
    avail[i].off = avail[i].off + size;
    (mode == FIRSTFIT) ? firstfit(i) : sortedfit();
    if(rec_siz == 0)
        availsize--;
    return off;
}

/* Finds hole using the availability list */
long findHole(int size) {
    int i;

    if(mode != BESTFIT) {
        for(i = 0; i < availsize; i++)
            if(avail[i].siz >= size)
                return delfromavail(i, size);
    } else {
        for(i = availsize-1; i >= 0; i--)
            if(avail[i].siz >= size)
                return delfromavail(i, size);
    }
    
    fseek(fp, 0, SEEK_END);
    return ftell(fp);
}

int main(int argc, const char * argv[]) {
    if(argc != 3){
        printf("Arguments count mismatch!");
        return 0;
    }
    
    const char *srch = argv[1];
    if(strcmp(srch, "--first-fit") == 0)
        mode = FIRSTFIT;
    else if(strcmp(srch, "--best-fit") == 0)
        mode = BESTFIT;
    else if(strcmp(srch, "--worst-fit") == 0)
        mode = WORSTFIT;
    else
        printf("Not a valid search mode!");
    
    char cmdtype[100];
    
    long rec_off; /* Record offset */
    int rec_siz; /* Record size, in characters */
    int i;

    /* If student.db doesn't exist, create it */
    if ( ( fp = fopen( argv[2], "r+b" ) ) == NULL ) {
        fp = fopen( argv[2], "w+b" );
    }
    else {
        /* If index.bin exists, load into memory */
        if((out = fopen("index.bin", "rb")) != NULL) {
            fseek(out, 0, SEEK_END);
            indexsize = (int) ftell(out)/sizeof(index_S);
            rewind(out);
            fread(prim, sizeof(index_S), indexsize, out);
            fclose(out);
        }

        /* If avail.bin exists, load into memory */
        if((avl = fopen("avail.bin", "rb")) != NULL) {
            fseek(avl, 0, SEEK_END);
            availsize = (int) ftell(avl)/sizeof(avail_S);
            rewind(avl);
            fread(avail, sizeof(avail_S), availsize, avl);
            fclose(avl);
        }
    }
    
    while(1) {
        scanf("%s", cmdtype);
        
        /* add - adds new record */
        if(strcmp(cmdtype, "add") == 0) {
            scanf("%d %s", &prim[indexsize].key, cmdtype);
            rec_siz = (int) strlen(cmdtype);
            
            //Check if the key already exists
            if(binSearch(prim[indexsize].key, prim, 0, indexsize-1) >= 0) {
                printf("Record with SID=%d exists\n", prim[indexsize].key);
                continue;
            }
            
            //Find a hole to insert the record
            rec_off = findHole(sizeof(int) + rec_siz);
            fseek(fp, rec_off, SEEK_SET);
            
            //Update and sort primary indices
            prim[indexsize++].off = rec_off;
            qsort(prim, indexsize, sizeof(index_S), primcomparator);
            
            //Write to student.db
            fwrite( &rec_siz, sizeof(int), 1, fp );
            fwrite( cmdtype, strlen( cmdtype ), 1, fp );
        }
        /* find - prints details if a record exists */
        else if (strcmp(cmdtype, "find") == 0) {
            int target;
            char* buf;
            scanf("%d", &target);
            
            //Check if the record exists
            int keyfound = binSearch(target, prim, 0, indexsize-1);
            if(keyfound != -1) {
                rec_off = prim[keyfound].off;
                fseek( fp, rec_off, SEEK_SET );
                fread( &rec_siz, sizeof( int ), 1, fp );
                buf = malloc( rec_siz );
                fread( buf, 1, rec_siz, fp );
                buf[rec_siz] = '\0';
		printf("%s\n",buf);
            }
            else
                printf("No record with SID=%d exists\n", target);
            
        }
        /* del - deletes a record, if it exists */
        else if (strcmp(cmdtype, "del") == 0) {
            int target;
            scanf("%d", &target);
            
            //Check if the record exists
            int keyfound = binSearch(target, prim, 0, indexsize-1);
            if(keyfound != -1) {
                rec_off = prim[keyfound].off;
                
                //Add a hole to availability list
                avail[availsize].off = rec_off;
                fseek( fp, rec_off, SEEK_SET );
                fread( &rec_siz, sizeof( int ), 1, fp );
                avail[availsize++].siz = sizeof(int) + rec_siz;
                
                //Sort the availability list if required
                if(mode != FIRSTFIT) {
                    sortedfit();
                }
                
                //Delete record from the index list
                prim[keyfound].key = 0;
                qsort(prim, indexsize, sizeof(index_S), primcomparator);
                indexsize--;
            }
            else
                printf("No record with SID=%d exists\n", target);
        }
        /* end - saves files to disks, then exits */
        else if (strcmp(cmdtype, "end") == 0) {
            fclose(fp);
            
            //Write to index.bin
            out = fopen( "index.bin", "wb" );
            fwrite( prim, sizeof( index_S ), indexsize, out );
            fclose( out );
            
            //Write to avail.bin
            avl = fopen( "avail.bin", "wb" );
            fwrite( avail, sizeof( avail_S ), availsize, avl );
            fclose( avl );
            
            //Print index list
            printf("Index:\n");
            for(i = indexsize-1 ; i >= 0; i--)
                printf( "key=%d: offset=%ld\n", prim[i].key, prim[i].off );
            printf("Availability:\n");
            
            //Print availability list
            int hole_siz = 0;
            if(mode != BESTFIT) {
                for(i=0;i<availsize;i++) {
                    hole_siz += avail[i].siz;
                    printf( "size=%d: offset=%ld\n", avail[i].siz, avail[i].off );
                }
            } else {
                for(i = availsize - 1 ; i >= 0; i--) {
                    hole_siz += avail[i].siz;
                    printf( "size=%d: offset=%ld\n", avail[i].siz, avail[i].off );
                }
            }
            printf( "Number of holes: %d\n", availsize );
            printf( "Hole space: %d\n", hole_siz );
            
            //exit
            return 0;
        }
        else {
            printf( "Not a valid command\n" );
            continue;
        }
        
    }
    return 0;
}
