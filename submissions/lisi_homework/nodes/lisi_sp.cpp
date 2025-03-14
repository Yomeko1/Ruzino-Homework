#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "GCore/Components/MeshOperand.h"
#include "GCore/GOP.h"
#include "nodes/core/def/node_def.hpp"
#include "simple_util.h"

#include "Eigen/Dense"
namespace Dij {

/// Graph
struct GraphBase {
    struct Edge {
        size_t pa, pb;
        double weight;
        Edge() : pa(0), pb(0), weight(0.0) { };
        template<typename T, typename D, typename F>
        Edge(T a, D b, F w) : pa(a),
                              pb(b),
                              weight(w)
        {
        }
        bool operator==(const Edge& rhs)
        {
            return pa == rhs.pa && pb == rhs.pb;
        }
    };

    std::vector<Edge*> edge_buffer;
    std::vector<std::vector<Edge*>> sites_edges;
    size_t num_sites;
    GraphBase(size_t p_sites) : num_sites(p_sites), sites_edges(p_sites)
    {
        if (p_sites == 0)
            throw std::exception("Number of sites cannot be zero!");
    }
    ~GraphBase()
    {
        for (auto e : edge_buffer)
            delete e;
    }
    GraphBase(const GraphBase& rhs) : GraphBase(rhs.num_sites)
    {
        for (auto edge : rhs.edge_buffer) {
            AddConnection(edge->pa, edge->pb, edge->weight);
        }
    }
    GraphBase(const GraphBase&& rhs) noexcept : GraphBase(rhs.num_sites)
    {
        for (auto edge : rhs.edge_buffer) {
            AddConnection(edge->pa, edge->pb, edge->weight);
        }
    }

    virtual void AddConnection(size_t psitea, size_t psiteb, double weight) { };
    virtual void DeleteConnectionSilently(size_t pa, size_t pb) { };
    virtual void
    ChangeConnectionWeightSilently(size_t pa, size_t pb, double weight) { };
    virtual size_t GetToIdx(Edge* e_ptr, size_t selfidx)
    {
        return 0;
    };

    template<typename Func>
    std::vector<bool> BFSDo(size_t startat, Func f)
    {
        std::vector<bool> visited(num_sites, false);
        if (num_sites == 0)
            return visited;
        std::queue<size_t> next_id;
        next_id.push(startat);
        while (!next_id.empty()) {
            auto top_id = next_id.front();
            next_id.pop();
            f(top_id, this);
            visited[top_id] = true;
            for (auto e : sites_edges[top_id]) {
                auto next = GetToIdx(e, top_id);
                if (!visited[next])
                    next_id.push(next);
            }
        }
        return visited;
    }
};

struct Undigraph : GraphBase {
    using GraphBase::GraphBase;

    virtual void AddConnection(size_t psitea, size_t psiteb, double weight)
        override
    {
        if (psitea >= num_sites || psiteb >= num_sites)
            throw std::exception("Index out of range");
        if (psitea == psiteb)
            throw std::exception("Cannot connect self to self");
        if (weight <= 0)
            throw std::exception("Weight must be > 0");
        auto e = new Edge(
            std::min(psitea, psiteb), std::max(psitea, psiteb), weight);
        edge_buffer.push_back(e);
        sites_edges[psitea].push_back(e);
        sites_edges[psiteb].push_back(e);
    }
    virtual void DeleteConnectionSilently(size_t pa, size_t pb) override
    {
        if (pb >= num_sites || pa >= num_sites)
            throw std::exception("Index out of range");
        if (pa == pb)
            throw std::exception("There is no connection with itself");
        size_t a = std::min(pa, pb);
        size_t b = std::max(pa, pb);
        for (size_t i = 0; i < edge_buffer.size(); ++i) {
            auto e = edge_buffer[i];
            if (e->pa == a && e->pb == b) {
                auto iter1 =
                    std::find(sites_edges[a].begin(), sites_edges[a].end(), e);
                auto iter2 =
                    std::find(sites_edges[b].begin(), sites_edges[b].end(), e);
                sites_edges[a].erase(iter1);
                sites_edges[b].erase(iter2);
                edge_buffer.erase(edge_buffer.begin() + i);
                delete e;
                return;
            }
        }
    }
    virtual void
    ChangeConnectionWeightSilently(size_t pa, size_t pb, double weight) override
    {
        if (pb >= num_sites || pa >= num_sites)
            throw std::exception("Index out of range");
        if (pa == pb)
            throw std::exception("There is no connection with itself");
        if (weight <= 0)
            throw std::exception("Weight must be > 0");

        size_t a = std::min(pa, pb);
        size_t b = std::max(pa, pb);
        for (size_t i = 0; i < edge_buffer.size(); ++i) {
            auto e = edge_buffer[i];
            if (e->pa == a && e->pb == b) {
                e->weight = weight;
                return;
            }
        }
    }
    virtual size_t GetToIdx(Edge* e_ptr, size_t selfidx) override
    {
        return e_ptr->pa == selfidx ? e_ptr->pb : e_ptr->pa;
    }
};

struct Digraph : GraphBase {
    using GraphBase::GraphBase;

    virtual void AddConnection(size_t psitea, size_t psiteb, double weight)
        override
    {
        if (psitea >= num_sites || psiteb >= num_sites)
            throw std::exception("Index out of range");
        if (psitea == psiteb)
            throw std::exception("Cannot connect self to self");
        if (weight <= 0)
            throw std::exception("Weight must be > 0");

        auto e = new Edge(psitea, psiteb, weight);
        edge_buffer.push_back(e);
        sites_edges[psitea].push_back(e);
    }
    virtual void DeleteConnectionSilently(size_t pa, size_t pb) override
    {
        if (pb >= num_sites || pa >= num_sites)
            throw std::exception("Index out of range");
        if (pa == pb)
            throw std::exception("There is no connection with itself");
        for (size_t i = 0; i < edge_buffer.size(); ++i) {
            auto e = edge_buffer[i];
            if (e->pa == pa && e->pb == pb) {
                auto iter1 = std::find(
                    sites_edges[pa].begin(), sites_edges[pa].end(), e);
                sites_edges[pa].erase(iter1);
                edge_buffer.erase(edge_buffer.begin() + i);
                delete e;
                return;
            }
        }
    }
    virtual void
    ChangeConnectionWeightSilently(size_t pa, size_t pb, double weight) override
    {
        if (pb >= num_sites || pa >= num_sites)
            throw std::exception("Index out of range");
        if (pa == pb)
            throw std::exception("There is no connection with itself");
        if (weight <= 0)
            throw std::exception("Weight must be > 0");

        for (size_t i = 0; i < edge_buffer.size(); ++i) {
            auto e = edge_buffer[i];
            if (e->pa == pa && e->pb == pb) {
                e->weight = weight;
                return;
            }
        }
    }
    virtual size_t GetToIdx(Edge* e_ptr, size_t selfidx) override
    {
        return e_ptr->pb;
    }
};

std::ostream& operator<<(std::ostream& ost, GraphBase& g)
{
    for (size_t k = 0; k < g.sites_edges.size(); ++k) {
        ost << "Site index: " << k << "\tConnects to:\n";
        for (auto e : g.sites_edges[k]) {
            ost << g.GetToIdx(e, k) << "(" << e->weight << ")\t";
        }
        ost << "\n";
    }
    return ost;
}
std::ostream& operator<<(std::ostream& ost, GraphBase&& g)
{
    for (size_t k = 0; k < g.sites_edges.size(); ++k) {
        ost << "Site index: " << k << "\tConnects to:\n";
        for (auto e : g.sites_edges[k]) {
            ost << g.GetToIdx(e, k) << "(" << e->weight << ")\t";
        }
        ost << "\n";
    }
    return ost;
}

/// Dijkstra
struct DijkstraRet {
    size_t beg, graphsize;
    std::vector<size_t> pre;
    std::vector<double> distance;
};

DijkstraRet BasicDijkstraSolverNoCheck(size_t startidx, GraphBase& graph)
{
    if (startidx >= graph.num_sites)
        throw std::exception("Index is out of range!");
    std::vector<bool> flag_in_U(graph.num_sites, false);
    std::vector<std::pair<double, size_t>> edge_vtx_pair_heap;
    edge_vtx_pair_heap.push_back(std::make_pair(0.0, startidx));
    std::vector<size_t> pre(graph.num_sites, graph.num_sites);
    std::vector<double> dist(graph.num_sites, -1.0);
    dist[startidx] = 0;
    const auto cmp = [](std::pair<double, size_t>& a,
                        std::pair<double, size_t>& b) {
        return a.first > b.first;
    };

    while (!edge_vtx_pair_heap.empty()) {
        auto top = edge_vtx_pair_heap[0];

        std::pop_heap(
            edge_vtx_pair_heap.begin(), edge_vtx_pair_heap.end(), cmp);
        edge_vtx_pair_heap.pop_back();
        if (flag_in_U[top.second])
            continue;
        flag_in_U[top.second] = true;
        for (auto e : graph.sites_edges[top.second]) {
            size_t idx_to = graph.GetToIdx(e, top.second);
            if (!flag_in_U[idx_to] &&
                (dist[idx_to] < 0 ||
                 dist[idx_to] > dist[top.second] + e->weight)) {
                dist[idx_to] = dist[top.second] + e->weight;
                pre[idx_to] = top.second;
                edge_vtx_pair_heap.push_back(
                    std::make_pair(dist[idx_to], idx_to));
                std::push_heap(
                    edge_vtx_pair_heap.begin(), edge_vtx_pair_heap.end(), cmp);
            }
        }
    }

    return { startidx, graph.num_sites, pre, dist };
}

struct DijkstraResParsed {
    std::vector<size_t> seq;
    double dist;
    DijkstraResParsed() : dist(0)
    {
    }
    template<typename Vec>
    DijkstraResParsed(Vec&& v, double distance) : dist(distance),
                                                  seq(v)
    {
    }
};

template<typename Ret>
DijkstraResParsed DijResParser(Ret&& ret, size_t target)
{
    size_t rt = target;
    if (target == ret.beg)
        return DijkstraResParsed();
    if (target >= ret.graphsize)
        throw std::exception("Index out of range");
    std::vector<size_t> res;
    while (target != ret.graphsize) {
        res.push_back(target);
        target = ret.pre[target];
    }
    std::reverse(res.begin(), res.end());
    return { res, ret.distance[rt] };
}

std::ostream& operator<<(std::ostream& ost, DijkstraResParsed& g)
{
    for (size_t i = 0; i < g.seq.size(); ++i) {
        if (i != 0)
            ost << "->";
        ost << g.seq[i];
    }
    ost << "\n";
    ost << "Distance: " << g.dist << "\n";
    return ost;
}
std::ostream& operator<<(std::ostream& ost, DijkstraResParsed&& g)
{
    for (size_t i = 0; i < g.seq.size(); ++i) {
        if (i != 0)
            ost << "->";
        ost << g.seq[i];
    }
    ost << "\n";
    ost << "Distance: " << g.dist << "\n";
    return ost;
}
std::ostream& operator<<(std::ostream& ost, DijkstraRet& ret)
{
    ost << "Begin at: " << ret.beg << "\n";
    for (size_t i = 0; i < ret.graphsize; ++i) {
        if (i == ret.beg)
            continue;
        auto r = DijResParser(ret, i);
        ost << r;
    }
    return ost;
}
std::ostream& operator<<(std::ostream& ost, DijkstraRet&& ret)
{
    ost << "Begin at: " << ret.beg << "\n";
    for (size_t i = 0; i < ret.graphsize; ++i) {
        if (i == ret.beg)
            continue;
        auto r = DijResParser(ret, i);
        ost << r;
    }
    return ost;
}

template<class G>
std::optional<DijkstraResParsed>
DijkstraSolver(size_t startidx, size_t endidx, G& graph)
{
    std::cout << "BFS Check...\n";
    auto traverse_res = graph.BFSDo(startidx, [](size_t idx, GraphBase* g) { });

    if (!traverse_res[endidx])
        return {};
    size_t now_cnt = 0;
    std::unordered_map<size_t, size_t> now_original, original_now;
    /// Sites remapping
    for (size_t k = 0; k < traverse_res.size(); ++k)
        if (traverse_res[k]) {
            now_original[now_cnt] = k;
            original_now[k] = now_cnt;
            ++now_cnt;
        }

    /// Simplify the graph
    std::cout << "Simplify the graph...\n";
    G simgraph(graph.num_sites);
    for (auto& e : graph.edge_buffer)
        if (traverse_res[e->pa] && traverse_res[e->pb]) {
            simgraph.AddConnection(
                original_now[e->pa], original_now[e->pb], e->weight);
        }

    /// Calculate
    std::cout << "Calc...\n";
    auto parsed = DijResParser(
        BasicDijkstraSolverNoCheck(original_now[startidx], simgraph),
        original_now[endidx]);
    for (auto& v : parsed.seq)
        v = now_original[v];
    return { parsed };
}


template<class G>
std::optional<DijkstraResParsed>
DijkstraSolverNoCheck(size_t startidx, size_t endidx, G& graph)
{
    /// Calculate
    std::cout << "Calc...\n";
    auto parsed = DijResParser(
        BasicDijkstraSolverNoCheck(startidx, graph),
        endidx);
    return { parsed };
}



}  // namespace Dij

typedef OpenMesh::TriMesh_ArrayKernelT<> MyMesh;

// Return true if the shortest path exists, and fill in the shortest path
// vertices and the distance. Otherwise, return false.
bool find_shortest_path(
    const MyMesh::VertexHandle& start_vertex_handle,
    const MyMesh::VertexHandle& end_vertex_handle,
    const MyMesh& omesh,
    std::list<size_t>& shortest_path_vertex_indices,
    float& distance)
{
    // TODO: Implement the shortest path algorithm
    // You need to fill in `shortest_path_vertex_indices` and `distance`
    size_t num_vert = omesh.n_vertices();
    auto graph = Dij::Undigraph(num_vert);
    std::cout << "Generating the graph...\n";
    for (MyMesh::EdgeIter ed = omesh.edges_begin(); ed != omesh.edges_end(); ed++)
    {
        size_t idx1 = ed->v0().idx();
        size_t idx2 = ed -> v1().idx();
        MyMesh::Point p1 = omesh.point(ed->v0());
        MyMesh::Point p2 = omesh.point(ed->v1());
        auto w = (p1-p2).norm();
        graph.AddConnection(idx1, idx2,w);
    }
    std::cout << "Solving the problem...\n";
    auto dijres = Dij::DijkstraSolverNoCheck(start_vertex_handle.idx(), end_vertex_handle.idx(), graph);

    std::cout << "Parsing the result...\n";

    if (dijres.has_value())
    {
        auto res = dijres.value();
        for (auto v : res.seq) shortest_path_vertex_indices.push_back(v);
        distance = res.dist;
        return true;
    }
    return false;
}

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(lisi_shortest_path)
{
    b.add_input<std::string>("Picked Mesh [0] Name");
    b.add_input<std::string>("Picked Mesh [1] Name");
    b.add_input<Geometry>("Picked Mesh");
    b.add_input<size_t>("Picked Vertex [0] Index");
    b.add_input<size_t>("Picked Vertex [1] Index");

    b.add_output<std::list<size_t>>("Shortest Path Vertex Indices");
    b.add_output<float>("Shortest Path Distance");
}

NODE_EXECUTION_FUNCTION(lisi_shortest_path)
{
    foo();
    auto picked_mesh_0_name =
        params.get_input<std::string>("Picked Mesh [0] Name");
    auto picked_mesh_1_name =
        params.get_input<std::string>("Picked Mesh [1] Name");
    // Ensure that the two picked meshes are the same
    if (picked_mesh_0_name != picked_mesh_1_name) {
        std::cerr << "Ensure that the two picked meshes are the same"
                  << std::endl;
        return false;
    }

    auto mesh = params.get_input<Geometry>("Picked Mesh")
                    .get_component<MeshComponent>();
    auto vertices = mesh->get_vertices();
    auto face_vertex_counts = mesh->get_face_vertex_counts();
    auto face_vertex_indices = mesh->get_face_vertex_indices();

    // Convert the mesh to OpenMesh
    MyMesh omesh;
    // Add vertices
    std::vector<OpenMesh::VertexHandle> vhandles;
    for (size_t i = 0; i < vertices.size(); i++) {
        omesh.add_vertex(
            OpenMesh::Vec3f(vertices[i][0], vertices[i][1], vertices[i][2]));
    }
    // Add faces
    size_t start = 0;
    for (size_t i = 0; i < face_vertex_counts.size(); i++) {
        std::vector<OpenMesh::VertexHandle> face;
        for (size_t j = 0; j < face_vertex_counts[i]; j++) {
            face.push_back(
                OpenMesh::VertexHandle(face_vertex_indices[start + j]));
        }
        omesh.add_face(face);
        start += face_vertex_counts[i];
    }

    auto start_vertex_index =
        params.get_input<size_t>("Picked Vertex [0] Index");
    auto end_vertex_index = params.get_input<size_t>("Picked Vertex [1] Index");

    // Turn the vertex indices into OpenMesh vertex handles
    OpenMesh::VertexHandle start_vertex_handle(start_vertex_index);
    OpenMesh::VertexHandle end_vertex_handle(end_vertex_index);

    // The indices of the vertices on the shortest path, including the start and
    // end vertices
    std::list<size_t> shortest_path_vertex_indices;

    // The distance of the shortest path
    float distance = 0.0f;

    if (find_shortest_path(
            start_vertex_handle,
            end_vertex_handle,
            omesh,
            shortest_path_vertex_indices,
            distance)) {
        params.set_output(
            "Shortest Path Vertex Indices", shortest_path_vertex_indices);
        params.set_output("Shortest Path Distance", distance);
        return true;
    }
    else {
        params.set_output("Shortest Path Vertex Indices", std::list<size_t>());
        params.set_output("Shortest Path Distance", 0.0f);
        return false;
    }
    Eigen::Vector3f vec{1.,2.,3.};
    vec.setZero();
    return true;
}

NODE_DECLARATION_UI(lisi_shortest_path);
NODE_DECLARATION_REQUIRED(lisi_shortest_path);

NODE_DEF_CLOSE_SCOPE