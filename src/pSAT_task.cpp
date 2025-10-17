
#include "pSAT_task.hpp"

#include "basic.hpp"
#include <iomanip>
#include <iostream>

// ----------------------------------------------------------------------------
// related to SQueue
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// Task class implementation
// ----------------------------------------------------------------------------

fastLEC::Task::Task()
    : state(WAITING), id(ID_NONE), cpu(CPU_NONE), new_cube_lit_cnt(0),
      father(ID_NONE), level(0)
{
    is_propagated = false;

    create_time = fastLEC::ResMgr::get().get_runtime();
    start_time = 0.0;
    stop_time = 0.0;
}

double fastLEC::Task::runtime() const
{
    if (is_solved())
        return stop_time - start_time;
    else if (start_time > 0.0)
        return fastLEC::ResMgr::get().get_runtime() - start_time;
    else
        return -1.0;
}

void fastLEC::Task::terminate_info_upd()
{
    task_states s = state.load(std::memory_order_relaxed);
    while (s == WAITING || s == ADDING || s == RUNNING)
    {
        if (state.compare_exchange_weak(s, UNKNOWN, std::memory_order_relaxed))
            break;
    }
    if (stop_time == 0.0)
        stop_time = fastLEC::ResMgr::get().get_runtime();
}

std::ostream &fastLEC::operator<<(std::ostream &os, const Task &t)
{
    os << "T[";
    if (t.state == task_states::WAITING)
        os << "W";
    else if (t.state == task_states::ADDING)
        os << "A";
    else if (t.state == task_states::RUNNING)
        os << "R";
    else if (t.state == task_states::SATISFIABLE)
        os << "S";
    else if (t.state == task_states::UNSATISFIABLE)
        os << "U";
    else
        os << "?";

    os << ",";
    os << std::left << t.id;
    os << ",C";
    os << std::left << t.cpu;
    os << ",L";
    os << std::left << t.level;
    os << "]";

    os << "{";
    std::string father_id = "";
    if (t.is_root())
        father_id = "N/A ";
    else if (t.father == ID_ROOT)
        father_id = "ROOT";
    else
        father_id = "F" + std::to_string(t.father);

    os << "+";
    for (unsigned i = t.cube.size() - t.new_cube_lit_cnt; i < t.cube.size();
         i++)
    {
        os << t.cube[i];
        if (i != t.cube.size() - 1)
            os << ",";
    }
    os << "}";

    os << "," << std::right << std::setw(7) << std::fixed
       << std::setprecision(2) << t.runtime() << "s";
    return os;
}

// Explicit template instantiation for SQueue<int>
template class fastLEC::SQueue<int>;