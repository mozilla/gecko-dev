/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_InlineList_h
#define jit_InlineList_h

#include "jsutil.h"

namespace js {

template <typename T> class InlineForwardList;
template <typename T> class InlineForwardListIterator;

template <typename T>
class InlineForwardListNode
{
  public:
    InlineForwardListNode() : next(nullptr)
    { }
    explicit InlineForwardListNode(InlineForwardListNode<T> *n) : next(n)
    { }

  protected:
    friend class InlineForwardList<T>;
    friend class InlineForwardListIterator<T>;

    InlineForwardListNode<T> *next;
};

template <typename T>
class InlineForwardList : protected InlineForwardListNode<T>
{
    friend class InlineForwardListIterator<T>;

    typedef InlineForwardListNode<T> Node;

    Node *tail_;
#ifdef DEBUG
    int modifyCount_;
#endif

    InlineForwardList<T> *thisFromConstructor() {
        return this;
    }

  public:
    InlineForwardList()
      : tail_(thisFromConstructor())
    {
#ifdef DEBUG
        modifyCount_ = 0;
#endif
    }

  public:
    typedef InlineForwardListIterator<T> iterator;

  public:
    iterator begin() const {
        return iterator(this);
    }
    iterator end() const {
        return iterator(nullptr);
    }
    iterator removeAt(iterator &where) {
        iterator iter(where);
        iter++;
        iter.prev = where.prev;
#ifdef DEBUG
        iter.modifyCount_++;
#endif

        // Once the element 'where' points at has been removed, it is no longer
        // safe to do any operations that would touch 'iter', as the element
        // may be added to another list, etc. This nullptr ensures that any
        // improper uses of this function will fail quickly and loudly.
        removeAfter(where.prev, where.iter);
        where.prev = where.iter = nullptr;

        return iter;
    }
    void pushFront(Node *t) {
        insertAfter(this, t);
    }
    void pushBack(Node *t) {
        JS_ASSERT(t->next == nullptr);
#ifdef DEBUG
        modifyCount_++;
#endif
        tail_->next = t;
        tail_ = t;
    }
    T *popFront() {
        JS_ASSERT(!empty());
        T* result = static_cast<T *>(this->next);
        removeAfter(this, result);
        return result;
    }
    T *back() {
        JS_ASSERT(!empty());
        return static_cast<T *>(tail_);
    }
    void insertAfter(Node *at, Node *item) {
        JS_ASSERT(item->next == nullptr);
#ifdef DEBUG
        modifyCount_++;
#endif
        if (at == tail_)
            tail_ = item;
        item->next = at->next;
        at->next = item;
    }
    void removeAfter(Node *at, Node *item) {
#ifdef DEBUG
        modifyCount_++;
#endif
        if (item == tail_)
            tail_ = at;
        JS_ASSERT(at->next == item);
        at->next = item->next;
        item->next = nullptr;
    }
    void splitAfter(Node *at, InlineForwardList<T> *to) {
        JS_ASSERT(to->empty());
        if (!at)
            at = this;
        if (at == tail_)
            return;
#ifdef DEBUG
        modifyCount_++;
#endif
        to->next = at->next;
        to->tail_ = tail_;
        tail_ = at;
        at->next = nullptr;
    }
    bool empty() const {
        return tail_ == this;
    }
    void clear() {
        this->next = nullptr;
        tail_ = this;
#ifdef DEBUG
        modifyCount_ = 0;
#endif
    }
};

template <typename T>
class InlineForwardListIterator
{
private:
    friend class InlineForwardList<T>;

    typedef InlineForwardListNode<T> Node;

    explicit InlineForwardListIterator<T>(const InlineForwardList<T> *owner)
      : prev(const_cast<Node *>(static_cast<const Node *>(owner))),
        iter(owner ? owner->next : nullptr)
#ifdef DEBUG
      , owner_(owner),
        modifyCount_(owner ? owner->modifyCount_ : 0)
#endif
    { }

public:
    InlineForwardListIterator<T> & operator ++() {
        JS_ASSERT(modifyCount_ == owner_->modifyCount_);
        prev = iter;
        iter = iter->next;
        return *this;
    }
    InlineForwardListIterator<T> operator ++(int) {
        InlineForwardListIterator<T> old(*this);
        operator++();
        return old;
    }
    T * operator *() const {
        JS_ASSERT(modifyCount_ == owner_->modifyCount_);
        return static_cast<T *>(iter);
    }
    T * operator ->() const {
        JS_ASSERT(modifyCount_ == owner_->modifyCount_);
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineForwardListIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineForwardListIterator<T> &where) const {
        return iter == where.iter;
    }

private:
    Node *prev;
    Node *iter;

#ifdef DEBUG
    const InlineForwardList<T> *owner_;
    int modifyCount_;
#endif
};

template <typename T> class InlineList;
template <typename T> class InlineListIterator;
template <typename T> class InlineListReverseIterator;

template <typename T>
class InlineListNode : public InlineForwardListNode<T>
{
  public:
    InlineListNode() : InlineForwardListNode<T>(nullptr), prev(nullptr)
    { }
    InlineListNode(InlineListNode<T> *n, InlineListNode<T> *p)
      : InlineForwardListNode<T>(n),
        prev(p)
    { }

  protected:
    friend class InlineList<T>;
    friend class InlineListIterator<T>;
    friend class InlineListReverseIterator<T>;

    InlineListNode<T> *prev;
};

template <typename T>
class InlineList : protected InlineListNode<T>
{
    typedef InlineListNode<T> Node;

  public:
    InlineList() : InlineListNode<T>(MOZ_THIS_IN_INITIALIZER_LIST(), MOZ_THIS_IN_INITIALIZER_LIST())
    { }

  public:
    typedef InlineListIterator<T> iterator;
    typedef InlineListReverseIterator<T> reverse_iterator;

  public:
    iterator begin() const {
        return iterator(static_cast<Node *>(this->next));
    }
    iterator begin(Node *t) const {
        return iterator(t);
    }
    iterator end() const {
        return iterator(this);
    }
    reverse_iterator rbegin() const {
        return reverse_iterator(this->prev);
    }
    reverse_iterator rbegin(Node *t) const {
        return reverse_iterator(t);
    }
    reverse_iterator rend() const {
        return reverse_iterator(this);
    }
    template <typename itertype>
    itertype removeAt(itertype &where) {
        itertype iter(where);
        iter++;

        // Once the element 'where' points at has been removed, it is no longer
        // safe to do any operations that would touch 'iter', as the element
        // may be added to another list, etc. This nullptr ensures that any
        // improper uses of this function will fail quickly and loudly.
        remove(where.iter);
        where.iter = nullptr;

        return iter;
    }
    void pushFront(Node *t) {
        insertAfter(this, t);
    }
    void pushFrontUnchecked(Node *t) {
        insertAfterUnchecked(this, t);
    }
    void pushBack(Node *t) {
        insertBefore(this, t);
    }
    void pushBackUnchecked(Node *t) {
        insertBeforeUnchecked(this, t);
    }
    T *popFront() {
        JS_ASSERT(!empty());
        T *t = static_cast<T *>(this->next);
        remove(t);
        return t;
    }
    T *popBack() {
        JS_ASSERT(!empty());
        T *t = static_cast<T *>(this->prev);
        remove(t);
        return t;
    }
    T *peekBack() const {
        iterator iter = end();
        iter--;
        return *iter;
    }
    void insertBefore(Node *at, Node *item) {
        JS_ASSERT(item->prev == nullptr);
        JS_ASSERT(item->next == nullptr);
        insertBeforeUnchecked(at, item);
    }
    void insertBeforeUnchecked(Node *at, Node *item) {
        item->next = at;
        item->prev = at->prev;
        at->prev->next = item;
        at->prev = item;
    }
    void insertAfter(Node *at, Node *item) {
        JS_ASSERT(item->prev == nullptr);
        JS_ASSERT(item->next == nullptr);
        insertAfterUnchecked(at, item);
    }
    void insertAfterUnchecked(Node *at, Node *item) {
        item->next = at->next;
        item->prev = at;
        static_cast<Node *>(at->next)->prev = item;
        at->next = item;
    }
    void remove(Node *t) {
        t->prev->next = t->next;
        static_cast<Node *>(t->next)->prev = t->prev;
        t->next = t->prev = nullptr;
    }
    void clear() {
        this->next = this->prev = this;
    }
    bool empty() const {
        return begin() == end();
    }
    void takeElements(InlineList &l) {
        MOZ_ASSERT(&l != this, "cannot takeElements from this");
        Node *lprev = l.prev;
        static_cast<Node *>(l.next)->prev = this;
        lprev->next = this->next;
        static_cast<Node *>(this->next)->prev = l.prev;
        this->next = l.next;
        l.clear();
    }
};

template <typename T>
class InlineListIterator
{
  private:
    friend class InlineList<T>;

    typedef InlineListNode<T> Node;

    explicit InlineListIterator(const Node *iter)
      : iter(const_cast<Node *>(iter))
    { }

  public:
    InlineListIterator<T> & operator ++() {
        iter = static_cast<Node *>(iter->next);
        return *this;
    }
    InlineListIterator<T> operator ++(int) {
        InlineListIterator<T> old(*this);
        operator++();
        return old;
    }
    InlineListIterator<T> & operator --() {
        iter = iter->prev;
        return *this;
    }
    InlineListIterator<T> operator --(int) {
        InlineListIterator<T> old(*this);
        operator--();
        return old;
    }
    T * operator *() const {
        return static_cast<T *>(iter);
    }
    T * operator ->() const {
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineListIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineListIterator<T> &where) const {
        return iter == where.iter;
    }

  private:
    Node *iter;
};

template <typename T>
class InlineListReverseIterator
{
  private:
    friend class InlineList<T>;

    typedef InlineListNode<T> Node;

    explicit InlineListReverseIterator(const Node *iter)
      : iter(const_cast<Node *>(iter))
    { }

  public:
    InlineListReverseIterator<T> & operator ++() {
        iter = iter->prev;
        return *this;
    }
    InlineListReverseIterator<T> operator ++(int) {
        InlineListReverseIterator<T> old(*this);
        operator++();
        return old;
    }
    InlineListReverseIterator<T> & operator --() {
        iter = static_cast<Node *>(iter->next);
        return *this;
    }
    InlineListReverseIterator<T> operator --(int) {
        InlineListReverseIterator<T> old(*this);
        operator--();
        return old;
    }
    T * operator *() {
        return static_cast<T *>(iter);
    }
    T * operator ->() {
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineListReverseIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineListReverseIterator<T> &where) const {
        return iter == where.iter;
    }

  private:
    Node *iter;
};

/* This list type is more or less exactly an InlineForwardList without a sentinel
 * node. It is useful in cases where you are doing algorithms that deal with many
 * merging singleton lists, rather than often empty ones.
 */
template <typename T> class InlineConcatListIterator;
template <typename T>
class InlineConcatList
{
  private:
    typedef InlineConcatList<T> Node;

    InlineConcatList<T> *thisFromConstructor() {
        return this;
    }

  public:
    InlineConcatList() : next(nullptr), tail(thisFromConstructor())
    { }

    typedef InlineConcatListIterator<T> iterator;

    iterator begin() const {
        return iterator(this);
    }

    iterator end() const {
        return iterator(nullptr);
    }

    void append(InlineConcatList<T> *adding)
    {
        JS_ASSERT(tail);
        JS_ASSERT(!tail->next);
        JS_ASSERT(adding->tail);
        JS_ASSERT(!adding->tail->next);

        tail->next = adding;
        tail = adding->tail;
        adding->tail = nullptr;
    }

  protected:
    friend class InlineConcatListIterator<T>;
    Node *next;
    Node *tail;
};

template <typename T>
class InlineConcatListIterator
{
  private:
    friend class InlineConcatList<T>;

    typedef InlineConcatList<T> Node;

    explicit InlineConcatListIterator(const Node *iter)
      : iter(const_cast<Node *>(iter))
    { }

  public:
    InlineConcatListIterator<T> & operator ++() {
        iter = static_cast<Node *>(iter->next);
        return *this;
    }
    InlineConcatListIterator<T> operator ++(int) {
        InlineConcatListIterator<T> old(*this);
        operator++();
        return old;
    }
    T * operator *() const {
        return static_cast<T *>(iter);
    }
    T * operator ->() const {
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineConcatListIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineConcatListIterator<T> &where) const {
        return iter == where.iter;
    }

  private:
    Node *iter;
};

} // namespace js

#endif /* jit_InlineList_h */
