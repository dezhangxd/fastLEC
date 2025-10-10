#include "pSAT.hpp"

// ----------------------------------------------------------------------------
// related to SQueue
// ----------------------------------------------------------------------------

// std::shared_mutex _prt_mtx;

template <typename T> bool fastLEC::SQueue<T>::empty() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.empty();
}

template <typename T> unsigned fastLEC::SQueue<T>::size() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.size();
}

template <typename T> void fastLEC::SQueue<T>::emplace(T &&item)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        q.emplace(std::move(item));
    }
    _cv.notify_one();
}

template <typename T> T fastLEC::SQueue<T>::pop()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _cv.wait(lock,
             [this]()
             {
                 return !q.empty();
             });

    T item = std::move(q.front());
    q.pop();
    return item;
}

template <typename T> bool fastLEC::SQueue<T>::try_pop(T &item)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (q.empty())
        return false;

    item = std::move(q.front());
    q.pop();
    return true;
}

// ----------------------------------------------------------------------

fastLEC::pSAT::pSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads)
    : xag(xag), n_threads(n_threads)
{
    cnf = xag->construct_cnf_from_this_xag();
}

fastLEC::ret_vals fastLEC::pSAT::check_xag() { return ret_vals::ret_UNS; }