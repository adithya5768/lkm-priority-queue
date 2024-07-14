#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/rbtree.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>

// ioctl
typedef struct obj_info {
    int32_t prio_que_size; 	// current number of elements in priority-queue
    int32_t capacity;		// maximum capacity of priority-queue
} obj_info;

#define PB2_SET_CAPACITY    _IOW(0x10, 0x31, int32_t*)
#define PB2_INSERT_INT      _IOW(0x10, 0x32, int32_t*)
#define PB2_INSERT_PRIO     _IOW(0x10, 0x33, int32_t*)
#define PB2_GET_INFO        _IOR(0x10, 0x34, obj_info*)
#define PB2_GET_MIN         _IOR(0x10, 0x35, int32_t*)
#define PB2_GET_MAX         _IOR(0x10, 0x36, int32_t*)

// utils
#define BUFSIZE 100
#define Array( T, n ) ( T * )kzalloc( n * sizeof(T) , GFP_KERNEL )
#define Swap( T , a, b) { T t = *a; *a = *b; *b = t; }
#define MAX_INT 99999999

// Element which goes into tree
typedef struct pair
{
    int32_t priority;
    int32_t value;
} pair;

////////////////////////////////////////////////////////////////////////////////////
// rbtree

typedef struct rb_root_ext {
    struct rb_root rb_root;         // root node
    struct rb_node* rb_leftmost;    // min node
    struct rb_node* rb_rightmost;   // max node
    int32_t capacity;               // capacity of tree (0 <= capacity <= 100)
    int32_t size;                   // Number of elements in tree
} rb_root_ext;

#define RB_ROOT_EXT(capcacity) (rb_root_ext) { {NULL, }, NULL, NULL, capacity, 0 }
#define rb_min(root) (root)->rb_leftmost
#define rb_max(root) (root)->rb_rightmost

static inline void rb_insert_color_ext(struct rb_node* node,
    rb_root_ext* root, bool leftmost, bool rightmost)
{
    if (leftmost)
    {
        root->rb_leftmost = node;
    }
    if (rightmost)
    {
        root->rb_rightmost = node;
    }
    rb_insert_color(node, &root->rb_root);
}

static inline void rb_erase_ext(struct rb_node* node, rb_root_ext* root)
{
    if (root->rb_leftmost == node)
    {
        root->rb_leftmost = rb_next(node);
    }
    if (root->rb_rightmost == node)
    {
        root->rb_rightmost = rb_prev(node);
    }
    rb_erase(node, &root->rb_root);
    root->size--;
}

typedef struct Node {
    struct rb_node node;
    pair data;
} Node;

Node* rb_create_node(pair data)
{
    Node* node = Array(Node, 1);
    node->data = data;
    return node;
}

rb_root_ext* rb_create_tree(int32_t capacity)
{
    rb_root_ext* root = Array(rb_root_ext, 1);
    *root = RB_ROOT_EXT(capacity);
    return root;
}

bool rb_insert_ext(rb_root_ext* root, Node* data)
{
    bool updateMin = false, updateMax = false;
    struct rb_node** new = &root->rb_root.rb_node, * parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        Node* this = container_of(*new, Node, node);
        int result = data->data.priority - this->data.priority;

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return false;
    }

    // update min and max if needed
    if (!root->rb_leftmost || RB_EMPTY_NODE(root->rb_leftmost) || data->data.priority < container_of(root->rb_leftmost, Node, node)->data.priority)
    {
        updateMin = true;
    }

    if (!root->rb_rightmost || RB_EMPTY_NODE(root->rb_rightmost) || data->data.priority > container_of(root->rb_rightmost, Node, node)->data.priority)
    {
        updateMax = true;
    }

    root->size++;

    /* Add new node and rebalance tree. */
    rb_link_node(&data->node, parent, new);
    rb_insert_color_ext(&data->node, root, updateMin, updateMax);

    return true;
}

void free_rb_tree(rb_root_ext* root)
{
    Node* current_node;

    while (root->rb_rightmost) 
    {
        current_node = rb_entry(root->rb_rightmost, Node, node);
        rb_erase_ext(root->rb_rightmost, root);
        kfree(current_node);
    }

    kfree(root);
}

////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// Process tree

typedef struct proc_tree
{
    int valid;          // Is this entry valid?
    pid_t pid;          // Process' pid	
    rb_root_ext* tree;  // red-black tree with O(1) access for min and max
    int writes;         // Number of writes
    int inserting;      // Is currently inserting?
    int32_t value;      // If inserting, what value?
} proc_tree;

static proc_tree pt[1000];

static spinlock_t pt_lock;	// Need to lock this to find a new index for a process in pt

int current_process_index(void)
{
    int i;
    for (i = 0; i < 1000; i++)
    {
        if (pt[i].valid == 1 && pt[i].pid == current->pid)
        {
            return i;
        }
    }

    return -1;
}

void free_proc_tree(int index)
{
    pt[index].valid = 0;
    pt[index].writes = 0;
    pt[index].inserting = 0;
    if (pt[index].tree)
        free_rb_tree(pt[index].tree);
    pt[index].tree = NULL;
}

static void init_proc_trees(void)
{
    int i;

    // initialize proc_tree
    for (i = 0; i < 1000; i++)
    {
        pt[i].valid = 0;
        pt[i].writes = 0;
        pt[i].inserting = 0;
        pt[i].tree = NULL;
    }

    // initialize spin lock
    spin_lock_init(&pt_lock);
}
////////////////////////////////////////////////////////////////////////////////////


// Make sure we only accept integer input
int getInt(int* n, const char* buf)
{
    char c;

    if (!buf)
    {
        return 0;
    }

    *n = 0;

    do {
        c = *buf++;

        if (c >= 48 && c <= 57)
        {
            *n = *n * 10 + (c - 48);
        }
        else if (c == '\n' || c == ' ' || c == '\t' || c == '\0')
        {
            break;
        }
        else
        {
            *n = 0;
            return 0;
        }

    } while (1);

    return 1;
}


// Buffer to read what process has written
char buffer[BUFSIZE];

static struct proc_dir_entry* ent;

static int myshow(struct seq_file* m, void* v)
{
    seq_printf(m, "Hello proc!\n");
    return 0;
}

static int myopen(struct inode* in, struct file* f)
{
    int i, index;
    unsigned long flags;

    // Make sure only one process executes the critical code at one time
    spin_lock_irqsave(&pt_lock, flags);

    // Check if file is already opened by this process
    index = current_process_index();

    if (index != -1)
    {
        printk(KERN_ERR "open handler: Process with PID = %d already has the file opened\n", (int)(current->pid));
        spin_unlock_irqrestore(&pt_lock, flags);
        return -EINVAL;
    }

    // find index to store new process' tree
    for (i = 0; i < 1000; i++)
    {
        if (!pt[i].valid)
        {
            index = i;
            break;
        }
    }

    // index not found?
    if (index == -1)
    {
        printk(KERN_ERR "proc_tree (max size = 1000) is full\n");
        spin_unlock_irqrestore(&pt_lock, flags);
        return -1;
    }
    else
    {
        pt[index].valid = 1;
        pt[index].pid = current->pid;
        pt[index].writes = 0;
        pt[index].inserting = 0;
        pt[index].tree = NULL;
    }

    spin_unlock_irqrestore(&pt_lock, flags);

    return single_open(f, myshow, NULL);
}


// Free resources and call release
static int myrelease(struct inode* in, struct file* f)
{
    int index, ret;
    unsigned long flags;
    printk( KERN_DEBUG "release handler: PID = %d\n", (int)(current->pid));

    // Make sure only one process executes the critical code at one time
    spin_lock_irqsave(&pt_lock, flags);

    index = current_process_index();

    if (index == -1)
    {
        printk(KERN_WARNING "release handler: Process with PID = %d has opened file but no index found in proc_trees\n", (int)(current->pid));
    }
    else
    {
        free_proc_tree(index);
    }

    ret = single_release(in, f);

    spin_unlock_irqrestore(&pt_lock, flags);

    printk(KERN_ALERT "myrelease: returning\n");

    return ret;
}

void set_capacity(int index, int32_t capacity)
{

    pt[index].writes = 0;
    pt[index].inserting = 0;
    if (pt[index].tree != NULL)
    {
        free_rb_tree(pt[index].tree);
        pt[index].tree = NULL;

        printk(KERN_WARNING "ioctl handler: Process with PID = %d reset\n", (int)current->pid);
    }

    pt[index].tree = rb_create_tree(capacity);
}

int32_t get_max(int index)
{
    int ret;
    Node* node = container_of(rb_max(pt[index].tree), Node, node);
    ret = node->data.value;
    rb_erase_ext(rb_max(pt[index].tree), pt[index].tree);
    kfree(node);
    return ret;
}
int32_t get_min(int index)
{
    int ret;
    Node* node = container_of(rb_min(pt[index].tree), Node, node);
    ret = node->data.value;
    rb_erase_ext(rb_min(pt[index].tree), pt[index].tree);
    kfree(node);
    return ret;
}

static long my_ioctl(struct file* f, unsigned int cmd, unsigned long arg)
{
    int32_t q, index;
    obj_info to_ret;
    Node* node = NULL;
    index = current_process_index();



    if (index == -1)
    {
        // ERROR: Should not have happened;
        printk(KERN_ERR "ioctl handler: Process with PID = %d opened the file but its index is not found in proc_trees\n", (int)current->pid);
        return -1;
    }

    switch (cmd)
    {
    case PB2_SET_CAPACITY:
        if (copy_from_user(&q, (int32_t*)arg, sizeof(int32_t)))
        {
            return -EACCES;
        }
        set_capacity(index, q);
        break;

    case PB2_INSERT_INT:
        if (pt[index].tree == NULL)
        {
            printk(KERN_WARNING "ioctl handler: Process with PID = %d does not have its tree initialized\n", (int)current->pid);
            return -EINVAL;
        }
        if (pt[index].tree->size == pt[index].tree->capacity)
        {
            printk(KERN_DEBUG "ioctl handler: Process with PID = %d already reached capacity\n", (int)current->pid);
            return -EACCES;
        }
        if (pt[index].inserting == 1)
        {
            printk(KERN_ERR "ioctl handler: Process with PID = %d is expecting PB2_INSERT_PRIO but PB2_INSERT_INT called\n", (int)current->pid);
            return -EINVAL;
        }
        if (copy_from_user(&q, (int32_t*)arg, sizeof(int32_t)))
        {
            return -EACCES;
        }
        pt[index].inserting = 1;
        pt[index].value = q;
        break;

    case PB2_INSERT_PRIO:
        if (pt[index].tree == NULL)
        {
            printk(KERN_WARNING "ioctl handler: Process with PID = %d does not have its tree initialized\n", (int)current->pid);
            return -EINVAL;
        }
        if (pt[index].inserting == 0)
        {
            printk(KERN_ERR "ioctl handler: Process with PID = %d is expecting PB2_INSERT_INT but PB2_INSERT_PRIO called\n", (int)current->pid);
            return -EINVAL;
        }
        if (copy_from_user(&q, (int32_t*)arg, sizeof(int32_t)))
        {
            return -EACCES;
        }
        node = rb_create_node((pair) { /* priority */ q, /* value */ pt[index].value });
        rb_insert_ext(pt[index].tree, node);
        pt[index].inserting = 0;
        break;

    case PB2_GET_INFO:
        if (pt[index].tree == NULL)
        {
            printk(KERN_WARNING "ioctl handler: Process with PID = %d does not have its tree initialized\n", (int)current->pid);
            return -EINVAL;
        }
        to_ret.prio_que_size = pt[index].tree->size;
        to_ret.capacity = pt[index].tree->capacity;
        if (copy_to_user((obj_info*)arg, &to_ret, sizeof(to_ret)))
        {
            return -EACCES;
        }
        printk(KERN_DEBUG "ioctl size: %d, capacity: %d, PID = %d\n", ((obj_info*)arg)->prio_que_size, ((obj_info*)arg)->capacity, (int)current->pid);
        break;

    case PB2_GET_MIN:
        if (pt[index].tree == NULL)
        {
            printk(KERN_WARNING "ioctl handler: Process with PID = %d does not have its tree initialized\n", (int)current->pid);
            return -EINVAL;
        }
        if (rb_min(pt[index].tree) == NULL || RB_EMPTY_NODE(rb_min(pt[index].tree)) || pt[index].tree->size == 0)
        {
            printk(KERN_DEBUG "ioctl handler: Process with PID = %d has empty priority queue\n", (int)current->pid);
            return -EACCES;
        }
        
        q = get_min(index);

        if (copy_to_user((int32_t*)arg, &q, sizeof(int32_t)))
        {
            return -EACCES;
        }
        printk(KERN_DEBUG "ioctl GET_MIN: %d, PID = %d\n", *(int32_t*)arg, (int)current->pid);
        break;

    case PB2_GET_MAX:
        if (pt[index].tree == NULL)
        {
            printk(KERN_WARNING "ioctl handler: Process with PID = %d does not have its tree initialized\n", (int)current->pid);
            return -EINVAL;
        }
        if (rb_max(pt[index].tree) == NULL || RB_EMPTY_NODE(rb_max(pt[index].tree)) || pt[index].tree->size == 0)
        {
            printk(KERN_DEBUG "ioctl handler: Process with PID = %d has empty priority queue\n", (int)current->pid);
            return -EACCES;
        }

        q = get_max(index);

        if (copy_to_user((int32_t*)arg, &q, sizeof(int32_t)))
        {
            return -EACCES;
        }
        printk(KERN_DEBUG "ioctl GET_MAX: %d, PID = %d\n", *(int32_t*)arg, (int)current->pid);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

static struct proc_ops myops =
{
    .proc_open = myopen,
    .proc_release = myrelease,
    .proc_ioctl = my_ioctl
};

static int proc_module_begin(void)
{
    printk(KERN_ALERT "proc_tree Module Initiated\n");
    ent = proc_create("partb_2_8", 0660, NULL, &myops);
    init_proc_trees();
    return 0;
}

static void proc_module_exit(void)
{
    printk(KERN_ALERT "proc_tree Module exit\n");
    proc_remove(ent);
}

MODULE_LICENSE("GPL");
module_init(proc_module_begin);
module_exit(proc_module_exit);
