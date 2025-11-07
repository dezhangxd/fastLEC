#include "pSAT.hpp"
#include "parser.hpp"

#include <iomanip>
#include <cassert>
#include <shared_mutex>

#include <algorithm>

std::shared_mutex _prt_mtx;

#define PRT_SOLVING_INFO

void fastLEC::PartitionSAT::show_unsolved_tasks()
{
    std::stringstream ss;
    for (auto &task : all_tasks)
    {
        if (!task->is_solved())
        {
            ss << "c [U] " << std::setw(6) << task->father;
            ss << " <f-";
            ss << std::setw(6) << task->id << " : ";
            for (auto &cube : task->cube)
                ss << std::setw(5) << cube << " ";
            ss << " | ";
            ss << *task;
            ss << std::endl;
        }
    }
    {
        std::lock_guard<std::shared_mutex> lock(_prt_mtx);
        std::cout << ss.str() << std::flush;
    }
}

void fastLEC::PartitionSAT::show_detailed_tasks()
{
    std::stringstream ss;
    ss << "c [All Task] ++++++++++++++++++++++";
    ss << std::left << std::setw(6) << std::right << this->all_tasks.size();
    ss << " tasks ++++++++++++++++++++++++++++++" << std::endl;
    for (auto &task : all_tasks)
    {
        ss << "c [A] " << std::setw(6) << task->father;
        ss << " <f-";
        ss << std::setw(6) << task->id << " : ";
        for (auto &cube : task->cube)
            ss << std::setw(5) << cube << " ";
        ss << " | ";
        ss << *task;
        ss << std::endl;
    }
    {
        std::lock_guard<std::shared_mutex> lock(_prt_mtx);
        std::cout << ss.str() << std::flush;
    }
}

void fastLEC::PartitionSAT::show_pool()
{
    unsigned line_ct = Param::get().custom_params.log_items_per_line;
    std::stringstream ss;
    ss << "c [CPU states] ----------- ";
    ss << std::left << std::setw(4) << this->n_threads;
    ss << " threads, time point:";
    ss << std::left << std::setw(7) << std::fixed << std::setprecision(2)
       << fastLEC::ResMgr::get().get_runtime();
    ss << "s -----------------" << std::endl;
    ss << "c [P] ";
    for (unsigned i = 0; i < this->n_threads; i++)
    {
        std::stringstream tmp;
        tmp << std::setw(3) << std::right << i << ": ";
        auto t = get_task_by_cpu(i);
        if (t == nullptr)
        {
            tmp << std::left << "N/A";
        }
        else
        {
            tmp << std::left << *t;
        }
        ss << std::setw(60) << std::left << tmp.str();
        if (i % line_ct == line_ct - 1 && i != this->n_threads - 1)
            ss << std::endl << "c [P] ";
    }
    ss << std::endl;

    {
        std::lock_guard<std::shared_mutex> lock(_prt_mtx);
        std::cout << ss.str() << std::flush;
    }
}

std::ostream &fastLEC::operator<<(std::ostream &os, const PartitionSAT &ps)
{
    unsigned line_ct = Param::get().custom_params.log_items_per_line;
    std::stringstream ss;
    ss << "c [All Task] ++++++++++++++++++++++";
    ss << std::left << std::setw(6) << std::right << ps.all_tasks.size();
    ss << " tasks ++++++++++++++++++++++++++++++" << std::endl;
    ss << "c [Q] ";
    for (unsigned i = 0; i < ps.all_tasks.size(); i++)
    {
        ss << std::right << std::setw(3) << i;
        ss << ":";
        std::stringstream tmp;
        tmp << *ps.all_tasks[i];
        ss << std::left << std::setw(60) << tmp.str();
        if (i % line_ct == line_ct - 1)
            ss << "\nc [Q] ";
    }

    {
        std::lock_guard<std::shared_mutex> lock(_prt_mtx);
        os << ss.str() << "\n" << std::flush;
    }

    return os;
}

fastLEC::PartitionSAT::PartitionSAT(std::shared_ptr<fastLEC::XAG> xag,
                                    unsigned n_threads)
    : xag(xag), n_threads(n_threads), stop(false),
      timeout_thread_running(false), states_updated(true),
      all_task_terminated(false)
{
    root_cnf = xag->construct_cnf_from_this_xag();
    root_cnf->build_watches();

    all_task_terminated.store(false);

    solvers.resize(n_threads, nullptr);

    running_cpu_cnt.store(0);

    std::shared_ptr<Task> root_task = std::make_shared<Task>();
    root_task->id = ID_ROOT;
    root_task->father = ID_NONE;
    root_task->level = 0;
    root_task->new_cube_lit_cnt = 0;
    submit_task(root_task);

    cpu_task_ids.resize(n_threads, ID_NONE);

    mutexes.reserve(n_threads);
    for (unsigned i = 0; i < n_threads; ++i)
        mutexes.emplace_back(std::make_unique<std::mutex>());

    for (unsigned i = 0; i < n_threads; ++i)
        workers.emplace_back(&PartitionSAT::worker_func, this, i);

    // start timeout monitor thread
    timeout_thread_running.store(true);
    timeout_thread = std::thread(&PartitionSAT::timeout_monitor_func, this);
}

fastLEC::PartitionSAT::~PartitionSAT()
{
    stop.store(true);

    // stop timeout monitor thread
    timeout_thread_running.store(false);
    if (timeout_thread.joinable())
    {
        timeout_thread.join();
    }

    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

fastLEC::ret_vals fastLEC::PartitionSAT::prop_task_status()
{
    int tid;
    while (q_prop_ids.try_pop(tid))
    {
        std::shared_ptr<Task> t = get_task_by_id(tid);
        if (t->is_sat())
        {
#ifdef PRT_SOLVING_INFO
            {
                std::lock_guard<std::shared_mutex> lock(_prt_mtx);
                std::cout << "c         [>] T" << t->id << " -> SAT"
                          << std::endl;
                std::cout << std::flush;
            }
#endif
            terminate_all_tasks();
            return ret_vals::ret_SAT;
        }
        else if (t->is_unsat())
        {
            if (t->is_root())
            {
#ifdef PRT_SOLVING_INFO
                {
                    std::lock_guard<std::shared_mutex> lock(_prt_mtx);
                    std::cout << "c         [>] T" << t->id << " -> UNSAT"
                              << std::endl;
                    std::cout << std::flush;
                }
#endif
                terminate_all_tasks();
                return ret_vals::ret_UNS;
            }

            std::vector<int> propagated;

            // propagate father;
            if (t->father != ID_NONE && !all_tasks[t->father]->is_solved())
            {
                for (auto brothers : all_tasks[t->father]->sons)
                {
                    bool all_brothers_unsat = true;
                    for (auto &bro : brothers)
                    {
                        if (!all_tasks[bro]->is_unsat())
                        {
                            all_brothers_unsat = false;
                            break;
                        }
                    }
                    if (all_brothers_unsat)
                    {
                        propagated.emplace_back(t->father);
                        all_tasks[t->father]->set_state(
                            task_states::UNSATISFIABLE);
                        terminate_task_by_id(t->father);
                    }
                }
            }

            // propagate sons;
            for (auto &brothers : t->sons)
            {
                for (auto &son : brothers)
                {
                    if (!all_tasks[son]->is_solved())
                    {
                        propagated.emplace_back(son);
                        all_tasks[son]->set_state(task_states::UNSATISFIABLE);
                        terminate_task_by_id(son);
                    }
                }
            }
#ifdef PRT_SOLVING_INFO
            if (propagated.size() > 0)
            {
                std::stringstream ss;
                ss << "c         [>] T" << t->id << " -> ";
                for (auto &i : propagated)
                    ss << "T" << i << " ";
                {
                    std::lock_guard<std::shared_mutex> lock(_prt_mtx);
                    std::cout << ss.str() << std::endl;
                    std::cout << std::flush;
                }
            }
#endif
        }
        // else if (t->paused.load())
        // {
        //     results.emplace(std::move(t));
        // }
    }

    return ret_vals::ret_UNK;
}

void fastLEC::PartitionSAT::worker_func(int cpu_id)
{
    int free_cnt = 0;
    while (!stop.load())
    {
        int id = ID_NONE;

        if (!q_wait_ids.try_pop(id))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            free_cnt++;
            if (free_cnt > 100)
            {
                free_cnt = 0;

                if (split_mutex.try_lock())
                {
                    std::shared_ptr<Task> task = pick_split_task();

                    if (Param::get().custom_params.vis)
                    {
                        Visualizer vis(this->xag);
                        vis.visualize(task->cube);
                    }

                    if (task)
                    {
                        int r = split_task_and_submit(task);

                        if (r == ret_father_solved)
                            terminate_task_by_id(task->id);
                    }
                    split_mutex.unlock();
                }
            }
            continue;
        }

        std::shared_ptr<Task> task = get_task_by_id(id);

        task->cpu = cpu_id;
        task->start_time = fastLEC::ResMgr::get().get_runtime();

        {
            std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
            cpu_task_ids[cpu_id] = task->id;
        }

        task->set_state(ADDING);

        int result = 0;

        assert(solvers[cpu_id] == nullptr);
        solvers[cpu_id] =
            std::shared_ptr<kissat>(kissat_init(), kissat_release);

        int start_pos = 0, end_pos = 0;
        for (int i = 0; i < root_cnf->num_clauses(); i++)
        {
            end_pos = root_cnf->cls_end_pos[i];
            for (int j = start_pos; j < end_pos; j++)
            {
                kissat_add(solvers[cpu_id].get(), root_cnf->lits[j]);
            }
            kissat_add(solvers[cpu_id].get(), 0);
            start_pos = end_pos;
        }

        for (int l : task->cube)
        {
            kissat_add(solvers[cpu_id].get(), l);
            kissat_add(solvers[cpu_id].get(), 0);
        }

        if (!stop.load())
        {
            task->set_state(RUNNING);
            running_cpu_cnt++;

#ifdef PRT_SOLVING_INFO
            {
                std::lock_guard<std::shared_mutex> lock(_prt_mtx);
                std::cout << "c  [+] T" << task->id << " on cpu" << cpu_id
                          << std::endl;
                std::cout << std::flush;
            }
#endif

            result = kissat_solve(solvers[cpu_id].get());

            if (result == 0)
            {
                if (!task->is_solved())
                    task->set_state(UNKNOWN);
            }
            else
            {
                q_prop_ids.emplace(std::move(id));

                if (result == 10)
                    task->set_state(SATISFIABLE);
                else if (result == 20)
                    task->set_state(UNSATISFIABLE);
                else if (!task->is_solved())
                    task->set_state(UNKNOWN);
            }

            if (task->id == ID_ROOT && task->is_solved())
                terminate_all_tasks();

            running_cpu_cnt--;
#ifdef PRT_SOLVING_INFO

            std::stringstream ss;
            states_updated.store(true);
            ss << "c      [-] T" << task->id << ", ";
            if (task->is_sat())
                ss << "sat";
            else if (task->is_unsat())
                ss << "unsat";
            else
                ss << "unknown";
            ss << std::endl;
            std::cout << std::flush;

            {
                std::lock_guard<std::shared_mutex> lock(_prt_mtx);
                std::cout << ss.str();
            }
#endif
        }
        task->terminate_info_upd();
        {
            std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
            cpu_task_ids[cpu_id] = ID_NONE;
        }

        fastLEC::ret_vals ret = ret_vals::ret_UNK;
        ret = prop_task_status();
        if (ret == ret_vals::ret_SAT || ret == ret_vals::ret_UNS)
            break;
    }
}

void fastLEC::PartitionSAT::timeout_monitor_func()
{

    double prt_time_interval =
        Param::get().custom_params.prt_cpu_t_interval; // seconds
    int prt_alltask_interval =
        Param::get().custom_params.prt_alltask_interval; // task number interval
    double last_prt_time = 0;
    int last_task_num = -prt_alltask_interval - 1;
    while (timeout_thread_running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

#ifdef PRT_SOLVING_INFO
        if (fastLEC::ResMgr::get().get_runtime() - last_prt_time >
                prt_time_interval &&
            states_updated.load())
        {
            last_prt_time = fastLEC::ResMgr::get().get_runtime();
            {
                show_pool();
                show_unsolved_tasks();
            }

            if ((int)this->all_tasks.size() - last_task_num >
                prt_alltask_interval)
            {
                last_task_num = this->all_tasks.size();
                std::cout << *this;
            }

            states_updated.store(false);
        }
#endif

        if (stop.load())
            break;

        if (global_solved_for_PPE.load())
        {
            if (fastLEC::Param::get().verbose > 1)
            {
                printf("c [pSAT] PPE solved, terminating all tasks\n");
                fflush(stdout);
            }

            terminate_all_tasks();
            break;
        }
        if (fastLEC::ResMgr::get().get_runtime() >
            fastLEC::Param::get().timeout)
        {
            if (fastLEC::Param::get().verbose > 1)
            {
                printf(
                    "c [pSAT] Timeout reached (%.2fs), terminating all tasks\n",
                    fastLEC::Param::get().timeout);
                fflush(stdout);
            }
            terminate_all_tasks();
            break;
        }
    }
}

std::shared_ptr<fastLEC::Task>
fastLEC::PartitionSAT::get_task_by_cpu(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= static_cast<int>(n_threads))
        return nullptr;

    std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
    return get_task_by_id(cpu_task_ids[cpu_id]);
}

std::shared_ptr<fastLEC::Task>
fastLEC::PartitionSAT::get_task_by_id(int task_id)
{
    if (task_id < 0 || task_id > (int)all_tasks.size())
        return nullptr;
    return all_tasks[task_id];
}

bool fastLEC::PartitionSAT::propagate_task(
    std::shared_ptr<Task> task,
    std::vector<bool> &forbidden_vars,
    std::vector<int> &new_decision_lits,
    std::vector<int> &new_propagated_lits)
{
    std::vector<bool> assign, val;

    if (!task->is_propagated)
    {
        bool res = root_cnf->perform_bcp(assign,
                                         val,
                                         task->cube,
                                         task->propagated_lits,
                                         {},
                                         new_propagated_lits);
        task->is_propagated = true;
        if (res == false)
            return false;
    }

    bool res = root_cnf->perform_bcp(forbidden_vars,
                                     val,
                                     task->cube,
                                     task->propagated_lits,
                                     new_decision_lits,
                                     new_propagated_lits);

    if (res == false)
        return false;

    return true;
}

int fastLEC::PartitionSAT::submit_task(std::shared_ptr<Task> task)
{
    if (task)
    {
        task->create_time = fastLEC::ResMgr::get().get_runtime();
        int task_id = task->id = (int)all_tasks.size();
        all_tasks.push_back(std::move(task));
        q_wait_ids.emplace(std::move(task_id));
        return task_id;
    }
    return ID_NONE;
}

std::shared_ptr<kissat> fastLEC::PartitionSAT::get_solver_by_cpu(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= static_cast<int>(n_threads))
    {
        return nullptr;
    }
    return solvers[cpu_id];
}

void fastLEC::PartitionSAT::terminate_task_by_id(int task_id)
{

    std::shared_ptr<Task> task = get_task_by_id(task_id);
    if (task == nullptr || task->cpu == CPU_NONE)
        return;

    std::lock_guard<std::mutex> lock(*mutexes[task->cpu]);
    if (cpu_task_ids[task->cpu] == task_id)
    {
        std::shared_ptr<kissat> p = solvers[task->cpu];
        if (p)
            kissat_terminate(p.get());
        cpu_task_ids[task->cpu] = ID_NONE;
    }
}

void fastLEC::PartitionSAT::terminate_task_by_cpu(int cpu_id)
{
    std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
    if (cpu_task_ids[cpu_id] != ID_NONE)
    {
        std::shared_ptr<kissat> p = solvers[cpu_id];
        if (p)
            kissat_terminate(p.get());
        cpu_task_ids[cpu_id] = ID_NONE;
    }
}

void fastLEC::PartitionSAT::terminate_all_tasks()
{
    stop.store(true);

    if (all_task_terminated.load())
        return;

    all_task_terminated.store(true);

    for (unsigned i = 0; i < n_threads; i++)
        terminate_task_by_cpu(i);

    for (auto &t : all_tasks)
        t->terminate_info_upd();
}

bool fastLEC::PartitionSAT::check_repeat(std::vector<int> &cube_var) const
{
    int cnt = 1;
    std::vector<int> mark(root_cnf->num_vars + 1, 0);
    for (auto &task : all_tasks)
    {
        // check cube_var in task->cube
        if (cube_var.size() > task->cube.size())
            continue;

        for (int l : task->cube)
            mark[abs(l)] = cnt;

        bool is_subset = true;
        for (int v : cube_var)
        {
            if (mark[v] != cnt)
            {
                is_subset = false;
                break;
            }
        }
        if (is_subset)
            return true;
        cnt++;
    }

    return false;
}

int fastLEC::PartitionSAT::split_task_and_submit(
    std::shared_ptr<fastLEC::Task> father)
{
    std::vector<int> split_vars = pick_split_vars(father);
    if (split_vars.size() == 1 && split_vars[0] == 0) // UNSAT: {0}
        return ret_father_solved;

    if (father->is_solved())
        return ret_father_solved;

    if (split_vars.size() == 0)
        return ret_cannot_split;

    int max_split_num = Param::get().custom_params.gt_max_split;
    if (father->split_ct() >= (unsigned)max_split_num)
        return ret_max_split;

    if (q_wait_ids.size() > 0)
        return ret_pool_has_tasks;

    std::vector<int> split_vars_tmp = split_vars;
    for (int l : father->cube)
        split_vars_tmp.push_back(abs(l));
    bool is_repeat = check_repeat(split_vars_tmp);
    if (is_repeat)
        return ret_repeat_task;

    std::vector<std::shared_ptr<fastLEC::Task>> sons;
    std::vector<int> sons_ids;
    for (unsigned cnt = 0; cnt < (1ul << split_vars.size()); cnt++)
    {
        std::shared_ptr<fastLEC::Task> new_task =
            std::make_shared<fastLEC::Task>();

        new_task->father = father->id;
        new_task->level = father->level + 1;
        new_task->cube = father->cube;
        for (unsigned i = 0; i < split_vars.size(); i++)
        {
            if (cnt & (1 << i))
                new_task->cube.push_back(split_vars[i]);
            else
                new_task->cube.push_back(-split_vars[i]);
        }
        new_task->new_cube_lit_cnt = split_vars.size();

        sons.push_back(new_task);
        // Note: new_task->id will be set in submit_task, so we can't
        // use it here sons_ids will be populated after submit_task
        // calls

        if (father->is_solved())
            break;
    }

    if (father->split_ct() >= (unsigned)max_split_num)
        return ret_max_split;

    if (!father->is_solved())
    {
        // Submit all tasks and collect their IDs
        for (unsigned i = 0; i < sons.size(); i++)
        {
            int task_id = submit_task(std::move(sons[i]));
            // The task ID is set in submit_task, so we need to get it
            // from the last added task
            sons_ids.push_back(task_id);
        }
        father->sons.push_back(sons_ids);
#ifdef PRT_SOLVING_INFO
        std::stringstream ss;
        ss << "c    [;] T" << father->id << " -s> ";
        for (unsigned i = 0; i < split_vars.size(); i++)
            ss << "v" << split_vars[i] << " ";
        ss << "{";
        for (unsigned i = 0; i < sons_ids.size(); i++)
        {
            ss << "T" << sons_ids[i];
            if (i < sons_ids.size() - 1)
                ss << " ";
        }
        ss << "}";

        {
            std::lock_guard<std::shared_mutex> lock(_prt_mtx);
            std::cout << ss.str() << std::endl;
            std::cout << std::flush;
        }
#endif

        return ret_success_split;
    }
    else
        return ret_father_solved;
}

bool fastLEC::PartitionSAT::compute_mask(std::shared_ptr<Task> task,
                                         std::vector<bool> &mask)
{

    mask.resize(root_cnf->num_vars + 1, false);
    std::vector<int> nd, np;
    bool res = propagate_task(task, mask, nd, np);

    return res;
}

std::vector<int>
fastLEC::PartitionSAT::pick_split_vars(std::shared_ptr<fastLEC::Task> father)
{
    std::vector<int> propagated_lits = father->cube;

    std::vector<bool> mask(root_cnf->num_vars + 1, false);

    bool res = compute_mask(father, mask);

    if (!res)
        return std::vector<int>({0});

    std::vector<double> scores;
    compute_scores(mask, scores);

    std::vector<int> candidates_vars;

    for (int i = 1; i <= root_cnf->num_vars; i++)
    {
        if (scores[i] <= 0.0)
            continue;

        if (!mask[i])
            candidates_vars.emplace_back(i);
    }

    std::sort(candidates_vars.begin(),
              candidates_vars.end(),
              [&](int x, int y)
              {
                  return scores[x] > scores[y];
              });

    int num_swaps = candidates_vars.size() * 0.1;
    for (int i = 0; i < num_swaps; ++i)
    {
        int pos = fastLEC::ResMgr::get().random_uint64() %
            (candidates_vars.size() - 1);
        std::swap(candidates_vars[pos], candidates_vars[pos + 1]);
    }

    unsigned pick_var_num = this->decide_split_var_num();

    std::vector<int> pick_vars;
    for (unsigned i = 0; pick_vars.size() < (unsigned)pick_var_num &&
         i < candidates_vars.size();
         i++)
    {
        int pick_aig_v = candidates_vars[i];
        pick_vars.push_back(pick_aig_v);
    }

    if (pick_vars.size() == 0)
        return std::vector<int>({0});

    return pick_vars;
}

fastLEC::ret_vals fastLEC::PartitionSAT::check()
{

    double start_time = fastLEC::ResMgr::get().get_runtime();
    ret_vals ret = ret_vals::ret_UNK;

    for (auto &worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }

    if (all_tasks[0]->state == SATISFIABLE)
        ret = ret_vals::ret_SAT;
    else if (all_tasks[0]->state == UNSATISFIABLE)
        ret = ret_vals::ret_UNS;
    else
        ret = ret_vals::ret_UNK;

#ifdef PRT_SOLVING_INFO
    {
        // std::cout << *this;
        show_detailed_tasks();
    }
#endif

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [pSAT] result = %d [var = %d, clause = %d, lit = %d]"
               "[time = %.2f]\n",
               ret,
               this->root_cnf->num_vars,
               this->root_cnf->num_clauses(),
               this->root_cnf->num_lits(),
               fastLEC::ResMgr::get().get_runtime() - start_time);
        fflush(stdout);
    }

    return ret;
}