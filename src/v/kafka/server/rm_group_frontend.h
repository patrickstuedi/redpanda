/*
 * Copyright 2020 Vectorized, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once

#include "cluster/rm_group_proxy.h"
#include "cluster/types.h"
#include "kafka/protocol/errors.h"
#include "kafka/server/fwd.h"
#include "kafka/types.h"
#include "model/metadata.h"
#include "rpc/fwd.h"
#include "seastarx.h"

namespace kafka {

ss::future<bool> try_create_consumer_group_topic(
  kafka::coordinator_ntp_mapper& mapper,
  cluster::topics_frontend& topics_frontend);

class rm_group_frontend {
public:
    rm_group_frontend(
      ss::sharded<cluster::metadata_cache>&,
      ss::sharded<rpc::connection_cache>&,
      ss::sharded<cluster::partition_leaders_table>&,
      cluster::controller*,
      ss::sharded<kafka::group_router>&);

    ss::future<cluster::begin_group_tx_reply> begin_group_tx(
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::begin_group_tx_reply>
      begin_group_tx_locally(cluster::begin_group_tx_request);
    ss::future<cluster::prepare_group_tx_reply> prepare_group_tx(
      kafka::group_id,
      model::term_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::prepare_group_tx_reply>
      prepare_group_tx_locally(cluster::prepare_group_tx_request);
    ss::future<cluster::commit_group_tx_reply> commit_group_tx(
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::commit_group_tx_reply>
      commit_group_tx_locally(cluster::commit_group_tx_request);
    ss::future<cluster::abort_group_tx_reply> abort_group_tx(
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::abort_group_tx_reply>
      abort_group_tx_locally(cluster::abort_group_tx_request);

private:
    ss::sharded<cluster::metadata_cache>& _metadata_cache;
    ss::sharded<rpc::connection_cache>& _connection_cache;
    ss::sharded<cluster::partition_leaders_table>& _leaders;
    cluster::controller* _controller;
    ss::sharded<kafka::group_router>& _group_router;
    int16_t _metadata_dissemination_retries;
    std::chrono::milliseconds _metadata_dissemination_retry_delay_ms;

    ss::future<cluster::begin_group_tx_reply> dispatch_begin_group_tx(
      model::node_id,
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::prepare_group_tx_reply> dispatch_prepare_group_tx(
      model::node_id,
      kafka::group_id,
      model::term_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::commit_group_tx_reply> dispatch_commit_group_tx(
      model::node_id,
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);
    ss::future<cluster::abort_group_tx_reply> dispatch_abort_group_tx(
      model::node_id,
      kafka::group_id,
      model::producer_identity,
      model::tx_seq,
      model::timeout_clock::duration);

    friend cluster::tx_gateway;
};

class rm_group_proxy_impl final : public cluster::rm_group_proxy {
public:
    rm_group_proxy_impl(ss::sharded<rm_group_frontend>& target)
      : _target(target) {}

    ss::future<cluster::begin_group_tx_reply> begin_group_tx(
      kafka::group_id group_id,
      model::producer_identity pid,
      model::tx_seq tx_seq,
      model::timeout_clock::duration timeout) override {
        return _target.local().begin_group_tx(group_id, pid, tx_seq, timeout);
    }

    ss::future<cluster::begin_group_tx_reply>
    begin_group_tx_locally(cluster::begin_group_tx_request req) override {
        return _target.local().begin_group_tx_locally(std::move(req));
    }

    ss::future<cluster::prepare_group_tx_reply> prepare_group_tx(
      kafka::group_id group_id,
      model::term_id etag,
      model::producer_identity pid,
      model::tx_seq tx_seq,
      model::timeout_clock::duration timeout) override {
        return _target.local().prepare_group_tx(
          group_id, etag, pid, tx_seq, timeout);
    }

    ss::future<cluster::prepare_group_tx_reply>
    prepare_group_tx_locally(cluster::prepare_group_tx_request req) override {
        return _target.local().prepare_group_tx_locally(std::move(req));
    }

    ss::future<cluster::commit_group_tx_reply> commit_group_tx(
      kafka::group_id group_id,
      model::producer_identity pid,
      model::tx_seq tx_seq,
      model::timeout_clock::duration timeout) override {
        return _target.local().commit_group_tx(group_id, pid, tx_seq, timeout);
    }

    ss::future<cluster::commit_group_tx_reply>
    commit_group_tx_locally(cluster::commit_group_tx_request req) override {
        return _target.local().commit_group_tx_locally(std::move(req));
    }

    ss::future<cluster::abort_group_tx_reply> abort_group_tx(
      kafka::group_id group_id,
      model::producer_identity pid,
      model::tx_seq tx_seq,
      model::timeout_clock::duration timeout) override {
        return _target.local().abort_group_tx(group_id, pid, tx_seq, timeout);
    }

    ss::future<cluster::abort_group_tx_reply>
    abort_group_tx_locally(cluster::abort_group_tx_request req) override {
        return _target.local().abort_group_tx_locally(std::move(req));
    }

private:
    ss::sharded<rm_group_frontend>& _target;
};

} // namespace kafka
