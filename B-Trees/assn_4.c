#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAXSIZE 1000000
FILE *fp; /* Input file stream */
int order; /* Order of B-tree */
long root; /* Offset of B-tree root node */
long q[MAXSIZE]; /* Queue for level-order printing */
int of; /* Overflow flag */
long promoted[3];

typedef struct { /* B-tree node */
    int n; /* Number of keys in node */
    int *key; /* Node's keys */
    long *child; /* Node's child subtree offsets */
} btree_node;

/* Return root node of the tree */
btree_node getRootNode() {
    btree_node node;
    node.n = 0;
    node.key = (int *)calloc(order-1, sizeof(int));
    node.child = (long *)calloc(order, sizeof(long));
    fseek(fp, root, SEEK_SET);
    fread(&node.n, sizeof(int), 1, fp);
    fread(node.key, sizeof(int), order-1, fp);
    fread(node.child, sizeof(long), order, fp);
    return node;
}

/* Check if a key exists in the tree */
long find(long off, int k) {
    int s = 0;
    btree_node node;
    node.n = 0;
    node.key = (int *)calloc(order-1, sizeof(int));
    node.child = (long *)calloc(order, sizeof(long));
    fseek(fp, off, SEEK_SET);
    fread(&node.n, sizeof(int), 1, fp);
    fread(node.key, sizeof(int), order-1, fp);
    fread(node.child, sizeof(long), order, fp);
    
    while(s < node.n) {
        if(k == node.key[s]) {
            return 0L;
        } else if(k < node.key[s])
            break;
        else s++;
    }
    if(node.child[s] != 0)
        return find(node.child[s], k);
    return off; //return offset of leaf (when key not found)
}

/* Bottom-up recursive adder function */
int addHelper(long off, int k) {
    btree_node node;
    int retval = 1; /* flag indicating if k is added */
    
    /* Fetch node at offset 'off' */
    node.n = 0;
    node.key = (int *)calloc(order-1, sizeof(int));
    node.child = (long *)calloc(order, sizeof(long));
    fseek(fp, off, SEEK_SET);
    fread(&node.n, sizeof(int), 1, fp);
    fread(node.key, sizeof(int), order-1, fp);
    fread(node.child, sizeof(long), order, fp);
    
    int i, j;
    int s = 0;
    while(s < node.n) {
        if(k < node.key[s])
            break;
        else s++;
    }
    
    /* Search child for a position to insert k, if child exists */
    if(node.child[s] != 0) {
        retval = addHelper(node.child[s], k);
        if(of) { //Overflow in child
            of = 0;
            if(node.n != order - 1) {
                k = node.key[node.n - 1];
                //Rearrange keys and children in the node, to insert promoted key
                for(i = node.n; i > s; i--) {
                    node.key[i] = node.key[i-1];
                    node.child[i+1] = node.child[i];
                }
                if(s != order - 1) {
                    node.key[s] = (int) promoted[0];
                    node.child[s+1] = promoted[2];
                }
                node.n++;
                fseek(fp, off, SEEK_SET);
                fwrite(&node.n, sizeof(int), 1, fp);
                fwrite(node.key, sizeof(int), order-1, fp);
                fwrite(node.child, sizeof(long), order, fp);
                return 0;
            }
            else { //Overflow in current node
                of = 1;
                k = (int) promoted[0]; //Promoted key becomes k
            }
        }
    }
    if(of || retval) {
        if(node.n == order - 1) { //Overflow in the node
            of = 1;
            promoted[1] = off;
            /* Create right child node */
            btree_node right;
            right.key = (int *)calloc(order-1, sizeof(int));
            right.child = (long *)calloc(order, sizeof(long));
            int* splitkey = (int *)calloc(order, sizeof(int)); /* Extended array containing node keys and k */
            long* splitchild = (long *)calloc(order+1, sizeof(long)); /* Extended array containing node children and k's children references */
            int addref = 0, cp = 0;
            
            for(j = 0, i = 0; i < order - 1; j++) { //Arrange extended arrays in sorted order
                if(!addref && k < node.key[i]) {
                    addref = 1;
                    splitkey[j] = k;
                    splitchild[cp++] = node.child[j];
                    splitchild[cp++] = promoted[2];
                }
                else {
                    splitkey[j] = node.key[i++];
                    splitchild[cp++] = node.child[j];
                }
            }
            if(!addref) {
                splitkey[node.n] = k;
                splitchild[cp++] = node.child[node.n];
                splitchild[cp] = promoted[2];
            }
            for(i = node.n - (node.n/2) + 1, j = 0; i < node.n; i++, j++) { //Initialize right child node
                right.key[j] = splitkey[i];
                right.child[j] = splitchild[i];
            }
            right.key[j] = splitkey[i];
            right.child[j] = splitchild[i];
            right.child[j+1] = splitchild[i+1];
            right.n = node.n/2;
            
            /* Write right child node to disk */
            fseek(fp, 0, SEEK_END);
            promoted[2] = ftell(fp);
            fwrite(&right.n, sizeof(int), 1, fp);
            fwrite(right.key, sizeof(int), order-1, fp);
            fwrite(right.child, sizeof(long), order, fp);
            
            /* Change current node as left child node */
            node.n = node.n - (node.n/2);
            for(i = 0; i < node.n; i++) {
                node.key[i] = splitkey[i];
                node.child[i] = splitchild[i];
            }
            node.child[i] = splitchild[i];
            promoted[0] = splitkey[node.n];
        }
        else{ //No overflow, rearrange elements to insert k
            for(i = node.n; i > s; i--) {
                node.key[i] = node.key[i-1];
                node.child[i+1] = node.child[i];
            }
            node.key[s] = k;
            node.n++;
        }
        /* Write current/left child node to disk */
        fseek(fp, off, SEEK_SET);
        fwrite(&node.n, sizeof(int), 1, fp);
        fwrite(node.key, sizeof(int), order-1, fp);
        fwrite(node.child, sizeof(long), order, fp);
    }
    return 0;
}

/* Adds a key to the tree */
void add(int k) {
    btree_node node; /* Single B-tree node */
    
    /* Reset global variables */
    of = 0;
    promoted[0] = promoted[1] = promoted[2] = 0;
    
    if(root != -1) {
        if(find(root, k) == 0L) { //Check if key already exisits
            printf("Entry with key=%d already exists\n", k);
            return;
        }
        addHelper(root, k); //Recursive adder function
        if(of) { //Add new root
            node.n = 1;
            node.key = (int *)calloc(order-1, sizeof(int));
            node.child = (long *)calloc(order, sizeof(long));
            node.key[0] = (int) promoted[0];
            node.child[0] = promoted[1];
            node.child[1] = promoted[2];
            fseek(fp, 0L, SEEK_END);
            root = ftell(fp);
            fwrite(&node.n, sizeof(int), 1, fp);
            fwrite(node.key, sizeof(int), order-1, fp);
            fwrite(node.child, sizeof(long), order, fp);
            rewind(fp);
            fwrite(&root, sizeof(long), 1, fp);
        }
    }
    else { //Adding for the first time
        root = sizeof(long);
        rewind(fp);
        fwrite(&root, sizeof(long), 1, fp);
        node.n = 1;
        node.key = (int *)calloc(order-1, sizeof(int));
        node.child = (long *)calloc(order, sizeof(long));
        node.key[0] = k;
        
        fwrite(&node.n, sizeof(int), 1, fp);
        fwrite(node.key, sizeof(int), order-1, fp);
        fwrite(node.child, sizeof(long), order, fp);
    }
}

void print() {
    int i; /* Loop counter */
    btree_node node; /* Node to print */
    int nlevel, level = 0, rear = -1, front = 0, n = 1; /* Queue variables */
    
    node.n = 0;
    node.key = (int *)calloc(order-1, sizeof(int));
    node.child = (long *)calloc(order, sizeof(long));
    
    q[++rear] = root;
    while(n) { //while q not empty
        if(front == 100) front = 0;
        
        nlevel = rear - front + 1;
        printf(" %d: ", ++level);
        while(nlevel--) {
            fseek(fp, q[front++], SEEK_SET);
            n--;
            fread(&node.n, sizeof(int), 1, fp);
            fread(node.key, sizeof(int), order-1, fp);
            fread(node.child, sizeof(long), order, fp);
            
            for(i = 0; i < node.n - 1; i++) {
                printf("%d,", node.key[i]);
            }
            printf("%d ", node.key[node.n - 1]);
            for(i = 0; i <= node.n; i++) {
                if(node.child[i] == 0) break;
                if(rear == MAXSIZE - 1) rear = -1;
                if(n == MAXSIZE) return;
                q[++rear] = node.child[i];
                n++;
            }
        }
        printf("\n");
    }
}

int main(int argc, const char * argv[]) {
    if(argc != 3) {
        printf("Arguments count mismatch!");
        return 0;
    }
    
    fp = fopen(argv[1], "r+b");
    
    if (fp == NULL) {
        root = -1;
        fp = fopen(argv[1], "w+b" );
        fwrite(&root, sizeof(long), 1, fp);
    } else {
        fread(&root, sizeof(long), 1, fp);
    }
    
    order = atoi(argv[2]);
    char cmdtype[10];
    
    while(1) {
        scanf("%s", cmdtype);
        
        if(strcmp(cmdtype, "add") == 0) {
            int k;
            scanf("%d", &k);
            add(k);
        }
        else if (strcmp(cmdtype, "find") == 0) {
            int tg;
            scanf("%d", &tg);
            if(root != -1 && find(root, tg) == 0L) {
                printf("Entry with key=%d exists\n", tg);
                continue;
            }
            printf("Entry with key=%d does not exist\n", tg);
        }
        else if (strcmp(cmdtype, "print") == 0) {
            if(root == -1) continue;
            print();
        }
        else if (strcmp(cmdtype, "end") == 0) {
            fclose(fp);
            return 0;
        }
        else {
            printf("Not a valid command\n");
            continue;
        }
    }
    return 0;
}
