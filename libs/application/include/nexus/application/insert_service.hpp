#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/error.hpp"
#include "nexus/ports/audit_writer.hpp"
#include "nexus/ports/event_publisher.hpp"
#include "nexus/ports/snapshot_writer.hpp"

namespace nexus::application {

// Ordering matters: write, then audit, then publish. Publishing before
// the write lands would let a subscriber observe a change that isn't
// visible to a concurrent select yet.
template <ports::SnapshotWriter Writer, ports::AuditWriter Auditor,
          ports::EventPublisher Publisher>
class InsertService {
public:
    InsertService(Writer& writer, Auditor& auditor, Publisher& publisher)
        : writer_(writer), auditor_(auditor), publisher_(publisher) {}

    domain::Expected<void> execute(const domain::TableName& table, domain::RowPtr row) {
        if (auto result = writer_.upsert(table, row); !result) {
            return std::unexpected(result.error());
        }

        auditor_.append(table, row);

        publisher_.publish(domain::ChangeEvent{
            .table = table,
            .row = row,
            .kind = domain::ChangeKind::Inserted,
        });

        return {};
    }

private:
    Writer& writer_;
    Auditor& auditor_;
    Publisher& publisher_;
};

} // namespace nexus::application
