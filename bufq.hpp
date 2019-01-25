#ifndef BUF_Q_HPP
#define BUF_Q_HPP
#include <cassert>
#include <iostream>
#include <memory>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <initializer_list>
#include <atomic>
template <typename T> class BufQ
{
  public:
    std::list<std::shared_ptr<T>> filled_q;
    std::list<std::shared_ptr<T>> unfilled_q;
    std::shared_ptr<T> proc_buf = nullptr;
    std::mutex mx;
    std::condition_variable cv;
    std::atomic_bool waiting;

  public:
    BufQ (const std::initializer_list<std::shared_ptr<T>> &data)
    : unfilled_q{ data }, waiting (false)
    {
    }

    BufQ (const std::vector<std::shared_ptr<T>> &data) : waiting (false)
    {
        for (auto &i : data)
            {
                unfilled_q.push_back (i);
            }
    }

    BufQ (const BufQ &) = delete;

  public:
    std::shared_ptr<T> fetch ()
    {
        using namespace std::chrono_literals;
        waiting.store (true);
        std::unique_lock<std::mutex> lk (mx);
        cv.wait (lk, [&, this] { return !this->filled_q.empty (); });
        waiting.store (false);
        if (proc_buf != nullptr)
            {
                unfilled_q.push_back (proc_buf);
            }
        proc_buf = filled_q.front ();
        filled_q.pop_front ();

        return proc_buf;
    }

    template <typename F> void write (F &&func)
    {
        auto p = prepare_write_buf ();

        func (p);

        submit ();
    }

    std::shared_ptr<T> prepare_write_buf ()
    {
        while (waiting.load () && !filled_q.empty ())
            {
                cv.notify_all ();
            }
        {
            std::lock_guard<std::mutex> lk (mx);

            // std::cout << "entering write" << std::endl;

            if (unfilled_q.empty ())
                {
                    unfilled_q.push_back (filled_q.front ());
                    filled_q.pop_front ();
                }
        }
        assert (!unfilled_q.empty ());
        return unfilled_q.front ();
    }

    void submit ()
    {
        {
            std::lock_guard<std::mutex> lock (mx);
            filled_q.push_back (unfilled_q.front ());
            unfilled_q.pop_front ();
        }
        cv.notify_all ();
    }
};


#endif