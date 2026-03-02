#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
    unsigned int freq;
    unsigned char symbol;
    struct node *left;
    struct node *right;
};

int nodeCompare(const void *a, const void *b) {
    return (* (struct node **) a)->freq - (* (struct node **) b)->freq;
}

struct node *buildTree(unsigned int freq[256]) {
    struct node *node_ptrs[256];
    unsigned int node_count = 0;
    struct node *node_temp;

    /* prepare array for symbols that appear */
    for (int i = 0; i < 256; i++) {
        if (freq[i]) {
            node_ptrs[node_count] = calloc(1, sizeof(struct node));
            node_ptrs[node_count]->freq = freq[i];
            node_ptrs[node_count++]->symbol = i;
        }
    }

    if (node_count == 0)
        return NULL;

    /* sort nodes smallest to large and create binary tree */
    while (node_count > 1) {
        qsort(node_ptrs, node_count--, sizeof(struct node *), nodeCompare);
        node_temp = calloc(1, sizeof(struct node));
        node_temp->freq = node_ptrs[0]->freq + node_ptrs[1]->freq;
        node_temp->left = node_ptrs[0];
        node_temp->right = node_ptrs[1];
        
        node_ptrs[0] = node_temp;
        /* shift nodes down */
        for (unsigned int i = 1; i <= node_count; i++) {
            node_ptrs[i] = node_ptrs[i+1];
        }
    }

    return node_ptrs[0];
}

void genSymbolTableInner(struct node *node, char **symbolTable,
                         char bitCode[256], unsigned int bitCount) {
    if (node->left == NULL && node->right == NULL) {
        bitCode[bitCount] = '\0';
        
        /* store string code in symbol table */
        symbolTable[node->symbol] = calloc(bitCount + 1, sizeof(char));
        for (int i = 0; i < bitCount; i++) symbolTable[node->symbol][i] = bitCode[i];
    } else {
        if (node->left != NULL) {
            /* add 0 to code for left child */
            bitCode[bitCount] = '0';
            genSymbolTableInner(node->left, symbolTable, bitCode, bitCount+1);
        }
        if (node->right != NULL){
            /* add 1 to code for right child */
            bitCode[bitCount] = '1';
            genSymbolTableInner(node->right, symbolTable, bitCode, bitCount+1);
        }
    }

}

void genSymbolTable(struct node *node, char **symbolTable) {
    char bitCode[256];

    // special case: only one symbol in tree
    if (node->left == NULL && node->right == NULL) {
        symbolTable[node->symbol] = calloc(2, sizeof(char));
        symbolTable[node->symbol][0] = '0';
        symbolTable[node->symbol][1] = '\0';
        return;
    }

    genSymbolTableInner(node, symbolTable, bitCode, 0);
}

void writeCompressed(FILE *src, FILE *dest, char *symbolTable[256]) {
    unsigned char buffer;
    unsigned char outByte = 0;
    int bitCount = 0;

    while (fread(&buffer, 1, 1, src)) {
        char *code = symbolTable[buffer];

        /* string code -> bits code */
        for (int i = 0; code[i] != '\0'; i++) {
            outByte <<= 1;

            if (code[i] == '1')
                outByte |= 1;

            bitCount++;

            if (bitCount == 8) {
                fwrite(&outByte, 1, 1, dest);
                outByte = 0;
                bitCount = 0;
            }
        }
    }

    /* remaining bits, pad with 0s */
    if (bitCount > 0) {
        outByte <<= (8 - bitCount);
        fwrite(&outByte, 1, 1, dest);
    }
}

void writeDecompressed(FILE *compressed, FILE *output, struct node *root,
                       unsigned int totalSymbols) {
                        
    // special case: only one symbol in tree
    if (root->left == NULL && root->right == NULL) {
        for (unsigned int i = 0; i < totalSymbols; i++) {
            fwrite(&root->symbol, 1, 1, output);
        }
        return;
    }
    
    struct node *current = root;
    unsigned char byte;
    unsigned int decoded = 0;

    while (fread(&byte, 1, 1, compressed) && decoded < totalSymbols) {

        for (int i = 7; i >= 0; i--) {

            unsigned char bit = (byte >> i) & 0x01;

            /* follow the tree */
            if (bit == 0)
                current = current->left;
            else if (bit == 1)
                current = current->right;

            /* end of tree */
            if (current->left == NULL && current->right == NULL) {

                fwrite(&current->symbol, 1, 1, output);
                decoded++;

                /* reset to root */
                current = root;

                /* skip padded bits from compress */
                if (decoded == totalSymbols)
                    break;
            }
        }
    }
}

void freeTree(struct node *node) {
    if (node == NULL)
        return;
    freeTree(node->left);
    freeTree(node->right);
    free(node);       
}

int main(int argc, char *argv[]) {
    FILE *src, *dest;
    unsigned char buffer;
    unsigned int freq[256];
    struct node *root;


    /* show proper usage if insufficient command line args */
    if (argc != 4) {
        printf("usage: huffman [comp/decomp] [src] [dest]\n");
        return 1;
    }

    /* try to open files, give error if cannot */
    if ((src = fopen(argv[2], "rb")) == NULL) {
        printf("cannot open %s for reading.\n", argv[2]);
        return 1;
    }
    if ((dest = fopen(argv[3], "wb")) == NULL) {
        printf("cannot open %s for writing.\n", argv[3]);
        return 1;
    }

    if (strcmp(argv[1], "comp") == 0) // Compression
    {
            
        /* calculate frequencies of symbols */
        for (int i = 0; i < 256; i++) freq[i] = 0;
        while (fread(&buffer, sizeof(unsigned char), 1, src)) freq[buffer]++;

        /* write freqency data */
        FILE *freqFile = fopen("freq.bin", "wb");
        if (!freqFile) {
            printf("cannot open freq.bin for writing\n");
            return 1;
        }
        fwrite(freq, sizeof(unsigned int), 256, freqFile);
        fclose(freqFile);


        root = buildTree(freq);
        if (!root) {
            printf("Empty file\n");
            return 0;
        }


        char *symbolTable[256] = {0};
        /* create symbol table */
        genSymbolTable(root, symbolTable);

        /* set file to beginning */
        fseek(src, 0, SEEK_SET);

        writeCompressed(src, dest, symbolTable);

        // free symbol table
        for (int i = 0; i < 256; i++) {
            free(symbolTable[i]);
        }

        freeTree(root);

    }
    else if (strcmp(argv[1], "decomp") == 0) // Decompression
    {
        unsigned int totalSymbols = 0;

        FILE *freqFile = fopen("freq.bin", "rb");
        if (!freqFile) {
            printf("cannot open freq.bin for writing\n");
            return 1;
        }

        fread(freq, sizeof(unsigned int), 256, freqFile);

        root = buildTree(freq);
        if (!root) {
            printf("Empty file\n");
            return 0;
        }

        for (int i = 0; i < 256; i++)
            totalSymbols += freq[i];

        writeDecompressed(src, dest, root, totalSymbols);
        freeTree(root);
    }
        
    fclose(dest);
    fclose(src);

    return 0;
}