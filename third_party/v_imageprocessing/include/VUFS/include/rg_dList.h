//********************************************************************
//
//    FILENAME    : rg_dList.h
//    
//    DESCRIPTION : 
//           This consists of the definition and interface
//                                 of class rg_dList using template.  
//
//    BASE CLASS  : None  
//
//    AUTHOR      : Tae-Bum Jang
//    START DATE  : 1997.  7. 10    
//
//            Copyright (c) CAD/CAM Lab.            
//
//    History     :     
//                  1. Set following function's  return type rg_FLAG
//                                  In  1997. 8.20 By Taebum Jang
//                       Old;                                 
//                     rg_INT isInList(const T etemp) const;
//                     rg_INT isInList(const rg_dNode<T> *thisNode);
//                     rg_INT isHead() const;
//                     rg_INT isTail() const; 
//
//                       New;
//                     rg_FLAG isInList(const T etemp) const;
//                     rg_FLAG isInList(const rg_dNode<T> *thisNode);
//                     rg_FLAG isHead() const;
//                     rg_FLAG isTail() const;
//                                     
//                  2. Add following functions  
//                                 In  1997. 8.21 By TaeBum Jang
//                     void reset4Loop();
//                     rg_FLAG setNext4Loop();
//                     rg_FLAG isEmpty() const; 
//
//                  3. Add following functions
//                                 In  1998. 3.16 By TaeBum Jang
//                     T*   getArray() const;                     
//
//                  4. Add the following functions
//                                 In  1998. 3. 30 By TaeBum Jang
//                     void killCurrent();
//
//                  5. Add the following functions 
//                     
//                     void killHead();
//                     void killTail();
//
//                  6. Add the following functions
//                     T** get2DArray() const;
//
//                  7. Add the following functions
//                      void replaceCurrent(const T& thisEntity);
//                                  added by Hyung-Joo Lee in 1998. 6.28
//
//                  8. T* getArrayKillList();
//                      return pointer of array store all data in this list
//                      and kill the original list.
//                                  added by Hyung-Joo Lee in 1998. 6.22
//					9. T* getpNextEntity()
//                     T* getLastpEntity()
//						by joongHyun Ryu 2000.7.3
//                 10. T* rg_dList<T>::getFirstpEntity()const
//                                  added by Youngsong Cho 2001. 11. 20
//
//				   11. Add Swap function between two nodes using index
//  				   void swapNode(const rg_INDEX& index1, const rg_INDEX& index2);
//									added by Cheol-Hyung Cho 2003. 8. 11
//
//*********************************************************************

#ifndef _RG_DLIST_H
#define _RG_DLIST_H

#include <stdlib.h>
#include <new>
#include "rg_Const.h"
#include "rg_BlockArena.h"
//  Node for Double linked list class using template
template <class T>
class rg_dList;

template <class T>
class rg_dNode
{
    friend class rg_dList<T>;
private:
    rg_dNode<T> *prev;
    rg_dNode<T> *next;
    T entity;
public:
//// constructor & destructor  /////////////////////////////
    rg_dNode();
    rg_dNode(const T& input,rg_dNode<T> *p,rg_dNode<T> *n);
    rg_dNode(const rg_dNode<T> &d);
//  ~rg_dNode();

//// get functions   ///////////////////////////////////////
    rg_dNode<T>& getNextNode();
    rg_dNode<T>& getPrevNode();
    rg_dNode<T>* getNext();// get pointer            ////
    rg_dNode<T>* getPrev();//       of related Node   ////
    T getEntity() const;
    T* getpEntity();
    T getNextEntity() const;
    T getPrevEntity() const;

//// set functions  ////////////////////////////////////////
    void setNext(rg_dNode<T> *thisNode);
    void setPrev(rg_dNode<T> *thisNode);
    void setEntity(const T& input);
};

//  Double linked list class using template 
template<class T>
class rg_dList
{
protected:
    rg_dNode<T> *head;
    rg_dNode<T> *tail;
    // revised by Y.Cho (2010.4.3)
    mutable rg_dNode<T> *current;
    //rg_dNode<T> *current;

    rg_INDEX size;

    // ---------------------------------------------------------------
    // Optional node allocator (perf/2a-lite, 2026-10-29).
    //
    // Default is rg_NULL, which preserves the ORIGINAL behavior of this
    // class exactly: every node comes from plain `new rg_dNode<T>()` and
    // goes back via plain `delete`.
    //
    // If setArena() is called with a non-null rg_BlockArena*, add()-family
    // calls place nodes into arena-provided memory instead (bump pointer,
    // no per-node malloc), and removeAll()/kill()-family calls run the
    // node's destructor in place WITHOUT returning its memory to the
    // allocator -- the arena reclaims everything in one shot when its
    // owner calls rg_BlockArena::reset() (or when the arena itself is
    // destroyed). This trades per-node free for bulk free, which is only
    // safe for lists whose entire memory is torn down together (i.e. no
    // other, still-alive node needs that memory back in between).
    //
    // setArena() MUST be called immediately after construction, before
    // the first add()/addHead()/addTail()/... call -- switching
    // allocators on a list that already owns nodes is unsupported.
    // -----------------------------------------------------------------
    rg_BlockArena* m_arena;

    rg_dNode<T>* allocateNode();
    void         deallocateNode(rg_dNode<T>* node);

public:

//// constructor & destructor  /////////////////////////////

    rg_dList();
    rg_dList(const T& etemp);
    rg_dList(const rg_dList<T>& thisList);
    virtual ~rg_dList();

    // See m_arena above for the usage contract.
    void setArena(rg_BlockArena* arena) { m_arena = arena; }


//// functions to add or delete entities //////////////////
  
    T* add(const T& etemp);
    T* addAt(const rg_INDEX& index,
             const T& eTemp);
    T* addTail(const T& etemp);
    T* addHead(const T& etemp);
    T* addWithoutSame(const T& etemp);

    void append(const rg_dList<T>& temp);
    void appendHead(const rg_dList<T>& temp);
    void appendTail(const rg_dList<T>& temp);

    void mergeHead( rg_dList<T>& temp);
    void mergeTail( rg_dList<T>& temp);

    void insertAfter(const T& etemp,rg_dNode<T>* preNode); // insert entity after preNode
	T*   insertAfterNode(const T& etemp,rg_dNode<T>* preNode); // insert entity after preNode // JHRYU    
    void insertAfter(const T& etemp); // insert entity after current Node

    void insertBefore(const T& etemp,rg_dNode<T>* preNode); // insert entity before preNode
    void insertBefore(const T& etemp);// insert entity before current Node

    void kill(const T& etemp);
    void kill(T* pEntity);
    void kill(rg_dNode<T>* thisNode);
    void killCurrent();
    void killHead();
    void killTail();


    virtual void removeAll();
    void removeAt(const rg_INDEX& index);
    
    void duplicateList(const rg_dList<T>& thisList);
    /////////////////////
    void setpNode(const rg_INDEX& index, const T& input);
    /////////////////////////////////
	void swapNode(const rg_INDEX& index1, const rg_INDEX& index2);  // added by Cheol-Hyung Cho in 2005.8.11
  
//// set functions ///////////////////////////////////////
  
    void setHead(const rg_dNode<T>* thisNode);
    void setHead(const T& thisEntity);
    void setTail(const rg_dNode<T>* thisNode);
    void setTail(const T& thisEntity);
// following set functions related to current 
    void setCurrent(rg_dNode<T>* thisNode);
    void setCurrent(const T& thisNode);
    void setCurrentHead(); // added by Hyung-Joo Lee in 1998. 7.20
    void setCurrentTail(); // added by Hyung-Joo Lee in 1998. 7.9
    void setCurrentNext();
    void setCurrentPrev();
    void reset();
//    void resetSize();
    // revised by Y.Cho (2010.4.3)
    void reset4Loop() const;// added by TaeBum Jang in 1997. 8.21
    rg_FLAG setNext4Loop() const;// added by TaeBum Jang in 1997. 8.21
    //void reset4Loop();// added by TaeBum Jang in 1997. 8.21
    //rg_FLAG setNext4Loop();// added by TaeBum Jang in 1997. 8.21
    void replaceCurrent(const T& thisEntity); // added by Hyung-Joo Lee in 1998. 6.28
    void replace(const T& oldEntity, const T& newEntity); // by Y.Cho in 2012.06.09

////pop & push functions /////////////////////////////////////////
//           by Youngsong Cho (2004. 3. 29)
    T popFront();
    T popBack();
    T* pushFront(const T& etemp);
    T* pushBack(const T& etemp);

    
////get functions /////////////////////////////////////////
    rg_INDEX getSize() const;
    rg_dNode<T>* getHead() const;
    rg_dNode<T>* getTail() const;

    T getNextEntity() const;
    T getPrevEntity() const;
    T getNextEntity(const rg_dNode<T> *currNode) const;
    T getPrevEntity(const rg_dNode<T> *currNode) const;

    T  getFirstEntity()const;
	T* getFirstpEntity()const;
    T  getSecondEntity()const;
    T  getLastEntity()const;
    T* getLastpEntity()const;
    T  getAt(const rg_INDEX& index) const;
    T  getEntity() const;
    T* getpEntity() const;
	T* getpNextEntity();
    T* getArray() const;// added by TaeBum Jang in 1998. 3.16
                        // return pointer of array store all data in this list
    T* getArrayKillList();// added by Hyung-Joo Lee in 1998. 6.22
                        // return pointer of array store all data in this list
                        // and kill the original list.

    rg_dNode<T>* getCurrentpNode(); // added by jhryu in 2007. 5. 28
    rg_dNode<T>* getNextpNode();
    rg_dNode<T>* getNextpNode(const rg_dNode<T> *currNode);
    rg_dNode<T>* getPrevpNode();
    rg_dNode<T>* getPrevpNode(const rg_dNode<T> *currNode);   
    rg_dNode<T>* getFirstpNode();
    rg_dNode<T>* getSecondpNode();
    rg_dNode<T>* getLastpNode();
    rg_dNode<T>* getpNode(const rg_INDEX& index);

//// Function to answer FAQ ///////////////////////////////////
    rg_FLAG isInList(const T& etemp) const;
    rg_FLAG isInList(const rg_dNode<T> *thisNode) const;
    rg_dNode<T>* whereInList(const T&     etemp,
                             const rg_INDEX& thisth=1) const;
    rg_dNode<T>* whereInList(      T*     pEntity,
                             const rg_INDEX& thisth=1) const;


    rg_FLAG isEmpty() const;//added by TaeBum Jang in 1997.8.21
// following ETC functios related to current
    rg_FLAG isHead() const; 
    rg_FLAG isTail() const;
    
//// overloading operator  ///////////////////////////////////
    T& operator[](const rg_INDEX& index);
    rg_dList<T>& operator=(const rg_dList<T>& temp); 
    T& elementAt(const rg_INDEX& index) const;
};

///////////////////////////////////////////////////////////////////////////////////
/// Double linked list class using template define ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

//  Node for Double linked list class using template

template <class T>
rg_dNode<T>::rg_dNode()
{
    prev=rg_NULL;
    next=rg_NULL;
}

template<class T>
rg_dNode<T>::rg_dNode(const T& input,rg_dNode<T> *p,rg_dNode<T> *n)
{
    entity=input;
    prev=p;
    next=n;
}

template<class T>
rg_dNode<T>::rg_dNode(const rg_dNode<T> &node)
{
    entity=node.entity;
    prev=node.prev;
    next=node.next;
}

// get functions
template<class T>
rg_dNode<T>& rg_dNode<T>::getNextNode()
{
    if ( next != rg_NULL)
    {
        return *(next);
    }
    return rg_NULL;
}

template<class T>
rg_dNode<T>& rg_dNode<T>::getPrevNode()
{
    if ( prev != rg_NULL)
    {
        return *(prev);
    }
    return rg_NULL;
}

template<class T>
rg_dNode<T>* rg_dNode<T>::getNext()
{
    if ( next != rg_NULL)
    {
        return next;
    }
    return rg_NULL;
}

template<class T>
rg_dNode<T>* rg_dNode<T>::getPrev()
{
    if ( prev != rg_NULL)
    {
        return prev;
    }
    return rg_NULL;
}

template<class T>
T rg_dNode<T>::getEntity() const
{
    return entity;
}

template<class T>
T* rg_dNode<T>::getpEntity() 
{
    return &entity;
}

template<class T>
T rg_dNode<T>::getNextEntity() const
{
    if ( next != rg_NULL )
    {
        return next->entity;
    }
    exit(0);
}

template<class T>
T rg_dNode<T>::getPrevEntity() const
{
    if ( prev != rg_NULL )
    {
        return prev->entity;
    }
    exit(0);
}


// set functions
template<class T>
void rg_dNode<T>::setNext(rg_dNode<T> *thisNode)
{
    next=thisNode;
}

template<class T>
void rg_dNode<T>::setPrev(rg_dNode<T> *thisNode)
{
    prev=thisNode;
}

template<class T>
void rg_dNode<T>::setEntity(const T& input)
{
    entity = input;
}


/////////////////////////////////////////////////////////////////
//  implementation of Double linked list class using template  //
/////////////////////////////////////////////////////////////////

// perf/2a-lite (2026-10-29): node allocation/deallocation funnel through
// these two helpers so every add()-family / kill()-family function gets
// arena support for free, while the m_arena==rg_NULL (default) path is
// byte-for-byte the original `new rg_dNode<T>()` / `delete node;`.
template<class T>
rg_dNode<T>* rg_dList<T>::allocateNode()
{
    if ( m_arena != rg_NULL ) {
        void* mem = m_arena->allocate(sizeof(rg_dNode<T>));
        return new (mem) rg_dNode<T>();
    }
    return new rg_dNode<T>();
}

template<class T>
void rg_dList<T>::deallocateNode(rg_dNode<T>* node)
{
    if ( m_arena != rg_NULL ) {
        // Memory is owned by the arena and reclaimed in bulk later --
        // only run the destructor (entity teardown) here.
        node->~rg_dNode<T>();
    }
    else {
        delete node;
    }
}

template<class T>
void rg_dList<T>::removeAll()
{
    rg_dNode<T>* currentNode=head;
    rg_dNode<T>* nextNode=rg_NULL;
    while( size > 0 )
    {
        nextNode=currentNode->getNext();
        deallocateNode(currentNode);
        currentNode=nextNode;
        size--;
    }
    head=rg_NULL;
    tail=rg_NULL;
    size=0;
    current=rg_NULL;
}


// constructor & destructor
template<class T>
rg_dList<T>::rg_dList()
{
    head=rg_NULL;
    tail=rg_NULL;
    size=0;
    current=rg_NULL;
    m_arena=rg_NULL;
}

template<class T>
rg_dList<T>::rg_dList(const T& etemp)
{
    m_arena=rg_NULL;
    head=new rg_dNode<T>();
    head->entity=etemp;
    head->prev=head;
    head->next=head;
    tail=head;
    current=head;
    size=1;
}

template<class T>
rg_dList<T>::rg_dList(const rg_dList<T>& thisList)
{
    head=rg_NULL;
    tail=rg_NULL;
    size=0;
    current=rg_NULL;
    m_arena=rg_NULL;
    duplicateList(thisList);
}

template<class T>
rg_dList<T>::~rg_dList()
{
    removeAll();
}

// functions to add or delete entities
template<class T>
T* rg_dList<T>::add(const T& etemp)
{
    return addTail(etemp);
}

template<class T>
T* rg_dList<T>::addTail(const T& etemp)
{
    rg_dNode<T> *newNode=rg_NULL;
    if ( size == 0 )
    {
        newNode=allocateNode();
        newNode->next=newNode;
        newNode->prev=newNode;
        newNode->entity=etemp;
        head=newNode;
        tail=head;
        current=head;
    }
    else
    {
        rg_dNode<T> *preNode=tail;
        rg_dNode<T> *nextNode=head;

        newNode=allocateNode();
        newNode->prev=preNode;
        newNode->next=nextNode;
        newNode->entity=etemp;

        preNode->next=newNode;
        nextNode->prev=newNode;
        tail=newNode;// Without this line ,
                     // all line is same as addHead()
    }
    size++;
    return &(newNode->entity);
}

template<class T>
T* rg_dList<T>::addHead(const T& etemp)
{
    rg_dNode<T>* newNode=rg_NULL;
    if ( size == 0 )
    {
        newNode=allocateNode();
        newNode->prev=newNode;
        newNode->next=newNode;
        head=newNode;
        head->entity=etemp;
        tail=head;
        current=head;
    }
    else
    {
        rg_dNode<T> *preNode=tail;
        rg_dNode<T> *nextNode=head;

        newNode=allocateNode();
        newNode->prev=preNode;
        newNode->next=nextNode;
        newNode->entity=etemp;

        preNode->next=newNode;
        nextNode->prev=newNode;
        head=newNode;// Without this line
                     // all line is same as addTail()
    }
    size++;
    return &(newNode->entity);
}

template<class T>
T* rg_dList<T>::addAt(const rg_INDEX& index,
                   const T& eTemp)
{
    if ( index < 0 || index >= size )
    {
        return 0;
    }

    if ( index == 0 )
    {
        return addHead(eTemp);
    }
//    else if ( index == size-1 )
//    {
//        return addTail(eTemp);
//    }
    
    // search for   pointer of index_th node  (nextNode)
    //          and pointer of index-1_th node(preNode)
    rg_dNode<T> *nextNode=head;
    rg_dNode<T> *preNode=rg_NULL;
    for( rg_INT i=0; i < (rg_INT)index; i++)
    {
        nextNode=nextNode->next;
    }
    preNode=nextNode->prev;

    //     create Node having etemp
    // and link pre Node and next Node
    rg_dNode<T> *newNode=allocateNode();
    newNode->prev=preNode;
    newNode->next=nextNode;
    newNode->entity=eTemp;

    // Update node's linkage related new node
    preNode->next=newNode;
    nextNode->prev=newNode;
    size++;
    
    return &(newNode->entity);
}



template<class T>
T* rg_dList<T>::addWithoutSame(const T& etemp)
{
    if ( isInList(etemp) == rg_FALSE )
    {
        return add(etemp);      
    }
    else
    {
        return rg_NULL;
    }

}

template<class T>
void rg_dList<T>::append(const rg_dList<T>& temp)
{
    appendTail(temp);
}

template<class T>
void rg_dList<T>::appendHead(const rg_dList<T>& temp)
{
    rg_dNode<T>* thisNode=temp.tail;

    for( rg_INDEX i=0; i < temp.size; i++ )
    {
        addHead( thisNode->entity );
        thisNode=thisNode->prev;
    }
}

template<class T>
void rg_dList<T>::appendTail(const rg_dList<T>& temp)
{
    rg_dNode<T>* thisNode=temp.head;
    for( rg_INDEX i=0; i < temp.size; i++ )
    {
        addTail( thisNode->entity );
        thisNode=thisNode->next;
    }
}

/////////////////////////////////////////////////////////////
//// illustration of Variable defined in merge           ////
////  <front list>         <rear list>                   ////
////  *-*-*-*-*-*-*-* <--> *-*-*-*-*-*-*-*-*-*-*         ////
////  head --> newHead        head --> rearStart         ////   
////  tail --> frontEnd       tail --> newTail           ////
/////////////////////////////////////////////////////////////
template<class T>
void rg_dList<T>::mergeTail( rg_dList<T>& temp )
{
    // this is front list and temp is rear list.
    if ( temp.size == 0 )
    {
        return;
    }

    if ( size == 0 )
    {
        head=temp.head;
        tail=temp.tail;
        current=temp.current;
        size=temp.size;
        
        temp.head=rg_NULL;
        temp.tail=rg_NULL;
        temp.current=rg_NULL;
        temp.size=0;

        return;
    }

    rg_dNode<T>* newHead=head;
    rg_dNode<T>* newTail=temp.tail;
    rg_dNode<T>* frontEnd=tail;
    rg_dNode<T>* rearStart=temp.head;
    
    // rg_Link3D between two list
    newHead->prev=newTail;
    newTail->next=newHead;
    frontEnd->next=rearStart;
    rearStart->prev=frontEnd;

    // Update this list
    size=size+temp.size;
    head=newHead;
    tail=newTail;

    // Update temp list
    temp.head=rg_NULL;
    temp.tail=rg_NULL;
    temp.current=rg_NULL;
    temp.size=0;
}

template<class T>
void rg_dList<T>::mergeHead( rg_dList<T>& temp )
{
    if ( temp.size == 0 )
    {
        return;
    }

    if ( size == 0 )
    {
        head=temp.head;
        tail=temp.tail;
        current=temp.current;
        size=temp.size;

        temp.head=rg_NULL;
        temp.tail=rg_NULL;
        temp.current=rg_NULL;
        temp.size=0;

        return;
    }

    // this is rear list and temp is front list
    rg_dNode<T>* newHead=temp.head;
    rg_dNode<T>* newTail=tail;
    rg_dNode<T>* frontEnd=temp.tail;
    rg_dNode<T>* rearStart=head;
    
    // rg_Link3D between two list
    newHead->prev=newTail;
    newTail->next=newHead;
    frontEnd->next=rearStart;
    rearStart->prev=frontEnd;

    // Update this list
    size=size+temp.size;
    head=newHead;
    tail=newTail;

    // Update temp
    temp.head=rg_NULL;
    temp.tail=rg_NULL;
    temp.current=rg_NULL;
    temp.size=0;
}

template<class T>
void rg_dList<T>::insertAfter(const T& etemp,rg_dNode<T>* preNode)// insertAfter entity after preNode
{
    if ( preNode == rg_NULL )
    {
        addHead(etemp);
    }
    else if( isInList(preNode) == rg_TRUE  )
    {
        rg_dNode<T> *nextNode=preNode->next;

        rg_dNode<T> *inputNode=allocateNode();
        inputNode->entity=etemp;
        inputNode->prev=preNode;
        inputNode->next=nextNode;

        preNode->next=inputNode;
        nextNode->prev=inputNode;
        size++;

        if ( preNode == tail )
            tail = inputNode;
    }
}

template<class T>
T* rg_dList<T>::insertAfterNode(const T& etemp,rg_dNode<T>* preNode)// insertAfter entity after preNode
{
	if ( preNode == rg_NULL )
	{
		return addHead(etemp);
	}
	else if( isInList(preNode) == rg_TRUE  )
	{
		rg_dNode<T> *nextNode=preNode->next;

		rg_dNode<T> *inputNode=allocateNode();
		inputNode->entity=etemp;
		inputNode->prev=preNode;
		inputNode->next=nextNode;

		preNode->next=inputNode;
		nextNode->prev=inputNode;
		size++;

		if ( preNode == tail )
			tail = inputNode;

		return &(inputNode->entity);
	}
}

template<class T>
void rg_dList<T>::insertAfter(const T& etemp)// insert entity before current Node when no pointer indicated
{
    insertAfter(etemp,current);
}

template<class T>
void rg_dList<T>::insertBefore(const T& etemp,rg_dNode<T>* nextNode)// insert entity Before nextNode
{
    if ( nextNode == rg_NULL )
    {
        addTail(etemp);
    }
    else if ( nextNode == head )
    {
        addHead(etemp);
    }
    else if ( isInList(nextNode) == rg_TRUE  )
    {
        rg_dNode<T> *preNode=nextNode->prev;

        rg_dNode<T> *inputNode=allocateNode();
        inputNode->entity=etemp;
        inputNode->prev=preNode;
        inputNode->next=nextNode;

        preNode->next=inputNode;
        nextNode->prev=inputNode;
        size++;
    }

}

template<class T>
void rg_dList<T>::insertBefore(const T& etemp)// insert entity before current Node when no pointer indicated
{
    insertBefore(etemp,current);
}    

template<class T>
void rg_dList<T>::kill(const T& etemp)
{
    if ( head == rg_NULL ) // for the case of size 0
    {
        return;
    }

    rg_dNode<T> *thisNode=whereInList(etemp);

    kill(thisNode);

    /*
    if ( thisNode == rg_NULL )
    {
        return;
    }

    if ( size == 1)// for the case of size 1 
    {
        delete thisNode;
        size=0;
        head=rg_NULL;
        tail=rg_NULL;
        current=rg_NULL;

        return;
    }

    if ( thisNode == head )
    {
        head=head->next;
    }
    else if( thisNode == tail )
    {
        tail=tail->prev;
    }

    rg_dNode<T> *preNode=thisNode->prev;
    rg_dNode<T> *nextNode=thisNode->next;

    preNode->next=nextNode;
    nextNode->prev=preNode;

    delete thisNode;
    size--;
    */
}

template<class T>
void rg_dList<T>::kill(T* pEntity)
{
    if ( head == rg_NULL ) // for the case of size 0
    {
        return;
    }

    rg_dNode<T> *thisNode=whereInList(pEntity);

    kill(thisNode);

    /*
    if ( thisNode == rg_NULL )
    {
        return;
    }

    if ( size == 1)// for the case of size 1 
    {
        delete thisNode;
        size=0;
        head=rg_NULL;
        tail=rg_NULL;
        current=rg_NULL;

        return;
    }

    if ( thisNode == head )
    {
        head=head->next;
    }
    else if( thisNode == tail )
    {
        tail=tail->prev;
    }

    rg_dNode<T> *preNode=thisNode->prev;
    rg_dNode<T> *nextNode=thisNode->next;

    preNode->next=nextNode;
    nextNode->prev=preNode;

    delete thisNode;
    size--;
    */
}

template<class T>
void rg_dList<T>::kill(rg_dNode<T>* thisNode)
{
    // Assumption: There is thisNode in this list.
    if ( thisNode == rg_NULL || size <= 0 ) {
        return;
    }



    if ( thisNode == current ) {
        killCurrent();
    }
    else {
        if ( thisNode == head ) {
            head=head->next;
        }
        else if ( thisNode == tail )
        {
            tail=tail->prev;
        }
        else {
            // do nothing.
        }


    
        rg_dNode<T> *preNode=thisNode->prev;
        rg_dNode<T> *nextNode=thisNode->next;

        preNode->next=nextNode;
        nextNode->prev=preNode;

        deallocateNode(thisNode);
        size--;

        if ( size == 0 ) {
            head    = rg_NULL;
            tail    = rg_NULL;
            current = rg_NULL;
        }
    }
}


template<class T>
void rg_dList<T>::killCurrent()
{
    if ( current == rg_NULL || size <= 0 ) {
        return;
    }


    // revised by Y.Cho, C.M. Kim, J.Kim and C.Lee (2010.4.3)
    rg_dNode<T>* newCurrent = rg_NULL;
	if ( current == head ) {
		head       = head->next;
        newCurrent = rg_NULL;
	}
	else if ( current == tail )	{
		tail       = tail->prev;
        newCurrent = tail;
	}
    else {
        newCurrent = current->prev;
    }

	rg_dNode<T> *tempNext = current->next;
    rg_dNode<T> *tempPrev = current->prev;

    tempNext->prev=tempPrev;
    tempPrev->next=tempNext;

	deallocateNode(current);


	current = newCurrent;
    size--;

    if ( size == 0 ) {
        head    = rg_NULL;
        tail    = rg_NULL;
        current = rg_NULL;
    }

//revised by Donguk (2000.5.18)
    // if( size == 1 ) {
    //     kill(current);
    // }
    // else
    // {
		//if ( current == tail )
		//{
		//	tail=tail->prev;
		//}
		//rg_dNode<T> *tempNext = current->next;
        //rg_dNode<T> *tempPrev = current->prev;
        //tempNext->prev=tempPrev;
        //tempPrev->next=tempNext;
		//delete current;
		//current = tempPrev;
		//size--;
//end

    // // // ORIGINAL (2000.5.18)
        // // // rg_dNode<T> *tempNext = current->next;
        // // // rg_dNode<T> *tempPrev = current->prev;
        // // // tempNext->prev=tempPrev;
        // // // tempPrev->next=tempNext;
        // // // kill(current);
        // // // current=tempNext;

        // // // rg_dNode<T> *tempNext = current->next;
        // // // rg_dNode<T> *tempPrev = current->prev;
        // // // // rg_dNode<T> *tempCurrent = current;
        // // // // setCurrentPrev();
        // // // // current->next = tempNext;
        // // // // setCurrentNext();
        // // // // current->prev = tempPrev;
        // // // // size = size - 1;

        // // // // kill(tempCurrent);
    // }

}

template<class T>
void rg_dList<T>::killHead()
{
    kill(head);
}

template<class T>
void rg_dList<T>::killTail()
{
    kill(tail);
}


template<class T>
void rg_dList<T>::removeAt(const rg_INDEX &index)
{
    rg_dNode<T> *ptr=getpNode(index);
    if ( ptr != rg_NULL )
    {
        kill(ptr);
    }
}

template<class T>
void rg_dList<T>::duplicateList(const rg_dList<T>& thisList)
{
    if ( size != 0 )
        removeAll();

    rg_dNode<T> *ptr=thisList.head;

    for( rg_INDEX i=0; i < thisList.size; i++)
    {
        addTail(ptr->entity);
        ptr=ptr->next;
    }
}

// set functions
template<class T>
void rg_dList<T>::setHead(const rg_dNode<T>* thisNode)
{
    if ( isInList(thisNode) == rg_TRUE )
    {
        head=(rg_dNode<T>*)thisNode;
        tail=head->prev;
    }
}

template<class T>
void rg_dList<T>::setHead(const T& thisEntity)
{
     rg_dNode<T> *thisNode=whereInList(thisEntity);
    if ( thisNode != rg_NULL )
    {
        head=thisNode;
        tail=head->prev;
    }
}

template<class T>
void rg_dList<T>::setTail(const rg_dNode<T>* thisNode)
{
    if ( isInList(thisNode) == rg_TRUE )
    {
        tail=thisNode;
        head=tail->next;
    }
}

template<class T>
void rg_dList<T>::setTail(const T& thisEntity)
{
    const rg_dNode<T> *thisNode=whereInList(thisEntity);
    if ( thisNode != rg_NULL )
    {
        tail=thisNode;
        head=tail->next;
    }
}

template<class T>
void rg_dList<T>::setCurrent(rg_dNode<T>* thisNode)
{
    if ( isInList(thisNode) == rg_TRUE )
    {
        current=thisNode;
    }
}

template<class T>
void rg_dList<T>::setCurrent(const T& thisEntity)
{
    rg_dNode<T> *thisNode=whereInList(thisEntity);
    if ( thisNode != rg_NULL )
    {
        current=thisNode;
    }
}

template<class T> // added by Hyung-Joo Lee in 1998. 7. 9
void rg_dList<T>::setCurrentTail()
{
    if ( size != 0 )
    {
        current = tail;
    }
}

template<class T>  // added by Hyung-Joo Lee in 1998. 7. 20
void rg_dList<T>::setCurrentHead()
{
    if ( size != 0 )
    {
        current = head;
    }
}

template<class T>
void rg_dList<T>::setCurrentNext()
{
    if ( current != rg_NULL )
    {
        current=current->next;
    }
}

template<class T>
void rg_dList<T>::setCurrentPrev()
{
    if ( current != rg_NULL )
    {
        current=current->prev;
    }
}

template<class T>
void rg_dList<T>::reset()
{
    current=head;
}

/*
template<class T>
void rg_dList<T>::resetSize()
{
    rg_dNodeByPtr<T>* now=head;

    rg_INDEX newSize=0;

    if ( now == rg_NULL )
    {
        newSize=0;
    }else
    {
        do
        {
            newSize++;
            now=now->next;
        }while(now != head);
    }
    size=newSize;
}
*/

////////////////////////////////////////////////
template<class T>
void rg_dList<T>::replaceCurrent(const T& thisEntity)
{
    current->entity = thisEntity;
}


template<class T>
void rg_dList<T>::replace(const T& oldEntity, const T& newEntity)
{
    rg_dNode<T>* currNode = head;
    for ( rg_INT i=0; i<size; i++, currNode=currNode->getNext() ) {
        if ( currNode->getEntity() == oldEntity ) {
            currNode->setEntity( newEntity );
            break;
        }
    }
}

///////////////////////////////////////////////////

// The next two functions are for loop with rg_dList<T> as follows.
// rg_dList<T>  temp;
// T data;
// temp.reset4Loop();
// while( temp.setNext4Loop() )
// {
//      data=temp.getEntity();
// }
//
// rg_dList<T> temp;
// T data;
// rg_INT size=temp.getSize();
// for( rg_INT i=0; i < temp.getSize(); i++, temp.setNext() )
// {
//     data=temp.getEntity();
// }
//
//
// cf) Example of rg_dListByPtr of Deok-soo Kim
// rg_dListByPtr temp;
// T* data;
// temp.reset;
// while( T = getNext() )
// {
// }
// added by TaeBum Jang in 1997. 8.21


template<class T>
void rg_dList<T>::reset4Loop() const
{
    current=rg_NULL;
}

template<class T>
rg_FLAG rg_dList<T>::setNext4Loop() const
{
    if ( head == rg_NULL ) // condition for size of list=0
    {
        return rg_FALSE;
    }

    if ( current == rg_NULL )
    {
        current=head;
        return rg_TRUE;
    }
    else
    {
        current=current->next;

        if ( current == head ) //condition for end of list
        {
            return rg_FALSE;
        }
        else
        {
            return rg_TRUE;
        }
    }
}

////pop & push functions /////////////////////////////////////////
//           by Youngsong Cho (2004. 3. 29)
template<class T>
T rg_dList<T>::popFront()
{
    T frontEntity = head->entity;

    killHead();

    return frontEntity;
}

template<class T>
T rg_dList<T>::popBack()
{
    T backEntity = tail->entity;
    
    killTail();

    return backEntity;
}

template<class T>
T* rg_dList<T>::pushFront(const T& etemp)
{
    return addHead( etemp );
}

template<class T>
T* rg_dList<T>::pushBack(const T& etemp)
{
    return addTail( etemp );
}

  
// get functions //

template<class T>
rg_INDEX rg_dList<T>::getSize() const
{
    return size;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getHead() const
{
    return head;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getTail() const
{
    return tail;
}

template<class T>
T rg_dList<T>::getFirstEntity()const
{
    T firstEntity=head->entity;

    return firstEntity;
}

//    made by Youngsong Cho 2001. 11. 20
template<class T>
T* rg_dList<T>::getFirstpEntity()const
{
//    T firstEntity=head->entity;

    return &(head->entity);
}

template<class T>
T rg_dList<T>::getSecondEntity()const
{
    T secondEntity=head->next->entity;

    return secondEntity;
}


template<class T>
T rg_dList<T>::getLastEntity()const
{
    T lastEntity=tail->entity;

    return lastEntity;
}

template<class T>
T* rg_dList<T>::getLastpEntity()const
{
//    T lastEntity=tail->entity;
//    modifed by Youngsong Cho 2001. 11. 20

    return &(tail->entity);
}

template<class T>
T rg_dList<T>::getAt(const rg_INDEX& index) const
{
    if ( index < 0 || index >= size )
        exit(1);
    
    rg_dNode<T>* ptr=head;
    for ( rg_INDEX i=1 ; i < index+1; i++)
    {
        ptr=ptr->next;
    }

    return ptr->entity;
}
////Swap functions of two nodes using index /////////////////////////////////////////
//           by Cheol-Hyung Cho (2005. 8. 11)
template<class T>
void rg_dList<T>::swapNode(const rg_INDEX& index1, const rg_INDEX& index2)
{
	if ( index1 < 0 || index1 >= size ||
		 index2 < 0 || index2 >= size )
        exit(1);

	int id1, id2;

	if( index2 < index1 )
	{		
		id1 = index2;
		id2 = index1;
	}
	else
	{
		id1 = index1;
		id2 = index2;
	}
    
    rg_dNode<T>* ptr1=head;
	rg_INDEX i=1;
	for (i=1; i < id1+1; i++)
    {
        ptr1=ptr1->next;
    }

	rg_dNode<T>* ptr1_next = ptr1->getNext();
	rg_dNode<T>* ptr1_prev = ptr1->getPrev();

	rg_dNode<T>* ptr2=head;
	for ( i=1 ; i < id2+1; i++)
    {
        ptr2=ptr2->next;
    }

	rg_dNode<T>* ptr2_next = ptr2->getNext();
	rg_dNode<T>* ptr2_prev = ptr2->getPrev();

	//Swapping start
	if( size == 2 )
	{
		head = ptr2;
		tail = ptr1;
	}
	if( ptr1_next == ptr2 && size > 2 )
	{
		ptr2_next->setPrev(ptr1);
		ptr1_prev->setNext(ptr2);

		ptr2->setPrev(ptr1_prev);
		ptr1->setNext(ptr2_next);

		ptr1->setPrev(ptr2);
		ptr2->setNext(ptr1);
	}
	else if( ptr2_next == ptr1 && size > 2 )
	{
		ptr2->setNext( ptr1_next );
		ptr1_next->setPrev( ptr2 );

		ptr1->setPrev( ptr2_prev );
		ptr2_prev->setNext( ptr1 );

		ptr1->setNext( ptr2 );
		ptr2->setPrev( ptr1 );
	}
	else
	{
		ptr1->setNext(ptr2_next);
		ptr2_next->setPrev(ptr1);

		ptr1->setPrev(ptr2_prev);
		ptr2_prev->setNext(ptr1);

		ptr2->setNext(ptr1_next);
		ptr1_next->setPrev(ptr2);

		ptr2->setPrev(ptr1_prev);
		ptr1_prev->setNext(ptr2);
	}

	if(id1 == 0)
		head = ptr2;

	if(id2 == 0)
		head = ptr1;

	if(id2 == size-1)
		tail = ptr1;

	if(id1 == size-1)
		tail = ptr2;

	current = head;
}

template<class T>
T rg_dList<T>::getEntity() const
{
    if ( current != rg_NULL )
    {
         return current->entity;
    }
    else {
    	//T output;
    
    	//return output;
        return T();
    }
    
}

template<class T>
T* rg_dList<T>::getpEntity() const
{
    if ( current != rg_NULL )
    {
        return &(current->entity);
    }
    else
    {
        return (T*)rg_NULL;
    }
}

template<class T>
T* rg_dList<T>::getpNextEntity()
{
    if ( current->next != rg_NULL )
    {
        return &(current->next->entity);
    }
    else
    {
        return (T*)rg_NULL;
    }
}

template<class T>
T rg_dList<T>::getNextEntity() const
{
    T nextEntity=(current->next)->entity;
    return nextEntity;
}

template<class T>     
T rg_dList<T>::getNextEntity(const rg_dNode<T> *currNode) const
{
    if ( isInList(currNode) != rg_NULL )
    {
        T nextEntity=(currNode->next)->entity;
        return nextEntity;
    }
    T nextEntity=(current->next)->entity;
    return nextEntity;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getCurrentpNode()
{
	return current;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getNextpNode()
{
    return current->next;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getNextpNode(const rg_dNode<T> *currNode)
{
    if ( isInList(currNode) == rg_TRUE )
    {
        return currNode->next;
    }

    return rg_NULL;
}

template<class T>
T rg_dList<T>::getPrevEntity() const
{
    T prevEntity=(current->prev)->entity;
    return prevEntity;
}

template<class T>
T rg_dList<T>::getPrevEntity(const rg_dNode<T> *currNode) const
{
    if ( isInList(currNode) != rg_NULL )
    {
        T prevEntity=(currNode->prev)->entity;
        return prevEntity;
    }
    T prevEntity=(current->prev)->entity;
    return prevEntity;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getPrevpNode()
{
    return current->prev;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getPrevpNode(const rg_dNode<T> *currNode)
{
    if ( isInList(currNode) == rg_TRUE )
    {
        return currNode->prev;
    }

    return rg_NULL;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getpNode(const rg_INDEX& index)
{
    if ( index < 0 || index >= size )
    {
        exit(1);
    }
    
    rg_dNode<T>* ptr=head;
    for ( rg_INDEX i=1 ; i < index+1; i++)
    {
        ptr=ptr->next;
    }

    return ptr;
}
//////////////////////////////////////
//////////////////////////////////////
template<class T>
void rg_dList<T>::setpNode(const rg_INDEX& index, const T& input)
{
    if ( index < 0 || index >= size )
    {
        exit(1);
    }
    
    rg_dNode<T>* ptr=head;
    for ( rg_INDEX i=1 ; i < index+1; i++)
    {
        ptr=ptr->next;
    }
    ptr->entity = input;

}
/////////////////////////////////////////
/////////////////////////////////////////
template<class T>
rg_dNode<T>* rg_dList<T>::getFirstpNode()
{
    return head;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getSecondpNode()
{
    return head->next;
}

template<class T>
rg_dNode<T>* rg_dList<T>::getLastpNode()
{
    return tail;
}

// added by TaeBum Jang in 1998. 3.16
// return pointer of array store all data in this list
template<class T>
T* rg_dList<T>::getArray() const
{
    if ( head == rg_NULL )
    {
        return rg_NULL;
    }

    T* output=new T[size];
    rg_dNode<T>* currentNode=head;

    for( rg_INDEX i=0; i < size; i++ )
    {
        output[i]=currentNode->getEntity();
        currentNode=currentNode->next;
    }

    return output;
}

// added by Hyung-Joo Lee in 1998. 6.22
// return pointer of array store all data in this list
// and kill the original list.
template<class T>
T* rg_dList<T>::getArrayKillList()
{
    if ( head == rg_NULL )
    {
        return rg_NULL;
    }

    T* output=new T[size];
    current = head;

    rg_INDEX n = size;
    for( rg_INDEX i = 0; i < n; i++ )
    {
        output[i]=current->getEntity();
        killCurrent();
    }

    removeAll();
    return output;
}

// Function to answer FAQ

template<class T>
rg_FLAG rg_dList<T>::isInList(const rg_dNode<T> *thisNode) const
{
    rg_dNode<T> *ptr=head;

    for( rg_INDEX i=0; i < size; i++)
    {
        if ( ptr == thisNode )
        {
            return rg_TRUE;
        }
        ptr=ptr->next;
    }
    return rg_FALSE;
}

template<class T>
rg_FLAG rg_dList<T>::isInList(const T& etemp) const
{
    rg_dNode<T> *ptr=head;

    for( rg_INDEX i=0; i < size; i++)
    {
        if ( ptr->getEntity() == etemp )
        {
            return rg_TRUE;
        }
        ptr=ptr->next;
    }
    return rg_FALSE;
}

template<class T>
rg_FLAG rg_dList<T>::isHead() const
{
    return ( current == head );
}

template<class T>
rg_FLAG rg_dList<T>::isTail() const
{
    return ( current == tail );
}

template<class T>
rg_FLAG rg_dList<T>::isEmpty() const
{
    return ( head == rg_NULL );
}

template<class T>
rg_dNode<T> *rg_dList<T>::whereInList(const T& etemp,const rg_INDEX& thisth) const
{
    rg_dNode<T> *ptr=head;
    rg_INDEX loop=0;

    for( rg_INDEX i=0; i < size; i++)
    {
        if ( ptr->entity == etemp )
        {
            loop++;
            if ( loop == thisth )
            {
                return ptr;
            }
        }
        ptr=ptr->next;
    }
    return rg_NULL;
}

template<class T>
rg_dNode<T>* rg_dList<T>::whereInList(      T*        pEntity,
                                      const rg_INDEX& thisth) const
{
    rg_dNode<T> *ptr=head;
    rg_INDEX loop=0;

    for( rg_INDEX i=0; i < size; i++)
    {
        if ( &(ptr->entity) == pEntity )
        {
            loop++;
            if ( loop == thisth )
            {
                return ptr;
            }
        }
        ptr=ptr->next;
    }
    return rg_NULL;
}


template<class T>
rg_dList<T>& rg_dList<T>::operator=(const rg_dList<T>& temp)
{
    duplicateList(temp);
    return (*this);
}

template<class T>
T& rg_dList<T>::operator[](const rg_INDEX& index)
{
    if ( index < 0 || index >= size )
    {
        exit(1);
    }
    
    rg_dNode<T>* ptr=head;
    for ( rg_INDEX i=1 ; i <= index; i++)
    {
        ptr=ptr->next;
    }

    return ptr->entity;
}

template<class T>
T& rg_dList<T>::elementAt(const rg_INDEX& index) const
{
    if ( index < 0 || index >= size )
    {
        exit(1);
    }
    
    rg_dNode<T>* ptr=head;
    for ( rg_INDEX i=1 ; i <= index; i++)
    {
        ptr=ptr->next;
    }

    return ptr->entity;
}

#endif


