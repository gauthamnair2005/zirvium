//! MOSIX: the Zirvium cluster and process subsystem.
//!
//! MOSIX's defining idea is that a cluster is a single pool of computing
//! resources: processes are created on one node and may migrate to another
//! without changing their identity.  Zirvium models that directly:
//!
//! * every process records both a *home node* and a *current node*;
//! * nodes publish CPU count, memory and a load average;
//! * the scheduler can move a process between nodes when a peer is reachable.
//!
//! This milestone owns a single machine, so the local node is the only online
//! member.  The API is cluster-shaped from the start: a future network driver
//! only has to call [`register_node`] and update load values.

pub mod vfs;

use alloc::string::String;
use alloc::vec::Vec;

use zirvstdlib::abi::{NodeInfo, NodeState, ProcessInfo, ProcessState, set_name};
use zirvstdlib::sync::SpinLock;

const MAX_PROCESSES: usize = 256;

pub struct Cluster {
    nodes: Vec<NodeInfo>,
    processes: Vec<ProcessInfo>,
    next_pid: u32,
    load_permille: u32,
    migrations: u64,
}

static CLUSTER: SpinLock<Option<Cluster>> = SpinLock::new(None);

impl Cluster {
    fn new(cpu_count: u32, memory_mib: u32) -> Self {
        let mut local = NodeInfo::empty();
        local.id = 0;
        local.state = NodeState::Online;
        local.cpu_count = cpu_count.max(1);
        local.memory_mib = memory_mib;
        local.load_permille = 0;
        set_name(&mut local.name, "zirvium-0");

        Self {
            nodes: alloc::vec![local],
            processes: Vec::new(),
            next_pid: 1,
            load_permille: 0,
            migrations: 0,
        }
    }

    fn spawn(&mut self, parent: u32, name: &str, memory_kib: u64, priority: u32) -> u32 {
        if self.processes.len() >= MAX_PROCESSES {
            return 0;
        }
        let pid = self.next_pid;
        self.next_pid += 1;
        let mut process = ProcessInfo::empty();
        process.pid = pid;
        process.parent_pid = parent;
        process.state = if pid == 1 {
            ProcessState::Running
        } else {
            ProcessState::Ready
        };
        process.priority = priority;
        process.memory_kib = memory_kib;
        process.home_node = 0;
        process.current_node = 0;
        set_name(&mut process.name, name);
        self.processes.push(process);
        pid
    }

    fn find(&mut self, pid: u32) -> Option<&mut ProcessInfo> {
        self.processes.iter_mut().find(|process| process.pid == pid)
    }
}

/// Initialise the cluster and create the kernel's own processes.
pub fn init(cpu_count: u32, memory_mib: u32) {
    let mut cluster = Cluster::new(cpu_count, memory_mib);
    cluster.spawn(0, "kernel/idle", 64, 0);
    cluster.spawn(0, "kernel/timer", 128, 90);
    cluster.spawn(0, "kernel/input", 96, 80);
    cluster.spawn(0, "displayjet/compositor", 24 * 1024, 70);
    cluster.spawn(0, "mosix/scheduler", 256, 60);
    cluster.spawn(1, "desktop/shell", 16 * 1024, 50);
    cluster.spawn(0, "vfs/mount", 512, 40);
    let pid = cluster.next_pid;
    *CLUSTER.lock() = Some(cluster);
    crate::sprintln!(
        "MOSIX: local node online, {} processes",
        pid.saturating_sub(1)
    );
}

fn with_cluster<R>(function: impl FnOnce(&mut Cluster) -> R) -> Option<R> {
    let mut lock = CLUSTER.lock();
    lock.as_mut().map(function)
}

/// Create a process on the local node.
pub fn spawn_process(parent: u32, name: &str, memory_kib: u64) -> u32 {
    with_cluster(|cluster| cluster.spawn(parent, name, memory_kib, 50)).unwrap_or(0)
}

/// Remove a process by PID.
pub fn exit_process(pid: u32) -> bool {
    with_cluster(|cluster| {
        let before = cluster.processes.len();
        cluster.processes.retain(|process| process.pid != pid);
        cluster.processes.len() != before
    })
    .unwrap_or(false)
}

/// Change a process's state.
pub fn set_state(pid: u32, state: ProcessState) {
    with_cluster(|cluster| {
        if let Some(process) = cluster.find(pid) {
            process.state = state;
        }
    });
}

/// Account CPU time to a process.
pub fn account(pid: u32, ticks: u64) {
    with_cluster(|cluster| {
        if let Some(process) = cluster.find(pid) {
            process.cpu_ticks = process.cpu_ticks.saturating_add(ticks);
        }
    });
}

/// Report the local CPU load (0..1000).
pub fn set_load(permille: u32) {
    with_cluster(|cluster| {
        cluster.load_permille = permille.min(1000);
        if let Some(node) = cluster.nodes.first_mut() {
            node.load_permille = cluster.load_permille;
            node.state = if permille >= 950 {
                NodeState::Busy
            } else {
                NodeState::Online
            };
        }
    });
}

pub fn load_permille() -> u32 {
    with_cluster(|cluster| cluster.load_permille).unwrap_or(0)
}

/// Snapshot of all processes.
pub fn processes() -> Vec<ProcessInfo> {
    with_cluster(|cluster| cluster.processes.clone()).unwrap_or_default()
}

pub fn process_count() -> usize {
    with_cluster(|cluster| cluster.processes.len()).unwrap_or(0)
}

/// Snapshot of all nodes.
pub fn nodes() -> Vec<NodeInfo> {
    with_cluster(|cluster| cluster.nodes.clone()).unwrap_or_default()
}

pub fn node_count() -> usize {
    with_cluster(|cluster| cluster.nodes.len()).unwrap_or(0)
}

/// Local node descriptor.
pub fn local_node() -> NodeInfo {
    with_cluster(|cluster| cluster.nodes.first().copied())
        .flatten()
        .unwrap_or_else(NodeInfo::empty)
}

/// Announce a peer node.  A network stack will call this when a MOSIX peer
/// answers a discovery probe.
pub fn register_node(name: &str, cpu_count: u32, memory_mib: u32, state: NodeState) -> u32 {
    with_cluster(|cluster| {
        let id = cluster.nodes.len() as u32;
        let mut node = NodeInfo::empty();
        node.id = id;
        node.state = state;
        node.cpu_count = cpu_count.max(1);
        node.memory_mib = memory_mib;
        set_name(&mut node.name, name);
        cluster.nodes.push(node);
        id
    })
    .unwrap_or(0)
}

/// Update a peer's load.
pub fn update_node_load(id: u32, permille: u32) {
    with_cluster(|cluster| {
        if let Some(node) = cluster.nodes.iter_mut().find(|node| node.id == id) {
            node.load_permille = permille.min(1000);
            node.state = if node.state == NodeState::Offline {
                NodeState::Offline
            } else if permille >= 950 {
                NodeState::Busy
            } else {
                NodeState::Online
            };
        }
    });
}

pub fn set_node_state(id: u32, state: NodeState) {
    with_cluster(|cluster| {
        if let Some(node) = cluster.nodes.iter_mut().find(|node| node.id == id) {
            node.state = state;
        }
    });
}

/// Migrate a process to another node.  Returns false when the target is
/// offline or the process does not exist.
pub fn migrate(pid: u32, node_id: u32) -> bool {
    with_cluster(|cluster| {
        let target_state = cluster
            .nodes
            .iter()
            .find(|node| node.id == node_id)
            .map(|node| node.state);
        let Some(state) = target_state else {
            return false;
        };
        if !matches!(state, NodeState::Online | NodeState::Busy) {
            return false;
        }
        let Some(process) = cluster.find(pid) else {
            return false;
        };
        process.current_node = node_id;
        process.state = ProcessState::Ready;
        cluster.migrations += 1;
        true
    })
    .unwrap_or(false)
}

/// Total migrations since boot.
pub fn migrations() -> u64 {
    with_cluster(|cluster| cluster.migrations).unwrap_or(0)
}

/// Recompute load and let sleeping processes become runnable.
pub fn tick() {
    with_cluster(|cluster| {
        // Processes that were marked sleeping wake after their state has been
        // visible for a scheduler pass.  Without user scheduling this is a
        // simple demonstration of state transitions.
        for process in &mut cluster.processes {
            if process.state == ProcessState::Sleeping {
                process.state = ProcessState::Ready;
            }
        }
    });
}

/// The PID of the desktop shell (PID 6 by construction).
pub fn desktop_pid() -> u32 {
    6
}

/// Render a textual cluster summary for `/proc/nodes` and the terminal.
pub fn nodes_text() -> String {
    let mut output = String::new();
    output.push_str("ID  NAME              STATE     CPUS  LOAD   MEMORY\n");
    for node in nodes() {
        use core::fmt::Write;
        let _ = writeln!(
            output,
            "{:<3} {:<17} {:<9} {:<5} {:>3}%   {} MiB",
            node.id,
            node.name_str(),
            state_name(node.state),
            node.cpu_count,
            node.load_permille / 10,
            node.memory_mib
        );
    }
    output
}

pub fn state_name(state: NodeState) -> &'static str {
    match state {
        NodeState::Offline => "offline",
        NodeState::Booting => "booting",
        NodeState::Online => "online",
        NodeState::Busy => "busy",
        NodeState::Draining => "draining",
    }
}

pub fn process_state_name(state: ProcessState) -> &'static str {
    match state {
        ProcessState::Unused => "unused",
        ProcessState::Ready => "ready",
        ProcessState::Running => "running",
        ProcessState::Sleeping => "sleeping",
        ProcessState::Blocked => "blocked",
        ProcessState::Zombie => "zombie",
        ProcessState::Migrating => "migrating",
    }
}
