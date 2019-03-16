#ifndef NodeType_H
#define NodeType_H

#include <algorithm>
#include <memory>
#include <vector>

template <typename NodeType, template <typename> class SharedPtrType = std::shared_ptr,
          template <typename> class EnableSharedFromThisType = std::enable_shared_from_this>
class AbstractTreeNode : public EnableSharedFromThisType<NodeType>
{
protected:
    AbstractTreeNode();

    std::vector<SharedPtrType<NodeType>> children;
    NodeType*                            parentObject = nullptr;

public:
    typedef std::vector<std::vector<SharedPtrType<NodeType>>> Container;

    typedef std::vector<SharedPtrType<NodeType>>                                value_type;
    typedef std::vector<SharedPtrType<NodeType>>&                               reference;
    typedef const NodeType&                                                     const_reference;
    typedef size_t                                                              size_type;
    typedef size_type                                                           difference_type;
    typedef AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType> AbstractTreeNodeType;

    typedef typename Container::iterator       iterator;
    typedef typename Container::const_iterator const_iterator;

    void      setParentObject(AbstractTreeNodeType* ptr);
    void      setParentObject(NodeType* ptr);
    void      setParentObject(std::nullptr_t);
    NodeType* getParentObject() const;
    long      getThisObjNumberInParent() const;

    size_type               countChildren() const;
    void                    reserveChildrenSize(size_t size);
    void                    appendChild(const SharedPtrType<NodeType> obj);
    void                    appendChildren(const std::vector<SharedPtrType<NodeType>>& objs);
    void                    removeChild(const NodeType* obj);
    void                    removeChild(int index);
    void                    removeChildren(int index, int count);
    void                    moveChildren(int from, int count, int to);
    SharedPtrType<NodeType> releaseChild(int index);
    void                    swapChildren(int index1, int index2);
    SharedPtrType<NodeType> cloneFromThis();
    SharedPtrType<NodeType> sharedFromThis();
    void                    insertChild(const SharedPtrType<NodeType> obj, long pos = 0);
    void                    insertChildren(const std::vector<SharedPtrType<NodeType>>& objs, long pos);

    iterator       begin();
    iterator       end();
    const_iterator begin() const;
    const_iterator end() const;

    SharedPtrType<NodeType>&       operator[](size_type index);
    const SharedPtrType<NodeType>& operator[](size_type index) const;
    SharedPtrType<NodeType>&       at(size_type index);
    const SharedPtrType<NodeType>& at(size_type index) const;

    virtual ~AbstractTreeNode();
};

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::AbstractTreeNode()
{
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::~AbstractTreeNode()
{
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
typename AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::size_type
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::countChildren() const
{
    return children.size();
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::reserveChildrenSize(
    size_t size)
{
    this->children.reserve(size);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::appendChildren(
    const std::vector<SharedPtrType<NodeType>>& objs)
{
    for (long i = 0; i < static_cast<long>(objs.size()); i++) {
        this->appendChild(objs[i]);
    }
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::appendChild(
    const SharedPtrType<NodeType> obj)
{
    if (obj.get() == this) {
        throw std::logic_error("A node cannot be a child of itself.");
    }
    obj->setParentObject(this);
    children.push_back(obj);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::insertChild(
    const SharedPtrType<NodeType> obj, long pos)
{
    if (obj.get() == this) {
        throw std::logic_error("A node cannot be a child of itself.");
    }
    obj->setParentObject(this);
    children.insert(children.begin() + pos, obj);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::insertChildren(
    const std::vector<SharedPtrType<NodeType>>& objs, long pos)
{
    for (long i = 0; i < static_cast<long>(objs.size()); i++) {
        insertChild(objs[i], pos + i);
    }
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::removeChild(
    const NodeType* obj)
{
    children.erase(
        std::remove_if(children.begin(), children.end(),
                       [&obj](const SharedPtrType<AbstractTreeNode>& el) { return (el.get() == obj); }),
        children.end());
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::removeChild(int index)
{
    children.erase(children.begin() + index);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::removeChildren(int index,
                                                                                         int count)
{
    children.erase(children.begin() + index, children.begin() + index + count);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::moveChildren(int from,
                                                                                       int count, int to)
{
    if (from == to)
        return;

    // fix boundary problems
    if (to + count - 1 > static_cast<long>(this->countChildren()) - 1) {
        to = static_cast<long>(this->countChildren()) - count;
    }
    if (to < 0) {
        to = 0;
    }

    int delta = to - from;

    std::vector<SharedPtrType<NodeType>> backup(children.begin() + from,
                                                children.begin() + from + count);
    // move the elements between "from" and "to"
    if (delta > 0) {
        int move_start = from + count;
        int move_end   = from + count + delta;
        int move_to    = from;

        std::copy(children.begin() + move_start, children.begin() + move_end,
                  children.begin() + move_to);
    } else {
        int move_start = from + delta;
        int move_end   = from;
        // keep in mind that move_to will indicate the past-last element
        // due to using copy_backward
        int move_to = from + count;
        std::copy_backward(children.begin() + move_start, children.begin() + move_end,
                           children.begin() + move_to);
    }
    std::copy(backup.begin(), backup.end(), children.begin() + to);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
SharedPtrType<NodeType>
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::releaseChild(int index)
{
    SharedPtrType<AbstractTreeNode> releasedObj = children[index];
    children.erase(children.begin() + index);
    releasedObj->setParentObject(nullptr);
    return releasedObj;
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::swapChildren(int index1,
                                                                                       int index2)
{
    children[index1].swap(children[index2]);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
SharedPtrType<NodeType>
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::cloneFromThis()
{
    SharedPtrType<NodeType> copy = std::make_shared<NodeType, SharedPtrType, EnableSharedFromThisType>();
    *copy                        = *static_cast<NodeType*>(this);
    copy->setParentObject(nullptr);
    copy->children.clear();
    for (long i = 0; i < static_cast<long>(this->countChildren()); i++) {
        copy->appendChild(this->children[i]->cloneFromThis());
    }
    return copy;
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
SharedPtrType<NodeType>
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::sharedFromThis()
{
    return std::static_pointer_cast<NodeType, SharedPtrType, EnableSharedFromThisType>(
        this->shared_from_this());
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::setParentObject(
    AbstractTreeNodeType* ptr)
{
    this->parentObject = static_cast<NodeType*>(ptr);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::setParentObject(NodeType* ptr)
{
    this->parentObject = static_cast<NodeType*>(ptr);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
void AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::setParentObject(std::nullptr_t)
{
    this->parentObject = nullptr;
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
NodeType* AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::getParentObject() const
{
    return parentObject;
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
typename AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::iterator
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::begin()
{
    return children.begin();
}
template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
typename AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::iterator
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::end()
{
    return children.end();
}
template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
typename AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::const_iterator
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::begin() const
{
    return children.begin();
}
template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
typename AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::const_iterator
AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::end() const
{
    return children.end();
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
SharedPtrType<NodeType>& AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::
                         operator[](size_type index)
{
    return children[index];
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
const SharedPtrType<NodeType>& AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::
                               operator[](size_type index) const
{
    return children[index];
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
SharedPtrType<NodeType>& AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::at(
    AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::size_type index)
{
    return children.at(index);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
const SharedPtrType<NodeType>& AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::at(
    AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::size_type index) const
{
    return children.at(index);
}

template <typename NodeType, template <typename> class SharedPtrType,
          template <typename> class EnableSharedFromThisType>
long AbstractTreeNode<NodeType, SharedPtrType, EnableSharedFromThisType>::getThisObjNumberInParent()
    const
{
    AbstractTreeNode* parentPtr = (AbstractTreeNode*)getParentObject();
    if (parentPtr == nullptr) {
        return 0; // this is never used by the model
    } else {
        for (long i = 0; i < static_cast<long>(parentPtr->countChildren()); i++) {
            if (this == (*parentPtr)[i].get()) {
                return i;
            }
        }
        return -1;
    }
}

#endif // NodeType_H
