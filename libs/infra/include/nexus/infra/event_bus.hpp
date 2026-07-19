#pragma once
#include "nexus/domain/change_event.hpp"
#include <functional>

namespace nexus::infra {

// MVP: synchronous, in-process delivery to a single handler (DispatchService).
// Satisfies ports::EventPublisher. Phase 2 swap: post to a per-table strand
// instead of calling inline — InsertService's dependency on this type does
// not change.
class EventBus {
public:
    void set_handler(std::function<void(const domain::ChangeEvent&)> handler) {
        handler_ = std::move(handler);
    }

    void publish(const domain::ChangeEvent& event) {
        if (handler_) handler_(event);
    }

private:
    std::function<void(const domain::ChangeEvent&)> handler_;
};

} // namespace nexus::infra
