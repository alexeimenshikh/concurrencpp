#include "concurrencpp/results/impl/result_state.h"
#include "concurrencpp/results/impl/shared_result_state.h"

using concurrencpp::details::result_state_base;

void result_state_base::assert_done() const noexcept {
    assert(m_pc_state.load(std::memory_order_relaxed) == pc_state::producer_done);
}

void result_state_base::wait() {
    const auto state = m_pc_state.load(std::memory_order_acquire);
    if (state == pc_state::producer_done) {
        return;
    }

    auto wait_ctx = std::make_shared<wait_context>();
    m_consumer.set_wait_context(wait_ctx);

    auto expected_state = pc_state::idle;
    const auto idle = m_pc_state.compare_exchange_strong(expected_state, pc_state::consumer_set, std::memory_order_acq_rel);

    if (!idle) {
        assert_done();
        return;
    }

    wait_ctx->wait();
    assert_done();
}

bool result_state_base::await(coroutine_handle<void> caller_handle) noexcept {
    const auto state = m_pc_state.load(std::memory_order_acquire);
    if (state == pc_state::producer_done) {
        return false;  // don't suspend
    }

    m_consumer.set_await_handle(caller_handle);

    auto expected_state = pc_state::idle;
    const auto idle = m_pc_state.compare_exchange_strong(expected_state, pc_state::consumer_set, std::memory_order_acq_rel);

    if (!idle) {
        assert_done();
    }

    return idle;  // if idle = true, suspend
}

void result_state_base::when_all(const std::shared_ptr<when_all_state_base>& when_all_state) noexcept {
    const auto state = m_pc_state.load(std::memory_order_acquire);
    if (state == pc_state::producer_done) {
        return when_all_state->on_result_ready();
    }

    m_consumer.set_when_all_context(when_all_state);

    auto expected_state = pc_state::idle;
    const auto idle = m_pc_state.compare_exchange_strong(expected_state, pc_state::consumer_set, std::memory_order_acq_rel);

    if (idle) {
        return;
    }

    assert_done();
    when_all_state->on_result_ready();
}

concurrencpp::details::when_any_status result_state_base::when_any(const std::shared_ptr<when_any_state_base>& when_any_state,
                                                                   size_t index) noexcept {
    const auto state = m_pc_state.load(std::memory_order_acquire);
    if (state == pc_state::producer_done) {
        return when_any_status::result_ready;
    }

    m_consumer.set_when_any_context(when_any_state, index);

    auto expected_state = pc_state::idle;
    const auto idle = m_pc_state.compare_exchange_strong(expected_state, pc_state::consumer_set, std::memory_order_acq_rel);

    if (idle) {
        return when_any_status::set;
    }

    // no need to continue the iteration of when_any, we've found a ready task.
    // tell all predecessors to rewind their state.
    assert_done();
    return when_any_status::result_ready;
}

void result_state_base::try_rewind_consumer() noexcept {
    const auto pc_state = m_pc_state.load(std::memory_order_acquire);
    if (pc_state != pc_state::consumer_set) {
        return;
    }

    auto expected_consumer_state = pc_state::consumer_set;
    const auto consumer = m_pc_state.compare_exchange_strong(expected_consumer_state, pc_state::idle, std::memory_order_acq_rel);

    if (!consumer) {
        assert_done();
        return;
    }

    m_consumer.clear();
}