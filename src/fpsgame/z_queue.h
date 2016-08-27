#ifdef Z_QUEUE_H
#error "already z_queue.h"
#endif
#define Z_QUEUE_H

// remove(-->) ..FXXXXXXXXXXXXXXXXL.. add(-->) pop(<--)
//               ^ first(-->)     ^ last(<--)
template<typename T> struct z_queue
{
    T *buf;
    int size, len, head, foot;

    z_queue(): buf(NULL), size(0) { clear(); }
    z_queue(int s): size(s) { buf = size ? new T[size] : NULL; clear(); }
    ~z_queue() { delete[] buf; }

    int capacity() const { return size; }
    bool length() const { return len; }
    bool empty() const { return len==0; }
    bool full() const { return len==size; }
    bool inrange(int i) const { return i>=0 && i < len; }
    bool inrange(size_t i) const { return i < size_t(len); }

    void clear()
    {
        len = head = foot = 0;
    }

    void resize(int newsize)
    {
        clear();
        delete[] buf;
        size = newsize;
        buf = size ? new T[size] : NULL;
    }

    T &add()
    {
        T &v = buf[foot];
        foot++; if(foot>=size) foot -= size;
        if(len < size) len++;
        else { head++; if(head>=size) head -= size; }
        return v;
    }
    T &add(const T &v) { return add() = v; }

    T &pop()
    {
        foot--; if(foot<0) foot += size;
        if(len > 0) len--;
        else { head--; if(head<0) head += size; }
        return buf[foot];
    }

    T &remove()
    {
        T &v = buf[head];
        head++; if(head>=size) head -= size;
        if(len > 0) len--;
        else { foot++; if(foot>=size) foot -= size; }
        return v;
    }

    T &first() { return buf[head]; }
    T &first(int offset) { return buf[head+offset<size ? head+offset : head+offset-size]; }
    T &last() { return buf[foot > 0 ? foot-1 : foot-1+size]; }
    T &last(int offset) { return buf[foot-offset > 0 ? foot-offset-1 : foot-offset-1+size]; }

    T &operator[](int i) { return first(i); }
    const T &operator[](int i) const { return first(i); }
};
