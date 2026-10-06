import React, { useState } from 'react';
import { 
  Cpu, 
  GitBranch, 
  Database, 
  ShieldCheck, 
  AlertTriangle, 
  RotateCcw, 
  Play, 
  CheckCircle2, 
  Clock, 
  Layers, 
  Search, 
  Terminal, 
  ExternalLink, 
  Github, 
  Zap, 
  Sparkles 
} from 'lucide-react';

interface TaskNode {
  id: string;
  name: string;
  action: string;
  status: 'pending' | 'running' | 'verified' | 'failed' | 'rolled_back';
  layer: number;
  output: string;
  latency_us: number;
  invariants: string[];
  risk: 'Safe' | 'ReadOnly' | 'Sensitive' | 'Destructive';
}

interface Workflow {
  id: string;
  title: string;
  role: string;
  description: string;
  nodes: TaskNode[];
}

const WORKFLOWS: Workflow[] = [
  {
    id: 'k8s_incident',
    title: 'Kubernetes Incident Remediation',
    role: 'SRE / DevOps Autonomous Worker',
    description: 'Autonomous pod failure triage, runbook Trie retrieval, metric assertion, and verified service recovery.',
    nodes: [
      {
        id: 'step1_metrics',
        name: 'Fetch Pod Prometheus Metrics',
        action: 'fetch_metrics',
        status: 'pending',
        layer: 0,
        output: '{"pod": "auth-service-pod-3", "status": "OOMKilled", "memory_usage_mb": 2048}',
        latency_us: 142,
        invariants: ['no_system_secrets', 'valid_json_boundary'],
        risk: 'ReadOnly'
      },
      {
        id: 'step2_similar',
        name: 'Query Historical Incident Memory',
        action: 'query_runbook',
        status: 'pending',
        layer: 1,
        output: '{"matched_incident": "inc_902", "similarity": 0.999018, "root_cause": "heap_exhaustion"}',
        latency_us: 18,
        invariants: ['valid_json_boundary'],
        risk: 'ReadOnly'
      },
      {
        id: 'step3_runbook',
        name: 'Resolve Runbook via Prefix Trie',
        action: 'query_runbook',
        status: 'pending',
        layer: 1,
        output: '{"action": "restart", "runbook_guidance": "Runbook #402: Restart container with 4GB heap limit"}',
        latency_us: 8,
        invariants: ['valid_json_boundary'],
        risk: 'ReadOnly'
      },
      {
        id: 'step4_restart',
        name: 'Execute Graceful Service Restart',
        action: 'restart_service',
        status: 'pending',
        layer: 2,
        output: '{"restart_status": "success", "pod": "auth-service-pod-3", "healthy": true}',
        latency_us: 320,
        invariants: ['no_system_secrets', 'sql_destructive_guard', 'valid_json_boundary'],
        risk: 'Sensitive'
      }
    ]
  },
  {
    id: 'customer_refund',
    title: 'Customer Refund & Ledger Audit',
    role: 'Financial Reconciliation Worker',
    description: 'Validates customer dispute, audits policy limits in Trie, checks invariant bounds, and executes double-entry ledger mutation.',
    nodes: [
      {
        id: 'step1_dispute',
        name: 'Ingest Stripe Chargeback Event',
        action: 'api_ingest',
        status: 'pending',
        layer: 0,
        output: '{"charge_id": "ch_9812", "amount": 85.00, "currency": "USD", "reason": "duplicate_billing"}',
        latency_us: 95,
        invariants: ['valid_json_boundary'],
        risk: 'ReadOnly'
      },
      {
        id: 'step2_policy',
        name: 'Verify Department Refund Threshold',
        action: 'trie_lookup',
        status: 'pending',
        layer: 1,
        output: '{"policy": "policy:finance:refund:limit", "max_automated_usd": 500.0, "eligible": true}',
        latency_us: 12,
        invariants: ['valid_json_boundary'],
        risk: 'ReadOnly'
      },
      {
        id: 'step3_ledger',
        name: 'Commit Transactional Ledger Entry',
        action: 'db_ledger_mutate',
        status: 'pending',
        layer: 2,
        output: '{"ledger_tx_id": "tx_4419", "debit": 85.00, "credit": 85.00, "balanced": true}',
        latency_us: 215,
        invariants: ['no_system_secrets', 'sql_destructive_guard', 'valid_json_boundary'],
        risk: 'Sensitive'
      }
    ]
  }
];

export default function App() {
  const [selectedWorkflow, setSelectedWorkflow] = useState<Workflow>(WORKFLOWS[0]);
  const [nodes, setNodes] = useState<TaskNode[]>(WORKFLOWS[0].nodes);
  const [isRunning, setIsRunning] = useState(false);
  const [selectedNode, setSelectedNode] = useState<TaskNode | null>(WORKFLOWS[0].nodes[0]);
  const [trieQuery, setTrieQuery] = useState('policy');
  const [activeTab, setActiveTab] = useState<'dag' | 'memory' | 'benchmarks'>('dag');
  const [faultInjection, setFaultInjection] = useState(false);

  const handleSelectWorkflow = (wf: Workflow) => {
    setSelectedWorkflow(wf);
    setNodes(wf.nodes.map(n => ({ ...n, status: 'pending' })));
    setSelectedNode(wf.nodes[0]);
    setFaultInjection(false);
  };

  const runWorkflow = async (injectError = false) => {
    setIsRunning(true);
    setFaultInjection(injectError);
    // Reset all to pending
    setNodes(prev => prev.map(n => ({ ...n, status: 'pending' })));

    // Group by layer
    const maxLayer = Math.max(...selectedWorkflow.nodes.map(n => n.layer));

    for (let l = 0; l <= maxLayer; l++) {
      // Set current layer to running
      setNodes(prev => prev.map(n => n.layer === l ? { ...n, status: 'running' } : n));
      await new Promise(r => setTimeout(r, 600));

      if (injectError && l === 1) {
        // Trigger fault
        setNodes(prev => prev.map(n => {
          if (n.layer === l) {
            return {
              ...n,
              status: 'failed',
              output: 'CRITICAL INVARIANT VIOLATION: Output leaked raw credentials [sk-live-992834]',
              invariants: ['FAIL: no_system_secrets']
            };
          }
          if (n.layer > l) {
            return {
              ...n,
              status: 'rolled_back',
              output: 'State rolled back to previous verified checkpoint (L0 Snapshot)'
            };
          }
          return n;
        }));
        setIsRunning(false);
        return;
      }

      // Mark verified
      setNodes(prev => prev.map(n => n.layer === l ? { ...n, status: 'verified' } : n));
      await new Promise(r => setTimeout(r, 300));
    }

    setIsRunning(false);
  };

  // Mock Prefix Trie data
  const TRIE_DATABASE: Record<string, string> = {
    'policy:hr:leave:annual': '20 days paid leave per fiscal year',
    'policy:hr:leave:sick': '10 days medical leave with doctor certificate',
    'policy:finance:refund:limit': '$500 manager autonomous approval threshold',
    'policy:finance:tax:rate': '10% standard VAT adjustment',
    'policy:security:token_ttl': '900 seconds rotation window',
    'runbook:devops:service_oom': 'Runbook #402: Check memory leak in worker pool & restart container',
    'runbook:devops:db_failover': 'Runbook #105: Promote standby replica & flush connection pool'
  };

  const trieMatches = Object.entries(TRIE_DATABASE).filter(([k]) => 
    k.toLowerCase().startsWith(trieQuery.toLowerCase())
  );

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans">
      {/* Top Navigation */}
      <header className="border-b border-slate-800 bg-slate-900/80 backdrop-blur sticky top-0 z-50 px-6 py-4">
        <div className="max-w-7xl mx-auto flex items-center justify-between">
          <div className="flex items-center space-x-3">
            <div className="p-2 bg-gradient-to-tr from-cyan-600 to-sky-500 rounded-lg shadow-lg shadow-cyan-500/20">
              <Cpu className="w-6 h-6 text-white" />
            </div>
            <div>
              <div className="flex items-center space-x-2">
                <span className="font-bold text-xl tracking-tight text-white">AetherRuntime</span>
                <span className="text-xs bg-cyan-500/10 text-cyan-400 border border-cyan-500/30 px-2 py-0.5 rounded-full font-mono">
                  C++20 Core Engine
                </span>
              </div>
              <p className="text-xs text-slate-400">Deterministic Autonomous Agent Execution & Invariant Verifier</p>
            </div>
          </div>

          <div className="flex items-center space-x-3">
            <div className="flex bg-slate-800/80 p-1 rounded-lg border border-slate-700 text-xs">
              <button 
                onClick={() => setActiveTab('dag')}
                className={`px-3 py-1.5 rounded-md font-medium transition ${activeTab === 'dag' ? 'bg-cyan-500 text-white shadow' : 'text-slate-400 hover:text-white'}`}>
                DAG Scheduler
              </button>
              <button 
                onClick={() => setActiveTab('memory')}
                className={`px-3 py-1.5 rounded-md font-medium transition ${activeTab === 'memory' ? 'bg-cyan-500 text-white shadow' : 'text-slate-400 hover:text-white'}`}>
                Hierarchical Memory
              </button>
              <button 
                onClick={() => setActiveTab('benchmarks')}
                className={`px-3 py-1.5 rounded-md font-medium transition ${activeTab === 'benchmarks' ? 'bg-cyan-500 text-white shadow' : 'text-slate-400 hover:text-white'}`}>
                C++ Benchmarks
              </button>
            </div>

            <a 
              href="https://github.com/TejdeepKodati/aether-runtime" 
              target="_blank" 
              rel="noreferrer"
              className="flex items-center space-x-2 text-xs bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 px-3 py-2 rounded-lg transition"
            >
              <Github className="w-4 h-4" />
              <span>GitHub</span>
            </a>
          </div>
        </div>
      </header>

      {/* Main Content Area */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-6 space-y-6">
        {/* Banner Section */}
        <div className="bg-gradient-to-r from-slate-900 via-slate-800 to-slate-900 border border-slate-800 rounded-xl p-5 shadow-sm">
          <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
            <div>
              <div className="flex items-center space-x-2 mb-1">
                <span className="text-xs font-semibold uppercase tracking-wider text-cyan-400">Architectural Core</span>
                <span className="text-slate-500">•</span>
                <span className="text-xs text-slate-400">CentrAlign AI Founding Engineer Blueprint</span>
              </div>
              <h1 className="text-xl font-bold text-white">Agent Runtime: Planning, Execution, Memory & Invariant Verification</h1>
              <p className="text-sm text-slate-300 mt-1 max-w-3xl">
                Engineered with Kahn's topological sort ($O(V+E)$), $O(1)$ LRU working cache, $O(L)$ Prefix Trie policy lookup, 
                dense vector cosine retrieval ($k$-NN), and deterministic state snapshot rollback on invariant violation.
              </p>
            </div>
            
            <div className="flex items-center space-x-2">
              <button 
                onClick={() => runWorkflow(false)}
                disabled={isRunning}
                className="flex items-center space-x-2 bg-cyan-500 hover:bg-cyan-400 disabled:opacity-50 text-slate-950 font-semibold px-4 py-2.5 rounded-lg shadow-lg shadow-cyan-500/20 text-sm transition">
                <Play className="w-4 h-4 fill-slate-950" />
                <span>Execute DAG</span>
              </button>

              <button 
                onClick={() => runWorkflow(true)}
                disabled={isRunning}
                className="flex items-center space-x-2 bg-rose-600/20 hover:bg-rose-600/30 text-rose-300 border border-rose-500/30 font-medium px-4 py-2.5 rounded-lg text-sm transition">
                <AlertTriangle className="w-4 h-4 text-rose-400" />
                <span>Inject Fault & Rollback</span>
              </button>
            </div>
          </div>
        </div>

        {/* Tab 1: DAG Scheduler */}
        {activeTab === 'dag' && (
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
            {/* Left Column: Workflow Selector & DAG Graph */}
            <div className="lg:col-span-2 space-y-6">
              {/* Workflow Selector Bar */}
              <div className="flex items-center space-x-3 bg-slate-900 p-2 rounded-xl border border-slate-800">
                {WORKFLOWS.map(wf => (
                  <button
                    key={wf.id}
                    onClick={() => handleSelectWorkflow(wf)}
                    className={`flex-1 text-left px-3 py-2 rounded-lg transition ${selectedWorkflow.id === wf.id ? 'bg-slate-800 border border-slate-700 text-white' : 'text-slate-400 hover:text-white'}`}
                  >
                    <div className="text-xs font-semibold">{wf.title}</div>
                    <div className="text-[11px] text-slate-400 truncate">{wf.role}</div>
                  </button>
                ))}
              </div>

              {/* Topological Layer Visualizer */}
              <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center space-x-2">
                    <GitBranch className="w-5 h-5 text-cyan-400" />
                    <h2 className="font-bold text-white text-base">Topological Execution Graph (Kahn's Algorithm)</h2>
                  </div>
                  <span className="text-xs text-slate-400 font-mono">
                    {nodes.filter(n => n.status === 'verified').length}/{nodes.length} Completed
                  </span>
                </div>

                <div className="space-y-4">
                  {[0, 1, 2].map(layerIdx => {
                    const layerNodes = nodes.filter(n => n.layer === layerIdx);
                    if (layerNodes.length === 0) return null;

                    return (
                      <div key={layerIdx} className="space-y-2">
                        <div className="flex items-center space-x-2 text-xs font-mono text-slate-400">
                          <span className="bg-slate-800 px-2 py-0.5 rounded border border-slate-700 text-cyan-400">
                            Layer {layerIdx} (Parallel Batch)
                          </span>
                          <span className="text-slate-600">→ zero remaining in-degrees</span>
                        </div>

                        <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
                          {layerNodes.map(node => (
                            <div
                              key={node.id}
                              onClick={() => setSelectedNode(node)}
                              className={`p-4 rounded-xl border cursor-pointer transition ${
                                selectedNode?.id === node.id 
                                  ? 'border-cyan-500 bg-slate-800/80 shadow-md shadow-cyan-500/10' 
                                  : 'border-slate-800 bg-slate-900/60 hover:border-slate-700'
                              }`}
                            >
                              <div className="flex items-start justify-between">
                                <div className="space-y-1">
                                  <div className="text-xs font-mono text-cyan-400">{node.id}</div>
                                  <div className="text-sm font-semibold text-white">{node.name}</div>
                                </div>
                                <div>
                                  {node.status === 'verified' && (
                                    <span className="flex items-center space-x-1 text-xs text-emerald-400 bg-emerald-500/10 border border-emerald-500/20 px-2 py-0.5 rounded">
                                      <CheckCircle2 className="w-3 h-3" />
                                      <span>Verified</span>
                                    </span>
                                  )}
                                  {node.status === 'running' && (
                                    <span className="flex items-center space-x-1 text-xs text-amber-400 bg-amber-500/10 border border-amber-500/20 px-2 py-0.5 rounded animate-pulse">
                                      <Zap className="w-3 h-3" />
                                      <span>Executing</span>
                                    </span>
                                  )}
                                  {node.status === 'failed' && (
                                    <span className="flex items-center space-x-1 text-xs text-rose-400 bg-rose-500/10 border border-rose-500/20 px-2 py-0.5 rounded">
                                      <AlertTriangle className="w-3 h-3" />
                                      <span>Failed</span>
                                    </span>
                                  )}
                                  {node.status === 'rolled_back' && (
                                    <span className="flex items-center space-x-1 text-xs text-purple-400 bg-purple-500/10 border border-purple-500/20 px-2 py-0.5 rounded">
                                      <RotateCcw className="w-3 h-3" />
                                      <span>RolledBack</span>
                                    </span>
                                  )}
                                  {node.status === 'pending' && (
                                    <span className="flex items-center space-x-1 text-xs text-slate-400 bg-slate-800 px-2 py-0.5 rounded">
                                      <Clock className="w-3 h-3" />
                                      <span>Pending</span>
                                    </span>
                                  )}
                                </div>
                              </div>

                              <div className="mt-3 flex items-center justify-between text-xs text-slate-400">
                                <span className={`px-2 py-0.5 rounded font-mono ${
                                  node.risk === 'Destructive' ? 'bg-rose-500/10 text-rose-400' :
                                  node.risk === 'Sensitive' ? 'bg-amber-500/10 text-amber-400' :
                                  'bg-slate-800 text-slate-300'
                                }`}>
                                  Risk: {node.risk}
                                </span>
                                <span className="font-mono">{node.latency_us} µs</span>
                              </div>
                            </div>
                          ))}
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>
            </div>

            {/* Right Column: Node Inspector & Verification Report */}
            <div className="space-y-6">
              <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-4">
                <div className="flex items-center justify-between border-b border-slate-800 pb-3">
                  <div className="flex items-center space-x-2">
                    <Terminal className="w-5 h-5 text-cyan-400" />
                    <h3 className="font-bold text-white text-base">Node Execution Inspector</h3>
                  </div>
                  {selectedNode && (
                    <span className="text-xs font-mono text-slate-400">{selectedNode.id}</span>
                  )}
                </div>

                {selectedNode ? (
                  <div className="space-y-4 text-xs">
                    <div>
                      <span className="text-slate-400 block mb-1">Action Type & Tool:</span>
                      <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800 font-mono text-cyan-300">
                        {selectedNode.action} ({selectedNode.risk} risk)
                      </div>
                    </div>

                    <div>
                      <span className="text-slate-400 block mb-1">Invariant Assertions Evaluated:</span>
                      <div className="space-y-1.5">
                        {selectedNode.invariants.map((inv, idx) => (
                          <div key={idx} className="flex items-center space-x-2 bg-slate-950 p-2 rounded border border-slate-800">
                            {inv.startsWith('FAIL') ? (
                              <AlertTriangle className="w-3.5 h-3.5 text-rose-400 flex-shrink-0" />
                            ) : (
                              <ShieldCheck className="w-3.5 h-3.5 text-emerald-400 flex-shrink-0" />
                            )}
                            <span className={inv.startsWith('FAIL') ? 'text-rose-400 font-mono' : 'text-slate-300 font-mono'}>
                              {inv}
                            </span>
                          </div>
                        ))}
                      </div>
                    </div>

                    <div>
                      <span className="text-slate-400 block mb-1">Committed Payload / Working Memory:</span>
                      <pre className="bg-slate-950 p-3 rounded-lg border border-slate-800 font-mono text-emerald-400 whitespace-pre-wrap break-all text-[11px] max-h-40 overflow-y-auto">
                        {selectedNode.output}
                      </pre>
                    </div>

                    <div className="border-t border-slate-800 pt-3 flex justify-between text-slate-400">
                      <span>Kahn Topological Layer: {selectedNode.layer}</span>
                      <span>Dispatch Latency: {selectedNode.latency_us} µs</span>
                    </div>
                  </div>
                ) : (
                  <div className="text-center text-slate-500 py-10">Select a node to inspect state</div>
                )}
              </div>

              {/* Checkpoint & Rollback Status Card */}
              <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-3">
                <div className="flex items-center space-x-2">
                  <RotateCcw className="w-4 h-4 text-purple-400" />
                  <h4 className="font-semibold text-sm text-white">Transactional Checkpoint Stack</h4>
                </div>
                <p className="text-xs text-slate-400">
                  Every DAG node invocation snapshots state. If verification fails or invariant assertion breaks, 
                  the runtime executes an $O(1)$ stack rollback to restore system invariants.
                </p>
                <div className="bg-slate-950 p-3 rounded-lg border border-slate-800 text-xs font-mono space-y-1 text-slate-300">
                  <div>Status: {faultInjection ? 'Rollback Restored (Safe Invariant)' : 'Deterministic Checkpoint Active'}</div>
                  <div>Stack Depth: {nodes.filter(n => n.status === 'verified').length} commits</div>
                  <div>Rollback Recovery Time: &lt; 5 microseconds</div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Tab 2: Hierarchical Memory Explorer */}
        {activeTab === 'memory' && (
          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            {/* L1 LRU Cache */}
            <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-4">
              <div className="flex items-center space-x-2">
                <Database className="w-5 h-5 text-cyan-400" />
                <div>
                  <h3 className="font-bold text-white text-sm">L1 Working Memory (LRU Cache)</h3>
                  <p className="text-xs text-slate-400">Custom Doubly-Linked List + Hash Map</p>
                </div>
              </div>

              <div className="bg-slate-950 p-3 rounded-lg border border-slate-800 text-xs font-mono space-y-1 text-slate-300">
                <div className="text-cyan-400 font-semibold mb-2">Complexity: O(1) Get / Put</div>
                <div>Capacity: 128 working keys</div>
                <div>Hits: 1,420 | Misses: 12</div>
                <div>Hit Rate: 99.16%</div>
              </div>

              <div className="space-y-2">
                <span className="text-xs text-slate-400">Most Recently Used Keys:</span>
                <div className="space-y-1.5 text-xs font-mono">
                  <div className="bg-slate-800/60 p-2 rounded border border-slate-700 text-slate-200">
                    task_result:step1_metrics (head)
                  </div>
                  <div className="bg-slate-800/60 p-2 rounded border border-slate-700 text-slate-200">
                    task_result:step2_similar
                  </div>
                  <div className="bg-slate-800/60 p-2 rounded border border-slate-700 text-slate-400">
                    user_session:u_8829 (tail / lru)
                  </div>
                </div>
              </div>
            </div>

            {/* L2 Prefix Trie */}
            <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-4">
              <div className="flex items-center space-x-2">
                <Layers className="w-5 h-5 text-emerald-400" />
                <div>
                  <h3 className="font-bold text-white text-sm">L2 Company Ontology (Prefix Trie)</h3>
                  <p className="text-xs text-slate-400">Instant Prefix Matching O(L)</p>
                </div>
              </div>

              <div className="space-y-2">
                <label className="text-xs text-slate-400">Interactive Prefix Query:</label>
                <div className="relative">
                  <Search className="w-4 h-4 text-slate-500 absolute left-3 top-2.5" />
                  <input
                    type="text"
                    value={trieQuery}
                    onChange={e => setTrieQuery(e.target.value)}
                    placeholder="Search prefix (e.g., policy, runbook)..."
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg pl-9 pr-3 py-2 text-xs font-mono text-white focus:outline-none focus:border-cyan-500"
                  />
                </div>
              </div>

              <div className="space-y-1.5 max-h-56 overflow-y-auto">
                {trieMatches.map(([key, desc]) => (
                  <div key={key} className="bg-slate-950 p-2 rounded border border-slate-800 text-xs font-mono">
                    <div className="text-emerald-400 font-semibold">{key}</div>
                    <div className="text-slate-400 text-[11px] mt-0.5">{desc}</div>
                  </div>
                ))}
              </div>
            </div>

            {/* L3 Vector Memory */}
            <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-4">
              <div className="flex items-center space-x-2">
                <Sparkles className="w-5 h-5 text-purple-400" />
                <div>
                  <h3 className="font-bold text-white text-sm">L3 Semantic Context (Vector Index)</h3>
                  <p className="text-xs text-slate-400">Dense Embedding Cosine Similarity k-NN</p>
                </div>
              </div>

              <div className="bg-slate-950 p-3 rounded-lg border border-slate-800 text-xs font-mono space-y-1 text-slate-300">
                <div className="text-purple-400 font-semibold mb-2">Complexity: O(N·D), Top-K Heap O(N log K)</div>
                <div>Dimension: 16-d dense vectors</div>
                <div>Indexed Company Runbooks: 1,000 docs</div>
                <div>Average Query Latency: 14.33 µs</div>
              </div>

              <div className="space-y-2">
                <span className="text-xs text-slate-400">Top Semantic Retrieval Matches:</span>
                <div className="space-y-1.5 text-xs font-mono">
                  <div className="bg-slate-950 p-2 rounded border border-purple-500/30 text-purple-200">
                    <div className="flex justify-between">
                      <span className="font-semibold">inc_902</span>
                      <span className="text-purple-400">99.9% match</span>
                    </div>
                    <div className="text-[11px] text-slate-400 mt-0.5">OOM killer invoked on auth-service pod</div>
                  </div>
                  <div className="bg-slate-950 p-2 rounded border border-slate-800 text-slate-400">
                    <div className="flex justify-between">
                      <span>inc_901</span>
                      <span>34.2% match</span>
                    </div>
                    <div className="text-[11px] text-slate-500 mt-0.5">Payment gateway pool latency spike</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Tab 3: C++ Benchmarks */}
        {activeTab === 'benchmarks' && (
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-6 space-y-6">
            <div>
              <h2 className="text-lg font-bold text-white">C++ High-Performance Systems Benchmarks</h2>
              <p className="text-sm text-slate-400">
                Measured on local MinGW GCC C++17 build under release optimization flags (-O2).
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
              <div className="bg-slate-950 p-5 rounded-xl border border-slate-800 space-y-3">
                <div className="text-xs font-semibold uppercase tracking-wider text-cyan-400">L1 LRU Working Cache</div>
                <div className="text-3xl font-extrabold text-white font-mono">8.65M <span className="text-sm font-normal text-slate-400">ops/sec</span></div>
                <p className="text-xs text-slate-400">
                  Custom doubly-linked list with hash map node pointers. 100,000 lookups completed in 11.55 ms.
                </p>
                <div className="text-xs font-mono text-cyan-400 bg-cyan-500/10 p-2 rounded border border-cyan-500/20">
                  Put Throughput: 2.28M ops/sec
                </div>
              </div>

              <div className="bg-slate-950 p-5 rounded-xl border border-slate-800 space-y-3">
                <div className="text-xs font-semibold uppercase tracking-wider text-emerald-400">DAG Plan Compilation</div>
                <div className="text-3xl font-extrabold text-white font-mono">&lt; 150 <span className="text-sm font-normal text-slate-400">µs</span></div>
                <p className="text-xs text-slate-400">
                  Kahn's topological sort across 1,000 task nodes and multi-tier dependency chains with automated cycle detection.
                </p>
                <div className="text-xs font-mono text-emerald-400 bg-emerald-500/10 p-2 rounded border border-emerald-500/20">
                  Complexity: O(V + E) deterministic
                </div>
              </div>

              <div className="bg-slate-950 p-5 rounded-xl border border-slate-800 space-y-3">
                <div className="text-xs font-semibold uppercase tracking-wider text-purple-400">L3 Vector Similarity Search</div>
                <div className="text-3xl font-extrabold text-white font-mono">14.33 <span className="text-sm font-normal text-slate-400">µs / query</span></div>
                <p className="text-xs text-slate-400">
                  Top-5 k-NN retrieval over 1,000 dense normalized enterprise document vectors using min-heap top-K selection.
                </p>
                <div className="text-xs font-mono text-purple-400 bg-purple-500/10 p-2 rounded border border-purple-500/20">
                  Batch: 1,000 queries in 14.33 ms
                </div>
              </div>
            </div>

            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800">
              <div className="text-xs font-mono text-slate-400 mb-2 font-semibold">Raw CLI Benchmark Output (benchmark.exe):</div>
              <pre className="text-xs font-mono text-slate-300 overflow-x-auto whitespace-pre">
{`[BENCHMARK] TaskDAG Topological Plan Compilation (Kahn's Algorithm)...
  - 1,000 Node DAG Compilation: 0.00 us (5 parallel layers computed)

[BENCHMARK] L1 LRU Cache Throughput...
  - 100,000 Puts: 43.70 ms (2,288,172.44 ops/sec)
  - 100,000 Gets: 11.55 ms (8,659,508.14 ops/sec)

[BENCHMARK] L3 Vector Memory Cosine Similarity Search...
  - 1,000 Vector Queries over 1,000 Docs (k=5): 14.33 ms (14.33 us/query)`}
              </pre>
            </div>
          </div>
        )}
      </main>

      {/* Mandatory Footer with made with ♥ by Tejdeep */}
      <footer className="border-t border-slate-800 bg-slate-950/80 px-6 py-4 text-xs text-slate-500">
        <div className="max-w-7xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-3">
          <div>
            <span>AetherRuntime © 2026. Designed for Autonomous AI Workforce Infrastructure.</span>
          </div>
          
          <div className="flex items-center space-x-4">
            <a 
              href="https://github.com/TejdeepKodati/aether-runtime" 
              target="_blank" 
              rel="noreferrer"
              className="text-slate-300 hover:text-white transition font-medium underline flex items-center space-x-1"
            >
              <span>made with ♥ by Tejdeep</span>
              <ExternalLink className="w-3.5 h-3.5" />
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}
