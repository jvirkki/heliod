/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PTRLIST_H_
#define PTRLIST_H_
#include <prlock.h>
template <class T> class PtrListNode{
  public:
    PtrListNode(){
      next = NULL;
      data = NULL;
      prev = NULL;
    }

    ~PtrListNode(){
      if(next != NULL) delete next;
    }
 
    PtrListNode<T> *next;
    PtrListNode<T> *prev;
    T *data;
};

template <class T> class PtrList{
  public:
    PtrList();
    ~PtrList();
    T* remove(const T *value);
    T* removeAt(PRInt32 i);
    PRInt32 entries(void) const  {return size;}
    void clear(void);
    void append(T *a);
PRBool  insert(T *a){
//We are calling another public method so we do not lock the mutex
      append(a);
      return PR_TRUE; 
    }

    T* at (PRInt32 i)const{
      T* rv = NULL;
      PR_Lock(lock);
          if( (const_cast<PtrList *>(this))->ajust_current(i) == PR_TRUE){
            rv = current->data;
          }
      PR_Unlock(lock);
      return rv;
    }
  private:
    PRBool abs_remove(PtrListNode<T> *temp);
    PRBool ajust_current(PRInt32 i);
    PRBool is_valid_offset(PRInt32 i) const{
      return   (((i < size) && (i >= 0)) ?  PR_TRUE :  PR_FALSE);
    }
    
//List size
    PRInt32 size;
//Current position in the list
    PRInt32 current_offset;
    PtrListNode<T> *current;
//Head and tail nodes of the list
    PtrListNode<T> *head;
    PtrListNode<T> *tail;
//Corse grain lock
    PRLock *lock;
};


//End Class Def
template<class T>
PtrList<T>::PtrList(){
      head = NULL;
      tail = NULL;
      current = NULL;
      size = 0;
      current_offset = -1;
      lock = PR_NewLock();
    }

template<class T>
PtrList<T>::~PtrList(){
      PR_Lock(lock);
       if(head != NULL) delete head;
      PR_Unlock(lock);
      PR_DestroyLock(lock);
    } 
template<class T>
T* PtrList<T>::remove(const T *value){  
      PtrListNode<T> *temp; 
      PR_Lock(lock);
      T* rv = NULL;
      temp = head;
      while((temp != NULL) && (rv == NULL)){
        if(temp->data == value){ 
          if(abs_remove(temp) == PR_TRUE){
            rv = const_cast<T *>(value);
          }
        } 
        temp = temp->next;
      } 
      PR_Unlock(lock);
      return rv;
    }

template<class T>
T* PtrList<T>::removeAt(PRInt32 i){
      T* rv = NULL;
      PtrListNode<T> *temp = NULL; 
      PR_Lock(lock);
      
      if(ajust_current(i) == PR_TRUE) {
        temp = current;
        if( abs_remove(temp) == PR_TRUE) rv = temp->data;
      }
      PR_Unlock(lock); 
      return rv;
    }

template<class T>
void PtrList<T>::append(T *a){
      PR_Lock(lock);
      PtrListNode<T> *temp = new PtrListNode<T>;
      temp->data = a;
      if(size != 0){
        temp->prev = tail;
        tail->next = temp;
        tail = temp;
      }else{
        head = tail = temp;
      }
      size++;
      PR_Unlock(lock); 
    }

template<class T>
PRBool PtrList<T>::ajust_current(PRInt32 i){
      if(size <= 0) return PR_FALSE;
      if(is_valid_offset(i) == PR_FALSE){
        return PR_FALSE;
      }
      if(is_valid_offset(current_offset) == PR_FALSE){
        current = head;
        current_offset = 0;
      } 
      if(abs(current_offset - i) > i){
//The head of the list is closer to our move target
        current = head;
        current_offset = 0;
      } 
      if(abs(current_offset -1) > abs((size-1) - i)){
        current_offset = size -1;
        current = tail;
      }
//The current offset is before or at the target
      while(current_offset < i){
        current_offset++;
        current = current->next;
      }
      while(current_offset > i){
        current_offset--;
        current = current->prev;
      }
      if(current_offset == i) return PR_TRUE;
      return PR_FALSE;
    }

template<class T>
void PtrList<T>::clear(void){
      PR_Lock(lock);
      if(head != NULL) delete head;
      head = tail = NULL;
      size = 0;
      current = NULL;
      current_offset = -1;
      PR_Unlock(lock);
    }

template<class T>
    PRBool PtrList<T>::abs_remove(PtrListNode<T> *temp){
      if(temp == NULL) return PR_FALSE;
      if((temp == head) && (temp == tail)){
        delete head;
        head = tail = NULL;
      }else if((temp == head)){
        head = head->next;
        head->prev = NULL;
        temp->next = NULL; //So the destructor dosen't destroy all of the list
        delete temp;
      }else if((temp == tail)){
        tail = tail->prev;
        tail->next = NULL; 
        delete temp;
      }else{
//General case     
        temp->next->prev = temp->prev;
        temp->prev->next = temp->next;
        temp->next = NULL;
      }
      size--;
//Flush the cached position
      current_offset = -1;
      current = NULL;   
      return PR_TRUE;
   }
#endif
