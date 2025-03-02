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
#include "absl/container/node_hash_map.h"
#include "cluster/partition_manager.h"
#include "cluster/scheduling/leader_balancer_probe.h"
#include "cluster/scheduling/leader_balancer_strategy.h"
#include "cluster/types.h"
#include "raft/consensus.h"
#include "raft/consensus_client_protocol.h"
#include "seastarx.h"

#include <seastar/core/gate.hh>
#include <seastar/core/timer.hh>

using namespace std::chrono_literals;

namespace cluster {

/**
 * When a node crashes / restarts the most likely result is that it loses
 * leadership for all partitions with replicas assigned to it. The leadership
 * rebalancer will then react by treating this as spare capacity, and attempting
 * to move leadership back. If the node takes a while to boot up then these
 * movements will fail and the groups being moved with be temporarily muted from
 * further movement. Thus the mute timeout should be chosen with this in mind:
 * too long, and leadership recovery will take a long time. Too short and it
 * will be very noise with timeouts and RPC errros.
 *
 * TODO:
 *   - One approach to dealing with such situations in a smarter way might be to
 *   attempt to gauge the health of a node (e.g. tapping into rpc connection
 *   info, observing repeated failures, or pinging a node's health endpoint).
 *   This information could then be used reduce failed movement requests and
 *   improve responsiveness.
 *
 *   - Ultimately a more principled approach may be better: computing and
 *   disseminating a global schedule, a distributed choice-of-two strategy,
 *   push-pull p2p balancing. There are many options that are better than this
 *   centralized greedy approach :)
 */
class leader_balancer {
    using clock_type = ss::lowres_clock;

    /*
     * after becoming the raft0 leader apply a delay before the rebalancer
     * starts in order to give the system some time to stabilize.
     */
    static constexpr clock_type::duration leader_activation_delay = 30s;

    /*
     * normally leadership transfer should be fast. if a timeout occurs the
     * group will be retried again after other group rebalances are tried.
     */
    static constexpr clock_type::duration leader_transfer_rpc_timeout = 30s;

public:
    leader_balancer(
      topic_table&,
      partition_leaders_table&,
      raft::consensus_client_protocol,
      ss::sharded<shard_table>&,
      ss::sharded<partition_manager>&,
      ss::sharded<raft::group_manager>&,
      ss::sharded<ss::abort_source>&,
      config::binding<bool>&&,
      config::binding<std::chrono::milliseconds>&&,
      config::binding<std::chrono::milliseconds>&&,
      config::binding<std::chrono::milliseconds>&&,
      consensus_ptr);

    ss::future<> start();
    ss::future<> stop();

private:
    using index_type = leader_balancer_strategy::index_type;
    using reassignment = leader_balancer_strategy::reassignment;

    index_type build_index();
    std::optional<model::broker_shard> find_leader_shard(const model::ntp&);
    absl::flat_hash_set<raft::group_id> muted_groups() const;
    absl::flat_hash_set<model::node_id> muted_nodes() const;

    ss::future<bool> do_transfer(reassignment);
    ss::future<bool> do_transfer_local(reassignment) const;
    ss::future<bool> do_transfer_remote(reassignment);

    void on_enable_changed();

    void trigger_balance();
    ss::future<ss::stop_iteration> balance();

    // On/off switch: when off, leader balancer tick will run
    // but do nothing
    config::binding<bool> _enabled;

    /*
     * the balancer will go idle in different scenarios such as losing raft0
     * leadership, or when leadership balance cannot be improved.  for good
     * responsivenss, sub-system upcalls may wake-up the balancer when
     * leadership is regained or some threshold set of leadership change is
     * identified. as a defensive measure, we set an idle timeout to run a
     * balancing tick at low frequency in case some upcall is missed.
     *
     * TODO:
     *   - raft0 leadership upcall is active, but we require polling to wake-up
     *   the balancer when it has gone idel because balancing completed. for
     *   this we need ot add an upcall notification mechanism to the leaders
     *   table / dissemination framework.
     *
     *      See: https://github.com/redpanda-data/redpanda/issues/2031
     *
     *   Once this item is complete it would also make sense to increase this
     *   timeout to something larger like 5 minutes.
     */
    config::binding<std::chrono::milliseconds> _idle_timeout;

    /*
     * timeout used to mute groups. groups are muted in various scenarios such
     * as if they experience errors being moved, but also if they are moved
     * successfully so that we do not pertrub them too much on accident.
     */
    config::binding<std::chrono::milliseconds> _mute_timeout;

    /*
     * threshold timeout of not having a raft0 heartbeat after which the node
     * will be muted. muting a node removes its capacity from consideration when
     * choosing new rebalancing targets for leadership.
     */
    config::binding<std::chrono::milliseconds> _node_mute_timeout;

    struct last_known_leader {
        model::broker_shard shard;
        clock_type::time_point expires;
    };
    absl::btree_map<raft::group_id, last_known_leader> _last_leader;

    leader_balancer_probe _probe;
    bool _need_controller_refresh{true};
    absl::btree_map<raft::group_id, clock_type::time_point> _muted;
    cluster::notification_id_type _leader_notify_handle;
    topic_table& _topics;
    partition_leaders_table& _leaders;
    raft::consensus_client_protocol _client;
    ss::sharded<shard_table>& _shard_table;
    ss::sharded<partition_manager>& _partition_manager;
    ss::sharded<raft::group_manager>& _group_manager;
    ss::sharded<ss::abort_source>& _as;
    consensus_ptr _raft0;
    ss::gate _gate;
    ss::timer<clock_type> _timer;
};

} // namespace cluster
