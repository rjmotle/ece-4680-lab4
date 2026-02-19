#include <stdio.h>
#include <stdlib.h>

struct node {
    unsigned int freq;
    unsigned char symbol;
    struct node *left;
    struct node *right;
};

int nodeCompare(const void *a, const void *b) {
    return (* (struct node **) a)->freq - (* (struct node **) b)->freq;
}

void genSymbolTable(struct node *node) {
    if (node->freq == 1) {
        /* generate table */
    } else {
        if (node->left != NULL) genSymbolTable(node->left);
        if (node->right != NULL) genSymbolTable(node->left);
    }
}

int main(int argc, char *argv[]) {
    FILE *src, *dest;
    unsigned char buffer;
    unsigned int freq[256];
    struct node *node_ptrs[256];
    unsigned int node_count = 0;
    struct node **node_head, *node_temp;

    /* show proper usage if insufficient command line args */
    if (argc != 3) {
        printf("usage: huffman [src] [dest]\n");
        return 1;
    }

    /* try to open files, give error if cannot */
    if ((src = fopen(argv[1], "rb")) == NULL) {
        printf("cannot open %s for reading.\n", argv[1]);
        return 1;
    }
    if ((dest = fopen(argv[2], "wb")) == NULL) {
        printf("cannot open %s for writing.\n", argv[2]);
        return 1;
    }

    /* calculate frequencies of symbols */
    for (int i = 0; i < 256; i++) freq[i] = 0;
    while (fread(&buffer, sizeof(unsigned char), 1, src)) freq[buffer]++;

    /* prepare array for symbols that appear */
    for (int i = 0; i < 256; i++) {
        if (freq[i]) {
            node_ptrs[node_count] = calloc(1, sizeof(struct node));
            node_ptrs[node_count]->freq = freq[i];
            node_ptrs[node_count++]->symbol = i;
        }
    }

    /* sort nodes smallest to large and create binary tree */
    node_head = node_ptrs;
    while (node_count > 1) {
        qsort(node_head, node_count--, sizeof(struct node *), nodeCompare);
        node_temp = calloc(1, sizeof(struct node));
        node_temp->freq = node_head[0]->freq + node_head[1]->freq;
        node_temp->left = node_head[0];
        node_temp->right = node_head[1];
        node_head[1] = node_temp;
        node_head++;
    }

    /* write out */
    fseek(src, 0, SEEK_SET);
    
    fclose(src);
    return 0;
}