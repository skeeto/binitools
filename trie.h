#ifndef TRIE_H
#define TRIE_H

/**
 * This trie associates an arbitrary void* pointer with a UTF-8,
 * NUL-terminated C string key. All lookups are O(n), n being the
 * length of the string. Strings are stored sorted, so visitor
 * functions visit keys in lexicographical order. The visitor can also
 * be used to visit keys by a string prefix. An empty prefix ""
 * matches all keys (the prefix argument should never be NULL).
 *
 * Internally it uses flexible array members, which is why C99 is
 * required, and is why the initialization function returns a pointer
 * rather than fills a provided struct.
 *
 * Except for trie_free(), memory is never freed by the trie, even
 * when entries are "removed" by associating a NULL pointer.
 */

#include <stddef.h>

struct trie;

typedef int (*trie_visitor)(const char *key, void *data, void *arg, int);
typedef void *(*trie_replacer)(const char *key, void *current, void *arg);

/**
 * @return a freshly allocated trie, NULL on allocation error
 */
struct trie *trie_create(void);

/**
 * Destroys a trie created by trie_create().
 * @return 0 on success
 */
int trie_free(struct trie *);

/**
 * Finds the data associated with KEY.
 * @return the previously inserted data
 */
void *trie_search(const struct trie *, const char *key);

/**
 * Insert or replace DATA associated with KEY. Inserting NULL is the
 * equivalent of unassociating that key, though no memory will be
 * released.
 * @return 0 on success
 */
int trie_insert(struct trie *, const char *key, void *data);

/**
 * Replace data associated with KEY using a replacer function. The
 * replacer function gets the key, the original data (NULL if none)
 * and ARG. Its return value is inserted into the trie.
 * @return 0 on success
 */
int trie_replace(struct trie *, const char *key, trie_replacer f, void *arg);

/**
 * Visit in lexicographical order each key that matches the prefix. An
 * empty prefix visits every key in the trie. The visitor must accept
 * three arguments: the key, the data, and ARG. Iteration is aborted
 * (with success) if visitor returns non-zero.
 * @return 0 on success
 */
int trie_visit(struct trie *, const char *prefix, trie_visitor v, void *arg);

/* Implementation */

#include <stdlib.h>
#include <string.h>

struct trieptr {
    struct trie *trie;
    int c;
};

struct trie {
    void *data;
    unsigned char nchildren, size;
    struct trieptr children[1];
};

/* Mini stack library for non-recursive traversal. */

struct trie_stack_node {
    struct trie *trie;
    unsigned char i;
};

struct trie_stack {
    struct trie_stack_node *stack;
    size_t fill, size;
};

static int
trie_stack_init(struct trie_stack *s)
{
    s->size = 256;
    s->fill = 0;
    s->stack = malloc(s->size * sizeof(struct trie_stack_node));
    return s->stack ? 0 : -1;
}

static void
trie_stack_free(struct trie_stack *s)
{
    free(s->stack);
    s->stack = 0;
}

static int
trie_stack_grow(struct trie_stack *s)
{
    size_t newsize = s->size * 2 * sizeof(struct trie_stack_node);
    struct trie_stack_node *resize = realloc(s->stack, newsize);
    if (!resize) {
        trie_stack_free(s);
        return -1;
    }
    s->size *= 2;
    s->stack = resize;
    return 0;
}

static int
trie_stack_push(struct trie_stack *s, struct trie *trie)
{
    struct trie_stack_node empty = {0, 0};
    empty.trie = trie;
    if (s->fill == s->size)
        if (trie_stack_grow(s) != 0)
            return -1;
    s->stack[s->fill++] = empty;
    return 0;
}

static struct trie *
trie_stack_pop(struct trie_stack *s)
{
    return s->stack[--s->fill].trie;
}

static struct trie_stack_node *
stack_peek(struct trie_stack *s)
{
    return &s->stack[s->fill - 1];
}

/* Constructor and destructor. */

struct trie *
trie_create(void)
{
    /* Root never needs to be resized. */
    size_t tail_size = sizeof(struct trieptr) * 254;
    struct trie *root = malloc(sizeof(*root) + tail_size);
    if (!root)
        return 0;
    root->size = 255;
    root->nchildren = 0;
    root->data = 0;
    return root;
}

int
trie_free(struct trie *trie)
{
    struct trie_stack stack, *s = &stack;
    if (trie_stack_init(s) != 0)
        return 1;
    trie_stack_push(s, trie); /* first push always successful */
    while (s->fill > 0) {
        struct trie_stack_node *node = stack_peek(s);
        if (node->i < node->trie->nchildren) {
            if (trie_stack_push(s, node->trie->children[node->i].trie) != 0)
                return 1;
            node->i++;
        } else {
            free(trie_stack_pop(s));
        }
    }
    trie_stack_free(s);
    return 0;
}

/* Core search functions. */

static size_t
trie_binary_search(struct trie *self, struct trie **child,
                   struct trieptr **ptr, const char *key)
{
    size_t i = 0;
    int found = 1;
    *ptr = 0;
    while (found && key[i] != '\0') {
        int first = 0;
        int last = self->nchildren - 1;
        int middle;
        found = 0;
        while (first <= last) {
            struct trieptr *p;
            middle = (first + last) / 2;
            p = &self->children[middle];
            if (p->c < key[i]) {
                first = middle + 1;
            } else if (p->c == key[i]) {
                self = p->trie;
                *ptr = p;
                found = 1;
                i++;
                break;
            } else {
                last = middle - 1;
            }
        }
    }
    *child = self;
    return i;
}

void *
trie_search(const struct trie *self, const char *key)
{
    struct trie *child;
    struct trieptr *parent;
    struct trie *wself = (struct trie *)self;
    size_t depth = trie_binary_search(wself, &child, &parent, key);
    return key[depth] == '\0' ? child->data : 0;
}

/* Insertion functions. */

static struct trie *
trie_grow1(struct trie *self) {
    int size;
    struct trie *resized;
    size_t children_size;
    
    size = self->size * 2;
    if (size > 255)
        size = 255;
    children_size = sizeof(struct trieptr) * (size) - 1;
    resized = realloc(self, sizeof(*self) + children_size);
    if (!resized)
        return 0;
    resized->size = (unsigned char)size;
    return resized;
}

static int
trie_ptr_cmp(const void *a, const void *b)
{
    return ((struct trieptr *)a)->c - ((struct trieptr *)b)->c;
}

static struct trie *
trie_add1(struct trie *self, int c, struct trie *child)
{
    int i;
    if (self->size == self->nchildren) {
        self = trie_grow1(self);
        if (!self)
            return 0;
    }
    i = self->nchildren++;
    self->children[i].c = c;
    self->children[i].trie = child;
    qsort(self->children, self->nchildren,
          sizeof(self->children[0]), trie_ptr_cmp);
    return self;
}

static struct trie *
trie_create1(void)
{
    int size = 1;
    size_t children_size = sizeof(struct trieptr) * (size - 1);
    struct trie *trie = malloc(sizeof(*trie) + children_size);
    if (!trie)
        return 0;
    trie->size = (unsigned char)size;
    trie->nchildren = 0;
    trie->data = 0;
    return trie;
}

static void *
trie_identity1(const char *key, void *data, void *arg)
{
    (void) key;
    (void) data;
    return arg;
}

int
trie_replace(struct trie *self, const char *key, trie_replacer f, void *arg)
{
    struct trie *last;
    struct trieptr *parent;
    size_t depth = trie_binary_search(self, &last, &parent, key);
    while (key[depth] != '\0') {
        struct trie *subtrie, *added;
        subtrie = trie_create1();
        if (!subtrie)
            return 1;
        added = trie_add1(last, key[depth], subtrie);
        if (!added) {
            free(subtrie);
            return 1;
        }
        if (parent) {
            parent->trie = added;
            parent = 0;
        }
        last = subtrie;
        depth++;
    }
    last->data = f(key, last->data, arg);
    return 0;
}

int
trie_insert(struct trie *trie, const char *key, void *data)
{
    return trie_replace(trie, key, trie_identity1, data);
}

/* Mini buffer library. */

struct trie_buffer {
    char *buffer;
    size_t size, fill;
};

static int
trie_buffer_init(struct trie_buffer *b, const char *prefix)
{
    b->fill = strlen(prefix);
    b->size = b->fill > 256 ? b->fill * 2 : 256;
    b->buffer = malloc(b->size);
    if (b->buffer)
        strcpy(b->buffer, prefix);
    return b->buffer ? 0 : -1;
}

static void
trie_buffer_free(struct trie_buffer *b)
{
    free(b->buffer);
    b->buffer = 0;
}

static int
trie_buffer_grow(struct trie_buffer *b)
{
    char *resize = realloc(b->buffer, b->size * 2);
    if (!resize) {
        trie_buffer_free(b);
        return -1;
    }
    b->buffer = resize;
    b->size *= 2;
    return 0;
}

static int
trie_buffer_push(struct trie_buffer *b, int c)
{
    if (b->fill + 1 == b->size)
        if (trie_buffer_grow(b) != 0)
            return -1;
    b->buffer[b->fill++] = (char)c;
    b->buffer[b->fill] = '\0';
    return 0;
}

static void
trie_buffer_pop(struct trie_buffer *b)
{
    if (b->fill > 0)
        b->buffer[--b->fill] = '\0';
}

/* Core visitation functions. */

static int
visit(struct trie *self, const char *prefix, trie_visitor visitor, void *arg)
{
    struct trie_buffer buffer, *b = &buffer;
    struct trie_stack stack, *s = &stack;
    if (trie_buffer_init(b, prefix) != 0)
        return -1;
    if (trie_stack_init(s) != 0) {
        trie_buffer_free(b);
        return -1;
    }
    trie_stack_push(s, self);
    while (s->fill > 0) {
        struct trie_stack_node *node = stack_peek(s);
        if (node->i == 0 && node->trie->data) {
            void *data = node->trie->data;
            int nchildren = node->trie->nchildren;
            if (visitor(b->buffer, data, arg, nchildren) != 0) {
                trie_buffer_free(b);
                trie_stack_free(s);
                return 1;
            }
        }
        if (node->i < node->trie->nchildren) {
            if (trie_stack_push(s, node->trie->children[node->i].trie) != 0) {
                trie_buffer_free(b);
                return -1;
            }
            if (trie_buffer_push(b, node->trie->children[node->i].c) != 0) {
                trie_stack_free(s);
                return -1;
            }
            node->i++;
        } else {
            trie_buffer_pop(b);
            trie_stack_pop(s);
        }
    }
    trie_buffer_free(b);
    trie_stack_free(s);
    return 0;
}

int
trie_visit(struct trie *self, const char *prefix, trie_visitor f, void *arg)
{
    int r;
    struct trie *start = self;
    struct trieptr *ptr;
    size_t depth = trie_binary_search(self, &start, &ptr, prefix);
    if (prefix[depth] != '\0')
        return 0;
    r = visit(start, prefix, f, arg);
    return r >= 0 ? 0 : -1;
}

#endif
