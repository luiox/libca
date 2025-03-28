#ifndef LIBCA_BASE_WRAPPER_HPP
#define LIBCA_BASE_WRAPPER_HPP

namespace ca {

template<typename T>
class Ref
{
public:
    // 构造函数接受一个引用，并将其存储为指针
    Ref(T& ref)
        : ptr(&ref)
    {}

    // 重载解引用运算符，使得 Ref 可以像普通引用一样使用
    T& operator*() const { return *ptr; }

    // 重载成员访问运算符，使得 Ref 可以像普通指针一样使用
    T* operator->() const { return ptr; }

    operator T() const { return *ptr; }

    T& operator=(T value)
    {
        *ptr = value;
        return *ptr;
    }

private:
    T* ptr;   // 指向要引用的数据的指针
};


}   // namespace ca

#endif   // !LIBCA_BASE_WRAPPER_HPP
