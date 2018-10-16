#include <GL/glew.h>
#include <GL/glut.h>
#include <Eigen/Dense>
#include <vector>
#include <set>

using namespace Eigen;
using namespace std;

struct Polyhedron;

struct Vertex
{
    Vector3f pos;
    set<size_t> adj_faces;
};

struct Edge
{
    size_t v0, v1;
};

struct Face
{
    vector<size_t> v_indices;
    vector<size_t> v_indices_old;
    size_t new_face_idx = -1;

    Vector3f centroid(const Polyhedron &p) const;

    bool has_edge(size_t vi0, size_t vi1) const
    {
        const auto i0 = find(v_indices.begin(), v_indices.end(), vi0);
        if(i0 == v_indices.end()) return false;
        const auto i1 = find(v_indices.begin(), v_indices.end(), vi1);
        if(i1 == v_indices.end()) return false;
        return abs(distance(i0, i1)) == 1;
    }

    size_t new_vertex_idx(size_t old) const
    {
        const auto i = find(v_indices_old.begin(), v_indices_old.end(), old);
        return v_indices[i - v_indices_old.begin()];
    }
};

struct Polyhedron
{
    vector<Vertex> vertices;
    vector<Edge> edges;
    vector<Face> faces;
    using Edge = pair<size_t, size_t>;
    set<Edge> processed_edges;

    Vector3f v(size_t i) const
    {
        return vertices[i].pos;
    }

    static Edge make_edge(size_t vi0, size_t vi1)
    {
        assert(vi0 != vi1);
        return vi0 > vi1 ? make_pair(vi1, vi0) : make_pair(vi0, vi1);
    }

    bool is_edge_processed(size_t vi0, size_t vi1) const
    {
        return processed_edges.find(make_edge(vi0, vi1))
            != processed_edges.end();
    }

    void add_processed_edge(size_t vi0, size_t vi1)
    {
        processed_edges.insert(make_edge(vi0, vi1));
    }
};

Vector3f Face::centroid(const Polyhedron &p) const
{
    Vector3f sum = Vector3f::Zero();
    for(auto &&vi : v_indices)
    {
        sum += p.v(vi);
    }
    return sum / static_cast<float>(v_indices.size());
}

template <typename V, typename T>
constexpr V lerp(const V &v0, const V &v1, const T t)
{
    return (1 - t) * v0 + t * v1;
}

Polyhedron doo_sabin(Polyhedron &p, float t = 0.5f)
{
    Polyhedron s;
    // for each face - gen F-faces, this is the only step that creates new
    // vertices
    for(auto &&f : p.faces)
    {
        // calc centroid
        const auto centroid = f.centroid(p);
        // calc new vertices on mid points to the centroid
        Face nf;
        nf.v_indices.reserve(f.v_indices.size());
        nf.v_indices_old.reserve(f.v_indices.size());
        for(auto &&vi : f.v_indices)
        {
            const auto nv = lerp(centroid, p.v(vi), t);
            nf.v_indices.push_back(s.vertices.size());
            nf.v_indices_old.push_back(vi);
            s.vertices.push_back({ nv });
            s.vertices.back().adj_faces.insert(s.faces.size());
        }
        f.new_face_idx = s.faces.size();
        // create the new face
        s.faces.push_back(std::move(nf));
    }

    // second pass on faces to process edges
    for(auto &&f : p.faces)
    {
        // create E-faces for each edge
        for(size_t i = 0; i < f.v_indices.size() - 1; ++i)
        {
            const size_t vi0 = f.v_indices[i], vi1 = f.v_indices[i + 1];
            if(p.is_edge_processed(vi0, vi1))
                continue;
            // find adjacent face
            for(auto &&af : p.vertices[vi0].adj_faces) // or equivalently, use vi1
            {
                const auto &cf = p.faces[af];
                // adjacent face is found
                if(cf.has_edge(vi0, vi1))
                {
                    const auto &nf0 = s.faces[f.new_face_idx];
                    const auto &nf1 = s.faces[cf.new_face_idx];
                    // find corresponding new vertices on each face,
                    // to form a E-face
                    const auto evi0 = nf0.new_vertex_idx(vi0);
                    const auto evi1 = nf0.new_vertex_idx(vi1);
                    const auto evi2 = nf1.new_vertex_idx(vi1);
                    const auto evi3 = nf1.new_vertex_idx(vi0);
                    Face nf;
                    nf.v_indices = { evi0, evi1, evi2, evi3 };
                    s.faces.push_back(std::move(nf));
                }
            }
        }
    }

    // for each vertex - gen V-faces
    for(auto &&v : p.vertices)
    {
        Face nf;
        nf.v_indices.reserve(v.adj_faces.size());
        size_t i = 0;
        // find all connected faces
        for(auto &&fi : v.adj_faces)
        {
            // find corresponding new vertex in the new face
            size_t nvi = s.faces[p.faces[fi].new_face_idx].v_indices[i];
            nf.v_indices.push_back(nvi);
            ++i;
        }
        // connect all corresponding new vertices to form a V-face
        s.faces.push_back(std::move(nf));
    }
}

void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glutWireCube(1.f);
    glutSwapBuffers();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Triangle");
    glewInit();
    glutDisplayFunc(display);
    glutMainLoop();
}
