#pragma once
#include "nexus/application/dispatch_service.hpp"
#include "nexus/application/insert_service.hpp"
#include "nexus/application/select_service.hpp"
#include "nexus/application/subscribe_service.hpp"
#include "nexus/domain/change_event.hpp"

namespace nexus::app {

// Pulls the five nested types out of whatever traits struct T is and
// wires the four application services from them. This is the ONE
// composition root both nexus_demo and nexus_server are built from —
// they differ only in what they DO with the wired services (a canned
// console demo vs. dispatching real network requests), not in how
// those services get built.
template <typename T>
class AppEngine {
public:
    using Store = T::Store;
    using Log = T::Log;
    using Bus = T::Bus;
    using Registry = T::Registry;
    using Parser = T::Parser;

    using SelectSvc = application::SelectService<Store>;
    using InsertSvc = application::InsertService<Store, Log, Bus>;
    using SubscribeSvc = application::SubscribeService<Registry, Parser>;
    using DispatchSvc = application::DispatchService<Registry>;

    AppEngine()
        : dispatch_(registry_),
          select_(store_),
          insert_(store_, log_, bus_),
          subscribe_(registry_, parser_) {
        bus_.set_handler([this](const domain::ChangeEvent& event) { dispatch_.on_event(event); });
    }

    [[nodiscard]] SelectSvc& select() { return select_; }
    [[nodiscard]] InsertSvc& insert() { return insert_; }
    [[nodiscard]] SubscribeSvc& subscribe() { return subscribe_; }
    [[nodiscard]] const Parser& criteria_parser() const { return parser_; }
    [[nodiscard]] const Log& audit_log() const { return log_; }

private:
    Store store_;
    Log log_;
    Bus bus_;
    Registry registry_;
    Parser parser_;

    DispatchSvc dispatch_;
    SelectSvc select_;
    InsertSvc insert_;
    SubscribeSvc subscribe_;
};

} // namespace nexus::app
