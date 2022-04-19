#ifndef CONCURRENCPP_COROUTINE_H
#define CONCURRENCPP_COROUTINE_H

#include "../platform_defs.h"

#include <coroutine>

namespace concurrencpp::details {
    template<class promise_type>
    using coroutine_handle = std::coroutine_handle<promise_type>;
    using suspend_never = std::suspend_never;
    using suspend_always = std::suspend_always;
}  // namespace concurrencpp::details

#endif
