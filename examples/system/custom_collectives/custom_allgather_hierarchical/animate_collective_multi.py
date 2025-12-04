#!/usr/bin/env python3
import argparse
import glob
import os
import sys
from typing import Dict, List, Optional
import networkx as nx
from pathlib import Path
from collections import defaultdict
import plotly.graph_objects as go
import math

from chakra.src.third_party.utils.protolib import (
        decodeMessage as decode_message,
        openFileRd as open_file_rd,
    )

from chakra.schema.protobuf.et_def_pb2 import (
    GlobalMetadata,
    Node,
    COMM_SEND_NODE,
    COMM_RECV_NODE,
)

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

def attrs_to_dict(pb_node):
    out = {}
    for a in pb_node.attr:
        if a.HasField("int32_val"):   out[a.name] = a.int32_val
        elif a.HasField("int64_val"): out[a.name] = a.int64_val
        elif a.HasField("float_val"): out[a.name] = a.float_val
        elif a.HasField("double_val"):out[a.name] = a.double_val
        elif a.HasField("string_val"):out[a.name] = a.string_val
        elif a.HasField("bool_val"):  out[a.name] = a.bool_val
    return out

def _parse_rank_from_fname(fname: str) -> Optional[int]:
    parts = os.path.basename(fname).split(".")
    if len(parts) >= 3 and parts[-1] == "et":
        try:
            return int(parts[-2])
        except ValueError:
            return None
    return None

# ---------------------------------------------------------------------------
# Data loading / parsing classes
# ---------------------------------------------------------------------------

class ETLoader:
    """
    Responsible for reading Chakra ET files and building the rank → time_step index.
    """

    def __init__(self, input_dir: str):
        self.input_dir = input_dir

    def build_rank_ts_index(self) -> Dict[int, Dict[int, List[Dict[str, object]]]]:
        """
        Return:
          {
            <rank_id>: {
              <time_step>: [
                {
                  "type": "send" | "recv" | "other",
                  "peer": <int or None>,          # dst for send, src for recv
                  "data_deps": [...],             # raw ids as found 
                  "ctrl_deps": [...],             # raw ids as found 
                  "attrs": { ... },               # full attr dict
                  "et_file": "<filename>",       
                }, ...
              ],
            },
            ...
          }
        """
        input_dir = self.input_dir
        et_paths = sorted(glob.glob(os.path.join(input_dir, "*.et")))
        if not et_paths:
            raise FileNotFoundError(f"No .et files found under: {input_dir}")

        rank_ts: Dict[int, Dict[int, List[Dict[str, object]]]] = {}

        for p in et_paths:
            rank = _parse_rank_from_fname(p)
            if rank is None:
                continue
            fname = os.path.basename(p)

            et = open_file_rd(p)
            gm = GlobalMetadata()
            node = Node()
            decode_message(et, gm)

            while decode_message(et, node):
                attrs = attrs_to_dict(node)
                ts = attrs.get("time_step")
                # skip nodes without a valid time_step
                if not isinstance(ts, int):
                    continue

                comm_type = attrs.get("comm_type")
                if comm_type == COMM_SEND_NODE:
                    etype = "send"
                    peer = attrs.get("comm_dst")
                elif comm_type == COMM_RECV_NODE:
                    etype = "recv"
                    peer = attrs.get("comm_src")
                else:
                    etype = "other"
                    peer = None

                tbid = attrs.get("tb_id")

                # dependency fields
                hasdep = attrs.get("hasdep", False)
                depid  = attrs.get("depid")
                deps   = attrs.get("deps")

                rec = {
                    "type": etype,
                    "peer": peer if isinstance(peer, int) else None,
                    "tb_id": int(tbid) if (tbid is not None and tbid != "") else 0,
                    "step": int(ts) if (ts is not None and ts != "") else None,
                    "hasdep": bool(hasdep),
                    "depid": int(depid) if (depid is not None and depid != "") else None,
                    "deps": int(deps) if (deps is not None and deps != "") else None,
                    "data_deps": list(node.data_deps),
                    "ctrl_deps": list(node.ctrl_deps),
                    "attrs": attrs,
                    "et_file": fname,
                }

                if rank not in rank_ts:
                    rank_ts[rank] = {}
                if ts not in rank_ts[rank]:
                    rank_ts[rank][ts] = []
                rank_ts[rank][ts].append(rec)

            et.close()

        # deterministic order inside each (rank, ts)
        for r, ts_map in rank_ts.items():
            for ts, lst in ts_map.items():
                lst.sort(key=lambda rec: (rec["type"], rec["peer"] if rec["peer"] is not None else -1))

        return rank_ts

# ---------------------------------------------------------------------------
# Graph building class
# ---------------------------------------------------------------------------

class TENGraphBuilder:
    """
    Builds the global communication timeline graph (MultiDiGraph) from ET rank/time-step data.
    """

    def __init__(self, rank_ts: Dict[int, Dict[int, List[Dict[str, object]]]]):
        self.rank_ts = rank_ts

    def build_graph(self) -> nx.MultiDiGraph:
        """
        Global timeline defined ONLY by SEND steps, with constraints from full step DAG.
        Step identity: (rank, tb_id, time_step). We build per-rank DAG:
          - Intra-TB edges by ascending time_step
          - Dependency edges only when depid,deps are valid (>=0): (depid,deps) -> (tb_id,time_step)
        Then assign dense timestamps to SEND steps only (0..N-1 per rank) using a boundary DP
        so any RECV/OTHER constraints force SEND strictly later. Node names remain r{rank}:t{t}.
        """
        from collections import defaultdict, deque

        rank_ts = self.rank_ts

        # ---- Build step-nodes keyed by (rank, tb_id, step_ts) ----
        step_nodes = {}  # (rank, tb, step_ts) -> aggregated info
        ranks_present = set()

        for rank, ts_map in rank_ts.items():
            ranks_present.add(rank)
            for step_ts, recs in ts_map.items():
                for rec in recs:
                    tb = int(rec.get("tb_id", 0))
                    key = (rank, tb, int(step_ts))
                    if key not in step_nodes:
                        step_nodes[key] = {
                            "rank": rank,
                            "tb_id": tb,
                            "step_ts": int(step_ts),   # TB-local time_step
                            "events": [],
                            "has_send": False,
                        }
                    step_nodes[key]["events"].append(rec)
                    if rec.get("type") == "send":
                        step_nodes[key]["has_send"] = True

        # Collect all SEND events for comm edges later
        all_sends = []
        for (rank, tb, step_ts), node in step_nodes.items():
            for ev in node["events"]:
                if ev.get("type") == "send":
                    all_sends.append({
                        "rank": rank,
                        "tb_id": tb,
                        "step_ts": step_ts,
                        "peer": ev.get("peer"),
                        "attrs": ev.get("attrs", {}),
                    })

        if not all_sends:
            sys.stderr.write("[build_graph] WARNING: no SEND records found; returning empty graph.\n")
            return nx.MultiDiGraph()

        # ---- Build FULL per-rank DAG across step-nodes ----
        Gfull = nx.DiGraph()
        for key, data in step_nodes.items():
            Gfull.add_node(key, **data)

        # Intra-TB sequencing edges by step_ts
        tb_to_steps = defaultdict(list)  # (rank,tb) -> [step_ts]
        for (rank, tb, step_ts) in Gfull.nodes:
            tb_to_steps[(rank, tb)].append(step_ts)
        for (rank, tb), steps in tb_to_steps.items():
            steps = sorted(set(steps))
            for a, b in zip(steps, steps[1:]):
                Gfull.add_edge((rank, tb, a), (rank, tb, b))

        # Dependency edges: ONLY when depid,deps are valid (>=0)
        dep_edges = 0
        for (rank, tb, step_ts), data in step_nodes.items():
            for ev in data["events"]:
                depid = ev.get("depid", -1)
                deps  = ev.get("deps",  -1)
                if not (isinstance(depid, int) and isinstance(deps, int) and depid >= 0 and deps >= 0):
                    continue
                dep_src = (rank, int(depid), int(deps))   # the step we depend on
                dep_dst = (rank, tb, step_ts)             # this step
                if dep_src in Gfull and dep_dst in Gfull:
                    Gfull.add_edge(dep_src, dep_dst)
                    dep_edges += 1
                else:
                    if rank == 0:
                        if dep_src not in Gfull:
                            sys.stderr.write(f"[dep-miss rank0] src not found: {dep_src} (for dst {dep_dst})\n")
                        if dep_dst not in Gfull:
                            sys.stderr.write(f"[dep-miss rank0] dst not found: {dep_dst} (from src {dep_src})\n")

        sys.stderr.write(f"[build_graph] full_nodes={Gfull.number_of_nodes()}, dep_edges={dep_edges}\n")

        # ---- Per-rank topo + boundary DP: assign DENSE timestamps to SEND steps only ----
        send_ts = {}  # (rank,tb,step_ts) -> dense t
        for rank in sorted(ranks_present):
            nodes_r = [n for n in Gfull.nodes if n[0] == rank]
            if not nodes_r:
                continue
            H = Gfull.subgraph(nodes_r).copy()

            # Kahn topo for this rank
            indeg = {n: H.in_degree(n) for n in H.nodes}
            q = deque([n for n, d in indeg.items() if d == 0])
            topo = []
            while q:
                u = q.popleft()
                topo.append(u)
                for v in H.successors(u):
                    indeg[v] -= 1
                    if indeg[v] == 0:
                        q.append(v)

            # boundary[u] = minimal SEND index that must precede u
            boundary = defaultdict(int)
            next_free = 0  # next dense SEND index

            for u in topo:
                u_is_send = H.nodes[u].get("has_send", False)

                # required min index from predecessors
                req = 0
                for p in H.predecessors(u):
                    if H.nodes[p].get("has_send", False):
                        req = max(req, send_ts[p] + 1)   # strictly after any predecessor SEND
                    else:
                        req = max(req, boundary[p])      # carry boundary via non-SENDs

                if u_is_send:
                    t_u = max(req, next_free)
                    send_ts[u] = t_u
                    next_free = t_u + 1
                    boundary[u] = send_ts[u] + 1  # SEND contributes +1 to successors
                else:
                    boundary[u] = req

            # Safety: if any SEND missed (e.g., due to cycles), append them
            missing = [n for n in H.nodes if H.nodes[n].get("has_send", False) and n not in send_ts]
            if missing:
                missing.sort(key=lambda n: (n[1], n[2]))
                for n in missing:
                    send_ts[n] = next_free
                    next_free += 1

        if send_ts:
            rng = (min(send_ts.values()), max(send_ts.values()))
        else:
            rng = (0, 0)
        sys.stderr.write(f"[build_graph] SEND time range: {rng[0]}–{rng[1]}\n")

        # ---- DEBUG: print SEND order for all ranks ----
        rank_dep_map = defaultdict(list)
        for (r_, tb_, st_), data in step_nodes.items():
            seen = set()
            for ev in data["events"]:
                depid = ev.get("depid", -1)
                deps  = ev.get("deps",  -1)
                if isinstance(depid, int) and isinstance(deps, int) and depid >= 0 and deps >= 0:
                    if (depid, deps) not in seen:
                        rank_dep_map[(r_, tb_, st_)].append((depid, deps))
                        seen.add((depid, deps))
        ranks_seen = sorted({n[0] for n in send_ts})
        for rr in ranks_seen:
            sys.stderr.write(f"\n[SEND ORDER rank {rr}]\n")
            items = sorted(((n, t) for n, t in send_ts.items() if n[0] == rr), key=lambda x: x[1])
            for (r, tb, st), t in items:
                deps_list = rank_dep_map.get((r, tb, st), [])
                deps_txt = "none" if not deps_list else ", ".join(f"(tb={d_tb}, step={d_st})" for d_tb, d_st in deps_list)
                sys.stderr.write(f"tb={tb}, step_ts={st} -> global_t={t} | depends_on: {deps_txt}\n")
        sys.stderr.write("\n")

        # ---- Build visualization graph: r{rank}:t{t} nodes ----
        round_events = defaultdict(lambda: defaultdict(list))  # rank -> t -> [step-nodes with SEND]
        for key, t in send_ts.items():
            rank, tb, step_ts = key
            round_events[rank][t].append(Gfull.nodes[key])

        MG = nx.MultiDiGraph()
        for rank, tmap in round_events.items():
            for t, step_recs in tmap.items():
                node_name = f"r{rank}:t{t}"
                MG.add_node(node_name, label=str(rank), rank=rank, time_step=t, events=step_recs)

        # ---- Comm edges: sender at t -> receiver at t+1 ----
        for s in all_sends:
            skey = (s["rank"], s["tb_id"], s["step_ts"])
            if skey not in send_ts:
                tmax = max(round_events[s["rank"]].keys(), default=-1) + 1
                send_ts[skey] = tmax
                node_name = f"r{s['rank']}:t{tmax}"
                if node_name not in MG:
                    MG.add_node(node_name, label=str(s['rank']), rank=s['rank'], time_step=tmax, events=[Gfull.nodes[skey]])

            src_t = send_ts[skey]
            src_node = f"r{s['rank']}:t{src_t}"

            peer = s.get("peer")
            if not isinstance(peer, int):
                continue

            dst_t = src_t + 1
            dst_node = f"r{peer}:t{dst_t}"
            if dst_node not in MG:
                MG.add_node(dst_node, label=str(peer), rank=peer, time_step=dst_t, events=[])

            chunk_val = s.get("attrs", {}).get("chunk_offset")
            if isinstance(chunk_val, str):
                try:
                    chunk_val = int(chunk_val)
                except Exception:
                    pass
            chunk_count = s.get("attrs", {}).get("msg_chunk_cnt", 1)
            try:
                chunk_count = int(chunk_count)
            except:
                chunk_count = 1

            MG.add_edge(
                src_node, dst_node,
                dependency="comm", comm_dir="send",
                from_rank=s["rank"], to_rank=peer,
                from_ts=src_t, to_ts=dst_t,
                chunk=chunk_val,
                chunk_count=chunk_count,
                thick=False,
            )

        sys.stderr.write(f"[build_graph] MG nodes={MG.number_of_nodes()}, edges={MG.number_of_edges()}\n")
        print("\n=== DEBUG: COMM EDGES WITH CHUNK INFO ===")
        for u, v, d in MG.edges(data=True):
            if d.get("dependency") == "comm":
                print(f"{u} -> {v} | chunk_offset={d.get('chunk')} | chunk_count={d.get('chunk_count')}")
        print("========================================\n")

        return MG

# ---------------------------------------------------------------------------
# Plotly visualization class
# ---------------------------------------------------------------------------

class PlotlyTENVisualizer:
    """
    Plotly-based TEN layout visualizer with dynamic sizing and hover highlighting.
    """

    def __init__(self):
        pass

    def visualize(self, G, out_html: str | None = None):
        """
        Interactive TEN layout visualization with Plotly.
        """
        import math
        from collections import defaultdict
        import plotly.graph_objects as go

        # ---- layout/style constants ----
        n_ts = len({G.nodes[n].get("time_step") for n in G.nodes if G.nodes[n].get("time_step") is not None})
        n_ranks = len({G.nodes[n].get("rank", 0) for n in G.nodes})

        COL_GAP = 5  # horizontal spacing
        ROW_GAP = 3  # vertical spacing
        NODE_TRIM_D    = COL_GAP * 0.15
        NODE_MARKER_SZ = 30
        NODE_BORDER    = 2
        ARROW_W        = 1.4
        ARROW_W_RRC    = 2.0
        ARROWSIZE      = 0.80
        OFFSET_UNIT    = 0.08
        HEADER_PAD_Y   = 0.9
        FONT_HDR_SZ    = 14

        TAB20 = [
            "#1f77b4","#aec7e8","#ff7f0e","#ffbb78","#2ca02c","#98df8a",
            "#d62728","#ff9896","#9467bd","#c5b0d5","#8c564b","#c49c94",
            "#e377c2","#f7b6d2","#7f7f7f","#c7c7c7","#bcbd22","#dbdb8d",
            "#17becf","#9edae5"
        ]

        def chunk_color(cid):
            return "#9e9e9e" if cid is None else TAB20[hash(cid) % 20]

        def luminance(hex_color):
            hex_color = hex_color.lstrip("#")
            r = int(hex_color[0:2], 16) / 255.0
            g = int(hex_color[2:4], 16) / 255.0
            b = int(hex_color[4:6], 16) / 255.0
            return 0.299 * r + 0.587 * g + 0.114 * b

        # ---- grid placement ----
        ts_values = sorted({G.nodes[n].get("time_step")
                            for n in G.nodes if G.nodes[n].get("time_step") is not None})
        ranks = sorted({G.nodes[n].get("rank", 0) for n in G.nodes})
        if not ts_values or not ranks:
            print("No nodes to visualize; skipping HTML render.")
            return

        ts_to_col   = {t: i for i, t in enumerate(ts_values)}
        rank_to_row = {r: i for i, r in enumerate(ranks)}

        pos = {}
        for n in G.nodes:
            t  = G.nodes[n].get("time_step")
            rk = G.nodes[n].get("rank", 0)
            if t is None:
                continue
            pos[n] = (ts_to_col[t] * COL_GAP, -rank_to_row[rk] * ROW_GAP)

        # dynamic figure sizing (same as before, just computed; not directly used in layout)
        n_steps = len(ts_values)
        n_gpus = len(ranks)
        base_width_per_step = 85
        base_height_per_gpu  = 65
        fig_width  = min(4000, max(900, n_steps * base_width_per_step))
        fig_height = min(2500, max(700, n_gpus * base_height_per_gpu))

        # ---- node trace ----
        node_x, node_y, node_labels, node_hover = [], [], [], []
        for n, (x, y) in pos.items():
            node_x.append(x)
            node_y.append(y)
            node_labels.append(str(G.nodes[n].get("rank", "")))
            node_hover.append(f"Node={n}<br>rank={G.nodes[n].get('rank')}<br>t={G.nodes[n].get('time_step')}")

        node_trace = go.Scatter(
            x=node_x, y=node_y,
            mode="markers+text",
            text=node_labels,
            textposition="middle center",
            marker=dict(
                symbol="square",
                size=NODE_MARKER_SZ,
                line=dict(width=NODE_BORDER, color="black"),
                color="white",
            ),
            hovertext=node_hover,
            hoverinfo="text",
            name="nodes",
        )

        from collections import defaultdict
        edge_count = defaultdict(int)
        for u, v, k, d in G.edges(keys=True, data=True):
            if d.get("dependency") == "comm" and (u in pos) and (v in pos):
                edge_count[(u, v)] += 1
        edge_index = defaultdict(int)

        def shrink_segment_abs(x0, y0, x1, y1, trim_dist=NODE_TRIM_D):
            dx, dy = x1 - x0, y1 - y0
            L = math.hypot(dx, dy)
            if L == 0:
                return x0, y0, x1, y1
            t = min(trim_dist / L, 0.32)
            return x0 + dx * t, y0 + dy * t, x1 - dx * t, y1 - dy * t

        def apply_perp_offset(x0, y0, x1, y1, offset):
            dx, dy = x1 - x0, y1 - y0
            L = math.hypot(dx, dy)
            if L == 0:
                return x0, y0, x1, y1
            nx, ny = -dy / L, dx / L
            return x0 + nx * offset, y0 + ny * offset, x1 + nx * offset, y1 + ny * offset

        def alt_offset_for_idx(idx, base=OFFSET_UNIT):
            sign = 1 if (idx % 2 == 0) else -1
            mult = (idx // 2) + 1
            return sign * mult * base

        # ---- edges and arrows ----
        edge_traces = []
        DEFAULT_OPACITY = 0.2
        for u, v, k, d in G.edges(keys=True, data=True):
            if d.get("dependency") != "comm" or u not in pos or v not in pos:
                continue
            x0, y0 = pos[u]
            x1, y1 = pos[v]
            x0s, y0s, x1s, y1s = shrink_segment_abs(x0, y0, x1, y1, NODE_TRIM_D)
            total = edge_count[(u, v)]
            idx = edge_index[(u, v)]
            offset = alt_offset_for_idx(idx, OFFSET_UNIT) if total > 1 else 0.0
            x0o, y0o, x1o, y1o = apply_perp_offset(x0s, y0s, x1s, y1s, offset)
            is_rrc = bool(d.get("thick"))
            edge_color = "red" if is_rrc else "black"
            edge_width = ARROW_W_RRC if is_rrc else ARROW_W
            edge_traces.append(go.Scatter(
                x=[x0o, x1o],
                y=[y0o, y1o],
                mode='lines+markers',
                line=dict(width=edge_width, color=edge_color),
                marker=dict(
                    symbol=['circle', 'diamond'],
                    size=[0, ARROWSIZE*10],
                    color=[edge_color, edge_color],
                    line=dict(width=0),
                ),
                hoverinfo='text',
                hovertext=f"{u} → {v}",
                opacity=DEFAULT_OPACITY,
                name='edges',
                customdata=[f"{u}|{v}"],
            ))
            edge_index[(u, v)] += 1

        # ---- chunk bubbles ----
        circle_traces = []
        edge_index.clear()
        for u, v, k, d in G.edges(keys=True, data=True):
            if d.get("dependency") != "comm" or u not in pos or v not in pos:
                continue

            chunk_offset = d.get("chunk")
            chunk_count  = d.get("chunk_count", 1)
            if chunk_offset is None:
                continue

            x0, y0 = pos[u]
            x1, y1 = pos[v]
            x0s, y0s, x1s, y1s = shrink_segment_abs(x0, y0, x1, y1, NODE_TRIM_D)
            total = edge_count[(u, v)]
            idx   = edge_index[(u, v)]
            base_off = alt_offset_for_idx(idx, OFFSET_UNIT) if total > 1 else 0.0
            x0o, y0o, x1o, y1o = apply_perp_offset(x0s, y0s, x1s, y1s, base_off)

            for ci in range(chunk_count):
                cid = chunk_offset + ci

                frac = 0.25 + 0.12 * ci
                cx = x0o + frac * (x1o - x0o)
                cy = y0o + frac * (y1o - y0o)

                dx, dy = (x1o - x0o), (y1o - y0o)
                L = math.hypot(dx, dy) or 1.0
                nx_, ny_ = -dy/L, dx/L
                cx_lift = cx + nx_ * (0.06 + ci * 0.12)
                cy_lift = cy + ny_ * (0.06 + ci * 0.12)

                col = chunk_color(cid)
                txt_col = "black" if luminance(col) > 0.6 else "white"

                circle_traces.append(go.Scatter(
                    x=[cx_lift], y=[cy_lift],
                    mode="markers+text",
                    marker=dict(size=16, color=col, line=dict(width=1, color="black")),
                    text=[str(cid)],
                    textposition="middle center",
                    textfont=dict(size=8, color=txt_col),
                    hovertext=f"Chunk={cid}<br>{u} → {v}",
                    hoverinfo="text",
                    opacity=0.2,
                    name="chunks",
                    customdata=[f"{u}|{v}"],
                ))

            edge_index[(u, v)] += 1

        # ---- time-step headers ----
        header_x, header_y, header_text = [], [], []
        if pos:
            max_y = max(y for (_, y) in pos.values())
            top_y = max_y + HEADER_PAD_Y
            for t in ts_values:
                header_x.append(ts_to_col[t] * COL_GAP)
                header_y.append(top_y)
                header_text.append(f"t={t}")

        header_trace = go.Scatter(
            x=header_x, y=header_y,
            mode="text",
            text=header_text,
            textfont=dict(size=FONT_HDR_SZ, color="black"),
            hoverinfo="skip",
            name="headers",
        )

        fig = go.Figure(data=edge_traces + circle_traces + [node_trace, header_trace])

        fig.update_layout(
            width=200 + n_ts * COL_GAP * 30,
            height=200 + n_ranks * ROW_GAP * 30,
            showlegend=False,
            margin=dict(l=5, r=5, t=5, b=5),
            hovermode="closest",
            plot_bgcolor="white",
        )

        fig.update_traces(hoverlabel=dict(bgcolor="white", font_size=12))

        xs = []
        ys = []
        for x, y in pos.values():
            xs.append(x)
            ys.append(y)
        for trace in circle_traces + [node_trace]:
            xs.extend(trace.x)
            ys.extend(trace.y)

        x_min, x_max = min(xs), max(xs)
        y_min, y_max = min(ys), max(ys)
        BUFFER = 3.0
        fig.update_xaxes(range=[x_min - BUFFER, x_max + BUFFER], scaleanchor="y", scaleratio=1, visible=False)
        fig.update_yaxes(range=[y_min - BUFFER, y_max + BUFFER], visible=False)

        fig.write_html(out_html, include_plotlyjs='cdn', full_html=True)
        with open(out_html, 'a') as f:
            f.write('''
<script src="highlight_hover.js"></script>
<script>
document.addEventListener('DOMContentLoaded', function() {
    const plotDiv = document.getElementsByClassName('plotly-graph-div')[0];
    attachHoverHighlight(plotDiv);
});
</script>
''')

# ---------------------------------------------------------------------------
# Thin functional wrappers (public API)
# ---------------------------------------------------------------------------

def build_rank_ts_index(input_dir: str) -> Dict[int, Dict[int, List[Dict[str, object]]]]:
    loader = ETLoader(input_dir)
    return loader.build_rank_ts_index()

def build_graph(input_dir: str) -> nx.MultiDiGraph:
    rank_ts = build_rank_ts_index(input_dir)
    builder = TENGraphBuilder(rank_ts)
    return builder.build_graph()

def visualize_graph_plotly(G, out_html: str | None = None):
    PlotlyTENVisualizer().visualize(G, out_html)

# ---------------------------------------------------------------------------
# CLI / main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="TEN-style visualization from Chakra ET files"
    )
    ap.add_argument(
        "--input_dir",
        default=".",
        help="Directory containing the *.et files (default: current directory)",
    )
    ap.add_argument(
        "--html",
        default="TEN_graph.html",
        help="Output HTML file (default: TEN_graph.html).",
    )
    args = ap.parse_args()

    # Build the graph
    G = build_graph(args.input_dir)

    # Resolve HTML path
    out_html = Path(args.html)
    out_html.parent.mkdir(parents=True, exist_ok=True)

    # Render with Plotly
    visualize_graph_plotly(G, str(out_html))
    print(f"Interactive HTML graph written to {out_html}")


if __name__ == "__main__":
    main()
