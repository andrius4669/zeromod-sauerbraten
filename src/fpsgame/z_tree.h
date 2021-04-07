#ifdef Z_TREE_H
#error "already z_tree.h"
#endif
#define Z_TREE_H

template<typename T> struct z_avltree
{
    struct treenode
    {
        T data;
        treenode *left, *right;
        int height;

        treenode(const T &d): data(d), left(NULL), right(NULL), height(0) {}

        void updateheight()
        {
            if(left && right)
            {
                height = (left->height >= right->height)
                    ? (left->height + 1)
                    : (right->height + 1);
            }
            else if(left) height = left->height + 1;
            else if(right) height = right->height + 1;
            else height = 0;
        }

        int getbalance() const
        {
            if(left && right) return left->height - right->height;
            else if(left) return left->height + 1;
            else if(right) return -(right->height + 1);
            else return 0;
        }
    };

    treenode *root;
    vector<treenode **> stack;

    z_avltree(): root(NULL) {}
    ~z_avltree() { clear(); }

    void rot_left(treenode **p)
    {
        treenode *t = (*p)->right;
        (*p)->right = t->left;
        (*p)->updateheight();
        t->left = *p;
        t->updateheight();
        *p = t;
    }

    void rot_right(treenode **p)
    {
        treenode *t = (*p)->left;
        (*p)->left = t->right;
        (*p)->updateheight();
        t->right = *p;
        t->updateheight();
        *p = t;
    }

    void balance(treenode **p)
    {
        int b = (*p)->getbalance();
        if(b > 1)
        {
            if((*p)->left->getbalance() < 0) rot_left(&(*p)->left);
            rot_right(p);
        }
        else if(b < -1)
        {
            if((*p)->right->getbalance() > 0) rot_right(&(*p)->right);
            rot_left(p);
        }
    }

    template<typename K> T *find(const K &k)
    {
        treenode *p = root;
        while(p)
        {
            int cmp = p->data.compare(k);
            if(cmp < 0) p = p->left;
            else if(cmp > 0) p = p->right;
            else return &p->data;
        }
        return NULL;
    }

    bool add(const T &val, T **result = NULL)
    {
        treenode **p = &root;
        for(;;)
        {
            if(*p != NULL)
            {
                stack.add(p);
                int cmp = (*p)->data.compare(val);
                if(cmp < 0) p = &(*p)->left;
                else if(cmp > 0) p = &(*p)->right;
                else
                {
                    stack.setsize(0);
                    if(result) *result = &(*p)->data;
                    return false;
                }
            }
            else
            {
                *p = new treenode(val);
                if(result) *result = &(*p)->data;
                while(stack.length())
                {
                    p = stack.pop();
                    (*p)->updateheight();
                    balance(p);
                }
                return true;
            }
        }
    }

    // static because pointer to this instance is not needed
    static void clear_rec(treenode *t)
    {
        if(t->left) clear_rec(t->left);
        if(t->right) clear_rec(t->right);
        delete t;
    }

    void clear()
    {
        if(root)
        {
            clear_rec(root);
            root = NULL;
        }
#if 0 // pointless NULL assignments
        treenode **p = &root;
        while(*p)
        {
            if((*p)->left)
            {
                stack.add(p);
                p = &(*p)->left;
            }
            else if((*p)->right)
            {
                stack.add(p);
                p = &(*p)->right;
            }
            else
            {
                delete *p;
                *p = NULL;
                if(stack.length()) p = stack.pop();
            }
        }
#endif
    }
};

#if 0
template<typename T> static void z_printnode(char buf[], int depth, typename z_avltree<T>::treenode *n)
{
    if(n->left) z_printnode<T>(buf, depth+1, n->left);
    ////
    {
        char *p = buf;
        int pad = depth * (4 + 1);
        while(pad--) *p++ = ' ';
        *p++ = '#';
        pad = 4;
        while(pad--) *p++ = '-';
        *p++ = '@';
        p += n->data.print(p);
        *p++ = '|';
        p += sprintf(p, "%d", n->height);
        *p++ = '|';
        p += sprintf(p, "%d", n->getbalance());
        *p = 0;
        conoutf("%s", buf);
    }
    ////
    if(n->right) z_printnode<T>(buf, depth+1, n->right);
}

template<typename T> static void z_display_tree(z_avltree<T> &t)
{
    char line[MAXTRANS];
    conoutf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ start tree ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    if(t.root) z_printnode<T>(line, 0, t.root);
    conoutf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  end tree  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

struct test_s
{
    uint v;
    test_s(uint i): v(i) {}
    inline int compare(const test_s &t) const
    {
        if(v < t.v) return +1;
        if(v > t.v) return -1;
        return 0;
    }
    inline int print(char *p) { return sprintf(p, "%08X", v); }
};
z_avltree<test_s> z_tr_t;

void z_testtree_init()
{
    z_tr_t.clear();
    z_display_tree(z_tr_t);
}
COMMAND(z_testtree_init, "");

void z_testtree_add(uint *v)
{
    test_s t(*v), *e;
    if(z_tr_t.add(t, &e)) conoutf("successfully added 0x%08X", e->v);
    else conoutf("failed to add: 0x%08X conflicts with existing 0x%08X", t.v, e->v);
    z_display_tree(z_tr_t);
}
COMMAND(z_testtree_add, "i");

void z_testtree_rnd()
{
    test_s t(randomMT()), *e;
    if(z_tr_t.add(t, &e)) conoutf("successfully added 0x%08X", e->v);
    else conoutf("failed to add: 0x%08X conflicts with existing 0x%08X", t.v, e->v);
    z_display_tree(z_tr_t);
}
COMMAND(z_testtree_rnd, "");

void z_testtree_donothing()
{
    uint r;
    do { r = randomMT(); } while(r == 0x7FffFFff);
    test_s t(r);
}

void z_testtree_addval()
{
    uint r;
    do { r = randomMT(); } while(r == 0x7FffFFff);
    test_s t(r);
    z_tr_t.add(t);
}

void z_testtree_speed(int *num, int *anum)
{
    uint start, end, dend, aend;
    if(!*anum) conoutf("testing avl tree insert speed for %d values...", *num);
    else conoutf("testing avl tree insert speed for %d values and access speed for %d values", *num, *anum);
    z_tr_t.clear();
    loopi(1024) z_testtree_addval();
    z_tr_t.clear();
    start = enet_time_get();
    loopi(*num) z_testtree_addval();
    end = enet_time_get();
    loopi(*num) z_testtree_donothing();
    dend = enet_time_get();
    loopi(*anum) z_tr_t.find(0x7FffFFff);
    aend = enet_time_get();
    z_tr_t.clear();
    uint total = (end - start) - (dend - end);
    double speed = double(*num) / (total ? total : 1);
    conoutf("test done, results: %u ms (total time) - %u ms (randomMT time) = %u ms, %.3f op/ms",
            end-start, dend-end, total, speed);
    uint atime = aend-dend;
    double aspeed = double(*anum) / (atime ? atime : 1);
    if(*anum) conoutf("access speed: %u ms, %.3f op/ms", atime, aspeed);
}
COMMAND(z_testtree_speed, "ii");
#endif
