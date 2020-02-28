//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : List.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Simple single linked list. 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Lists are pretty useless (except in functional languages with garbage collection), but one use
//| case is to accumulate elements for later later disposal in a global variable. This class 
//| serves that purpose.                                                                                 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__List_hh
#define ZZ__Generics__List_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


template<class T>
struct ListNode {
    T elem;
    ListNode* next;

    ListNode(const T& elem_, ListNode* next_) : elem(elem_), next(next_) {}
};


template<class T>
class List {
    ListNode<T>* first;
    List(ListNode<T>* f) : first(f) {}

public:
    List() : first(NULL) {}
   ~List() { clear(); }

    bool empty() const { return first == NULL; }
    Null_Method(List) { return empty(); }

    void     push(const T& elem) { first = new ListNode<T>(elem, first); }
    const T& peek() const        { return first->elem; }
    T&       peek()              { return first->elem; }
    void     clear()             { while (!empty()) pop(); }

    T pop() {
        T ret = first->elem;
        ListNode<T>* tail = first->next;
        delete first;
        first = tail;
        return ret;
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
