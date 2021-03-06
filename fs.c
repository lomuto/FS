#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum DATA_SIZE {
    IBLOCK_CNT = 5,
    DATA_CNT = 56,
    BLK_CNT = 64,
    FILE_CNT = 80,
    INODE_SIZE = 256,
    BLK_SIZE = 4096
};

enum BLOCK {
    SUPER = 0,
    IBMAP,
    DBMAP,
    IBLOCK,
    DATA_REGION = 8
};

enum CMD {
    WRITE = 'w',
    READ = 'r',
    DELETE = 'd'
};

typedef struct _Block {
    unsigned char segment[4096];
} BLK;

typedef struct _Inode {
    unsigned int fsize;
    unsigned int blocks;
    unsigned int pointer[12];
    unsigned int NOT_USED_SPACE[50];
} Inode;

typedef struct _Dir {
    char inum;
    char name[3];
} Dir;

/****************************** NODE **********************************/

typedef struct _Node {
    void *address;
    struct _Node *next;
    struct _Node *prev;
} Node;

/**********************************************************************/

/****************************** QUEUE *********************************/

typedef struct _Queue {
    Node *head;
    Node *tail;
    int size;
} Queue;

void init_q(Queue **new_q) {
    (*new_q) = (Queue *)malloc(sizeof(Queue));
    (*new_q)->head = (Node *)malloc(sizeof(Node));
    (*new_q)->tail = (Node *)malloc(sizeof(Node));
    (*new_q)->head->prev = NULL;
    (*new_q)->tail->next = NULL;
    (*new_q)->head->next = (*new_q)->tail;
    (*new_q)->tail->prev = (*new_q)->head;
    (*new_q)->size = 0;

    return;
}

void enqueue(Queue **q, void *address) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->address = address;
    new_node->next = NULL;

    if ((*q)->size == 0) {
        (*q)->size = 1;
        (*q)->head = (*q)->tail = new_node;
        return;
    }

    ((*q)->size)++;
    Node *prev_tail = (*q)->tail;
    prev_tail->next = new_node;
    (*q)->tail = new_node;
    new_node->prev = prev_tail;
}

int dequeue(Queue **q, void **address) {
    if ((*q)->size == 0) {
        return -1;
    }

    *address = (*q)->head->address;
    Node *prev_head = (*q)->head;

    if ((*q)->size == 1) {
        ((*q)->size)--;
        (*q)->head = (*q)->tail = NULL;
        free(prev_head);
        return 1;
    }

    ((*q)->size)--;
    (*q)->head = prev_head->next;
    (*q)->head->prev = NULL;

    free(prev_head);
    return 1;
}

void push_front(Queue **q, void *address) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->address = address;
    new_node->next = NULL;

    if ((*q)->size == 0) {
        (*q)->size = 1;
        (*q)->head = (*q)->tail = new_node;
        return;
    }

    ((*q)->size)++;
    Node *prev_head = (*q)->head;
    prev_head->prev = new_node;
    (*q)->head = new_node;
    new_node->next = prev_head;
}

/**********************************************************************/
/*
*   Proto types of functions
*/
void init();
void print_EOF(unsigned const char *);
Inode *create_inode();
BLK *create_data();
int set_bitmap(BLK **);
void set_file_inode(Inode **inode, unsigned file_size, char byte);
void set_dir_inode(Inode **inode, unsigned file_size);
void read_file_inode(Inode **inode, unsigned file_size);
void remove_file_inode(int inum);

/**********************************************************************/

BLK *block;
Queue *inode_freelist;
Queue *data_freelist;
Inode *root_dir;
Queue *root_dir_freelist;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("ku_fs: Wrong number of arguments\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("ku_fs: Fail to open the input file\n");
        return 1;
    }

    init();
    assert(block != NULL);

    char buf[512];

    char f_name[3];
    char cmd;
    char c_size[16];
    while (fgets(buf, 512, fp)) {
        memcpy(f_name, buf, 2);
        f_name[2] = '\0';
        cmd = buf[3];
        int i = 0;
        for (char *ptr = buf + 5; i + 5 < strlen(buf); ptr++) {
            c_size[i++] = *ptr;
        }
        c_size[i] = '\0';

        switch (cmd) {
        case (WRITE): {
            int flag = 0;
            Dir *dirs = (Dir *)(block + DATA_REGION + (root_dir->pointer)[0]);
            for (int i = 0; i < 80 - root_dir_freelist->size; i++) {
                if (strcmp(dirs[i].name, f_name) == 0) {
                    flag = 1;
                    break;
                }
            }
            if (flag) {
                printf("Already exists\n");
                break;
            }
            Inode *new_inode;
            if ((new_inode = create_inode()) == NULL) {
                break;
            }
            int inode_num = new_inode - (Inode *)(block + IBLOCK);

            Dir *dir;
            if (dequeue(&root_dir_freelist, (void *)&dir) == -1) {
                perror("Error in root dir queue\n");
                exit(0);
            }
            dir->inum = inode_num;
            strcpy(dir->name, f_name);
            set_file_inode(&new_inode, atoi(c_size), f_name[0]);
            break;
        }
        case (READ): {
            Dir *dirs = (Dir *)(block + DATA_REGION + (root_dir->pointer)[0]);
            Inode *file = NULL;
            for (int i = 0; i < 80 - root_dir_freelist->size; i++) {
                if (strcmp(dirs[i].name, f_name) == 0) {
                    file = ((Inode *)(block + IBLOCK)) + dirs[i].inum;
                    break;
                }
            }
            if (file != NULL) {
                read_file_inode(&file, atoi(c_size));
            } else {
                printf("No such file\n");
            }
            break;
        }
        case (DELETE): {
            int flag = 0;
            Dir *dirs = (Dir *)(block + DATA_REGION + (root_dir->pointer)[0]);
            int inum;
            for (int i = 0; i < 80 - root_dir_freelist->size; i++) {
                if (strcmp(dirs[i].name, f_name) == 0) {
                    // inum 0?????? ????????? block?????????????????? ?????? remove ????????????
                    inum = dirs[i].inum;
                    dirs[i].inum = 0;
                    flag = 1;
                    break;
                }
            }
            if (flag == 0) {
                printf("No such file\n");
                break;
            }
            remove_file_inode(inum);
            break;
        }
        default:
            perror("Invalid cmd\n");
            exit(0);
            break;
        }
    }

    print_EOF((unsigned char *)block);
    // printf("%.2x\n", (block + IBMAP)->segment[0]);
    // printf("%.2x\n", (block + DBMAP)->segment[0]);
    // for (int i = 0; i < BLK_CNT; i++) {
    //     printf("%.2x\n", (block + i)->segment[0]);
    // }
}

/**********************************************************************/

void init() {
    block = (BLK *)calloc(sizeof(BLK), 64);
    init_q(&inode_freelist);
    init_q(&data_freelist);
    init_q(&root_dir_freelist);

    for (int i = DATA_REGION; i < BLK_CNT; i++) {
        enqueue(&data_freelist, block + i);
    }

    Inode *inode_entry = (Inode *)(block + IBLOCK);
    for (int i = 0; i < FILE_CNT; i++) {
        enqueue(&inode_freelist, inode_entry + i);
    }

    create_inode();                    // no inode
    create_inode();                    // bad block
    root_dir = (void *)create_inode(); // root dir

    //init root
    set_dir_inode(&root_dir, 80 * 4);
    Dir *root_dir_entry = (Dir *)(block + DATA_REGION + (root_dir->pointer)[0]);
    for (int i = 0; i < FILE_CNT; i++) {
        enqueue(&root_dir_freelist, root_dir_entry + i);
    }
}

Inode *create_inode() {
    BLK *ibmap_entry = block + IBMAP;
    int inode_num = set_bitmap(&ibmap_entry);

    // ?????? ????????? inode??? ??????????????? ?????? ???????????? ??????
    // // ibmap??? 1110111... ??? ??????
    if (inode_num < FILE_CNT - inode_freelist->size) {
        return (Inode *)(block + IBLOCK) + inode_num;
    }

    // ibmap??? ???????????? 11111... ??? ??????
    void *dequeued;
    if (dequeue(&inode_freelist, &dequeued) == -1) {
        printf("No space\n");
        return NULL;
    }

    return dequeued;
}

BLK *create_data() {
    BLK *dbmap_entry = block + DBMAP;
    int data_blk_num = set_bitmap(&dbmap_entry);

    // ?????? ????????? iblock??? ??????????????? ?????? ???????????? ??????
    // // dbmap??? 1110111... ??? ??????
    if (data_blk_num < DATA_CNT - data_freelist->size) {
        return block + DATA_REGION + data_blk_num;
    }

    // dbmap??? ???????????? 11111... ??? ??????
    void *dequeued;
    if (dequeue(&data_freelist, &dequeued) == -1) {
        return NULL;
    }

    return dequeued;
}

int set_bitmap(BLK **bmap) {
    /*
    *   bitmap block??? 4096??? 8?????? ????????? ??????.
    *   1?????? `unsigned char`?????? 8???
    *   ????????? 0?????? ???????????? ?????? loop break.
    */
    int row;
    int col;
    unsigned char check_mask = 0b10000000;
    for (row = 0;; row++) {
        int flag = 0;
        int cnt = 0;
        unsigned char curr_bit = (*bmap)->segment[row];

        while (cnt != 8) {
            // & ??????????????? ==???????????? ???????????? ????????? ?????? ???????????? ????????????
            if ((check_mask & curr_bit) == 0) {
                col = cnt;
                flag = 1;
                break;
            }
            curr_bit <<= 1;
            cnt++;
        }

        if (flag) {
            break;
        }
    }

    /*
    *   ????????? ?????? ????????? ???????????? ????????? 1 OR ???????????? ??????.
    */
    unsigned char mark = 0b10000000;
    unsigned char len = mark >>= col;
    unsigned char row_full = 0b11111111;
    (*bmap)->segment[row] |= len;

    return row * 8 + col;
}

void print_EOF(const unsigned char *block) {
    for (int i = 0; i < BLK_CNT * BLK_SIZE; i++, block++) {
        printf("%.2x ", *block);
    }
}

void set_file_inode(Inode **inode, unsigned int file_size, char byte) {
    (*inode)->fsize = file_size;
    // file ????????? disk total????????? ??? ??????. blocks count
    int blocks_cnt = 0;

    // block ????????????
    // 8192 ???????????? ??????2????????? 3??? ?????????. mod???????????? ???????????? ?????????
    int it = file_size % 4096 == 0 ? file_size / 4096 : file_size / 4096 + 1;
    BLK *temp;
    for (int i = 0; i < it; i++) {
        if ((temp = create_data()) == NULL) {
            printf("No space\n");
            (*inode)->fsize = BLK_SIZE * blocks_cnt;
            break;
        }
        blocks_cnt++;
        ((*inode)->pointer)[i] = temp - (block + DATA_REGION);
    }
    (*inode)->blocks = blocks_cnt;

    //block??? ??????
    for (int i = 0; i < (*inode)->blocks; i++) {
        int iteration = (i == (*inode)->blocks - 1 ? file_size - ((*inode)->blocks - 1) * 4096 : 4096);
        // file size??? ?????? ?????? ???????????? segfault
        // j??? segment ??????????????? 4095??? ?????? for??? ???????????? ??????
        for (int j = 0; j < iteration && j < BLK_SIZE; j++) {
            ((BLK *)(block + DATA_REGION + ((*inode)->pointer)[i]))->segment[j] = byte;
        }
    }
    // if given file size is bigger than previous one, blocks size has to be changed
}

void set_dir_inode(Inode **inode, unsigned file_size) {
    (*inode)->fsize = file_size;
    void *address = create_data();
    if (address == NULL) {
        printf("No space\n");
        return;
    }
    ((*inode)->pointer)[0] = (int)(address - (void *)(block + DATA_REGION));
    (*inode)->blocks = 1;
}

void read_file_inode(Inode **inode, unsigned file_size) {
    int read_bytes = ((*inode)->fsize < file_size ? (*inode)->fsize : file_size);
    int read_blocks = ((file_size / 4096 + 1) < (*inode)->blocks ? (file_size / 4096 + 1) : (*inode)->blocks);
    for (int i = 0; i < read_blocks; i++) {
        int iteration = (i == read_blocks - 1 ? read_bytes - (read_blocks - 1) * 4096 : 4096);
        for (int j = 0; j < iteration; j++) {
            printf("%c", ((BLK *)(block + DATA_REGION + ((*inode)->pointer)[i]))->segment[j]);
        }
    }
    printf("\n");
}

void remove_file_inode(int inum) {
    int segment_idx = inum / 8;
    int bit_idx = inum % 8;
    unsigned char mark = 0b00000001;
    unsigned char full_mask = 0b11111111;
    mark <<= (7 - bit_idx);
    unsigned char mask = mark ^ full_mask; // mask??? &??????
    // ibmap 0??????
    BLK *ibmap = block + IBMAP;
    ibmap->segment[segment_idx] &= mask;

    // block ?????????
    Inode *inode = (Inode *)(block + IBLOCK) + inum;
    for (int i = 0; i < inode->blocks; i++) {
        BLK *blk = block + DATA_REGION + inode->pointer[i];
        memset(blk, 0, BLK_SIZE);
    }

    // dbmap 0??????
    BLK *dbmap = block + DBMAP;
    for (int i = 0; inode->pointer[i] != 0; i++) {
        mark = 0b00000001;
        segment_idx = inode->pointer[i] / 8;
        bit_idx = inode->pointer[i] % 8;
        mask = (mark << (7 - bit_idx)) ^ full_mask;
        dbmap->segment[segment_idx] &= mask;
    }

    // inode ?????????
    memset(inode, 0, sizeof(Inode));
}