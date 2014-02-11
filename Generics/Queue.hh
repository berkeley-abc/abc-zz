//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Queue.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : A dynamic queue (cyclic buffer).
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__Queue_hh
#define ZZ__Generics__Queue_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


template<class T>
class Queue {
    T*      data;   // Elements (allocation is of size 'cap').
    uind    sz;     // Size (redundant, but makes the case 'head == tail' easier to handle)
    uind    cap;    // Capacity.
    uind    head;   // Pushing at the head (pointing to the next free slot to write to).
    uind    tail;   // Popping from the tail (pointint to the slot to read from).

    void grow();

public:
    Queue() : data(NULL), sz(0), cap(0), head(0), tail(0) {}
   ~Queue() { clear(true); }

    void clear(bool dealloc = false) {
        while (sz > 0) pop();
        if (dealloc){
            yfree(data, cap);
            data = NULL;
            cap = 0; }
        sz = head = tail = 0; }

    void moveTo(Queue& dst) {
        dst = *this;
        new (this) Queue(); }

    void copyTo(Queue& dst) const {
        dst = *this;
        dst.data = ymalloc<T>(cap);
        memcpy(dst.data, data, sizeof(T) * cap); }

    uind size() const {
        return sz; }

    void push(const T& elem) {
        if (sz == cap) grow();
        new (&data[head]) T(elem);
        sz++;
        head++;
        if (head == cap) head = 0; }

    const T& peek() const {
        assert_debug(size() > 0);
        return data[tail]; }

    T& peek() {
        assert_debug(size() > 0);
        return data[tail]; }

    const T& operator[](uind i) const {
        i += tail;
        if (i >= cap) i -= cap;
        assert(i < size());
        return data[i]; }

    T& operator[](uind i) {
        i += tail;
        if (i >= cap) i -= cap;
        assert(i < size());
        return data[i]; }

    void pop() {
        assert_debug(size() > 0);
        data[tail].~T();
        sz--;
        tail++;
        if (tail == cap) tail = 0; }

    T popC() {  // -- pop and copy
        T ret(peek());
        pop();
        return ret; }
};


template<class T>
void Queue<T>::grow()
{
    size_t new_cap = ((cap*5 >> 2) + 2) & ~1;
    T*     new_data = ymalloc<T>(new_cap);
    uind   j = 0;
    for (uind i = tail; i < cap ; i++) new (&new_data[j++]) T(data[i]);
    for (uind i = 0   ; i < tail; i++) new (&new_data[j++]) T(data[i]);
    head = cap;
    tail = 0;

    for (uind i = 0; i < cap; i++) data[i].~T();
    yfree(data, cap);
    data = new_data;
    cap  = new_cap;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
