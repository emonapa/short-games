import json
from dataclasses import dataclass
from typing import Any, Dict, List, Tuple


@dataclass
class SavedGraph:
    vertices: List[Tuple[float, float]]          # index = vertex id
    edges: List[Tuple[int, int, int]]            # (u, v, color)
    current_color: int = 0
    pending_u: int | None = None


def serialize_scene(scene) -> Dict[str, Any]:
    n = int(scene.g.num_vertices)

    vertices = [(scene.vertex_pos[i].x(), scene.vertex_pos[i].y()) for i in range(n)]
    edges = []

    for e_obj in scene.edge_items:
        edges.append((int(e_obj.u), int(e_obj.v), int(e_obj.color)))

    data: Dict[str, Any] = {
        "version": 1,
        "vertices": vertices,
        "edges": edges,
        "current_color": int(scene.current_color),
        "pending_u": None if scene.pending_u is None else int(scene.pending_u),
    }
    return data


def deserialize_scene(scene, data: Dict[str, Any]) -> None:
    if not isinstance(data, dict) or data.get("version") != 1:
        raise ValueError("Unsupported save format")

    vertices = data.get("vertices", [])
    edges = data.get("edges", [])

    if not isinstance(vertices, list) or not isinstance(edges, list):
        raise ValueError("Invalid save data")

    scene.clear_graph()

    if len(vertices) == 0:
        return

    # vertex 0
    x0, y0 = vertices[0]
    scene.vertex_pos[0].setX(float(x0))
    scene.vertex_pos[0].setY(float(y0))
    scene.is_ground[0] = True

    if scene.vertex_items[0] is not None:
        scene.removeItem(scene.vertex_items[0])
        scene.vertex_items[0] = None
    scene._render_vertex(0, is_ground=True)

    # zbytek vrcholu
    for i in range(1, len(vertices)):
        if i >= len(scene.vertex_pos):
            break
        x, y = vertices[i]
        scene.vertex_pos[i].setX(float(x))
        scene.vertex_pos[i].setY(float(y))
        scene.g.num_vertices = i + 1

        is_gnd = (float(y) == scene.ground_y)
        scene.is_ground[i] = is_gnd
        scene._render_vertex(i, is_ground=is_gnd)

    scene.parallel_count.clear()
    scene.g.num_edges = 0
    scene.edge_items.clear()

    for (u, v, color) in edges:
        scene._add_edge(int(u), int(v), int(color))

    scene.current_color = int(data.get("current_color", scene.current_color))
    pu = data.get("pending_u", None)
    scene.pending_u = None if pu is None else int(pu)
    scene._update_pending_marker()

def save_to_file(scene, path: str) -> None:
    data = serialize_scene(scene)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


def load_from_file(scene, path: str) -> None:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    deserialize_scene(scene, data)
