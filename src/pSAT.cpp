#include "pSAT.hpp"


// ----------------------------------------------------------------------------
// related to SQueue
// ----------------------------------------------------------------------------

template <typename T>
bool SQueue<T>::empty() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.empty();
}

template <typename T>
unsigned SQueue<T>::size() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.size();
}

template <typename T>
void SQueue<T>::emplace(T &&item)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        q.emplace(std::move(item));
    }
    _cv.notify_one();
}

template <typename T>
T SQueue<T>::pop()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _cv.wait(lock, [this]()
             { return !q.empty(); });

    T item = std::move(q.front());
    q.pop();
    return item;
}

template <typename T>
bool SQueue<T>::try_pop(T &item)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (q.empty())
        return false;

    item = std::move(q.front());
    q.pop();
    return true;
}