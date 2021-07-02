#ifndef CONCURRENCPP_CONSUMER_CONTEXT_H
#define CONCURRENCPP_CONSUMER_CONTEXT_H

#include "concurrencpp/coroutines/coroutine.h"
#include "concurrencpp/results/result_fwd_declerations.h"

#include <mutex>
#include <condition_variable>

namespace concurrencpp::details {
    class await_context {

       private:
        coroutine_handle<void> m_caller_handle;
        std::exception_ptr m_interrupt_exception;

       public:
        void resume() noexcept;

        void set_coro_handle(coroutine_handle<void> coro_handle) noexcept;
        void set_interrupt(const std::exception_ptr& interrupt) noexcept;

        void throw_if_interrupted() const;
    };

    class await_via_functor {

       private:
        await_context* m_ctx;

       public:
        await_via_functor(await_context* ctx) noexcept;
        await_via_functor(await_via_functor&& rhs) noexcept;
        ~await_via_functor() noexcept;

        void operator()() noexcept;
    };

    class wait_context {

       private:
        std::mutex m_lock;
        std::condition_variable m_condition;
        bool m_ready = false;

       public:
        void wait() noexcept;
        bool wait_for(size_t milliseconds) noexcept;

        void notify() noexcept;
    };

    class when_all_state_base {

       protected:
        std::atomic_size_t m_counter;
        std::recursive_mutex m_lock;

       public:
        virtual ~when_all_state_base() noexcept = default;
        virtual void on_result_ready() noexcept = 0;
    };

    class when_any_state_base {

       protected:
        std::atomic_bool m_fulfilled = false;
        std::recursive_mutex m_lock;

       public:
        virtual ~when_any_state_base() noexcept = default;
        virtual void on_result_ready(size_t) noexcept = 0;
    };

    class when_any_context {

       private:
        std::shared_ptr<when_any_state_base> m_when_any_state;
        size_t m_index;

       public:
        when_any_context(const std::shared_ptr<when_any_state_base>& when_any_state, size_t index) noexcept;
        when_any_context(const when_any_context&) noexcept = default;

        void operator()() const noexcept;
    };

    class consumer_context {

       private:
        enum class consumer_status { idle, await, wait, when_all, when_any };

        union storage {
            coroutine_handle<void> caller_handle;
            std::shared_ptr<wait_context> wait_ctx;
            std::shared_ptr<when_all_state_base> when_all_ctx;
            when_any_context when_any_ctx;

            template<class type, class... argument_type>
            static void build(type& o, argument_type&&... arguments) noexcept {
                new (std::addressof(o)) type(std::forward<argument_type>(arguments)...);
            }

            template<class type>
            static void destroy(type& o) noexcept {
                o.~type();
            }

            storage() noexcept {}
            ~storage() noexcept {}
        };

       private:
        consumer_status m_status = consumer_status::idle;
        storage m_storage;

       public:
        ~consumer_context() noexcept;

        void clear() noexcept;
        void resume_consumer() const noexcept;

        void set_await_handle(coroutine_handle<void> caller_handle) noexcept;
        void set_wait_context(const std::shared_ptr<wait_context>& wait_ctx) noexcept;
        void set_when_all_context(const std::shared_ptr<when_all_state_base>& when_all_state) noexcept;
        void set_when_any_context(const std::shared_ptr<when_any_state_base>& when_any_ctx, size_t index) noexcept;
    };
}  // namespace concurrencpp::details

#endif
