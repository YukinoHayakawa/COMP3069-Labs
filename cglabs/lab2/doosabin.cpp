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

struct Face
{
    vector<size_t> v_indices;
    vector<size_t> v_indices_old;
    size_t new_face_idx = 0;

    Vector3f centroid(const Polyhedron &p) const;

    bool has_edge(size_t vi0, size_t vi1) const
    {
        const auto i0 = find(v_indices.begin(), v_indices.end(), vi0);
        if(i0 == v_indices.end()) return false;
        const auto i1 = find(v_indices.begin(), v_indices.end(), vi1);
        if(i1 == v_indices.end()) return false;
        const auto d = abs(distance(i0, i1));
        return d == 1 || d == v_indices.size() - 1;
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
    vector<Face> faces;
    using Edge = pair<size_t, size_t>;
    set<Edge> processed_edges;
    Vector3f color = Vector3f::Ones();

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

    void draw() const
    {
        for(auto &&f : faces)
        {
            glBegin(GL_POLYGON);
            for(auto &&vi : f.v_indices)
            {
                const auto p = vertices[vi].pos;
                //glColor3fv((p + Vector3f::Ones()).normalized().cwiseAbs().eval().data());
                glColor3fv(color.data());
                glVertex3fv(p.data());
            }
            glEnd();
        }
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
    for(size_t fi = 0; fi < p.faces.size(); ++fi)
    {
        auto &f = p.faces[fi];
        // create E-faces for each edge
        for(size_t i = 0; i < f.v_indices.size() - 1; ++i)
        {
            const size_t vi0 = f.v_indices[i], vi1 = f.v_indices[i + 1];
            if(p.is_edge_processed(vi0, vi1))
                continue;
            // find adjacent face
            for(auto &&af : p.vertices[vi0].adj_faces) // or equivalently, use vi1
            {
                if(af == fi) continue;
                const auto &cf = p.faces[af];
                // adjacent face is found
                if(cf.has_edge(vi0, vi1))
                {
                    const auto &nf0 = s.faces[f.new_face_idx];
                    const auto &nf1 = s.faces[cf.new_face_idx];
                    // find corresponding new vertices on each face,
                    // to form a E-face
                    const auto evi0 = nf0.new_vertex_idx(vi1);
                    const auto evi1 = nf0.new_vertex_idx(vi0);
                    const auto evi2 = nf1.new_vertex_idx(vi0);
                    const auto evi3 = nf1.new_vertex_idx(vi1);
                    Face nf;
                    nf.v_indices = { evi0, evi1, evi2, evi3 };
                    s.faces.push_back(std::move(nf));
                    p.add_processed_edge(vi0, vi1);
                    break;
                }
            }
        }
    }

    /*
    // for each vertex - gen V-faces
    for(size_t i = 0; i < p.vertices.size(); ++i)
    {
        auto &v = p.vertices[i];
        Face nf;
        nf.v_indices.reserve(v.adj_faces.size());
        // find all connected faces
        for(auto &&fi : v.adj_faces)
        {
            // find corresponding new vertex in the new face
            size_t nvi = s.faces[p.faces[fi].new_face_idx].new_vertex_idx(i);
            nf.v_indices.push_back(nvi);
        }
        // connect all corresponding new vertices to form a V-face
        s.faces.push_back(std::move(nf));
    }*/
    return std::move(s);
}

Polyhedron box, sd;

void init_box()
{
    box.vertices =
    {
        { { -1, 1, 1 }, { 0, 2, 4 } },
        { { 1, 1, 1 }, { 0, 2, 5 } },
        { { -1, -1, 1 }, { 1, 2, 4 } },
        { { 1, -1, 1 }, { 1, 2, 5 } },
        { { -1, 1, -1 }, { 0, 3, 4 } },
        { { 1, 1, -1 }, { 0, 3, 5 } },
        { { -1, -1, -1 }, { 1, 3, 4 } },
        { { 1, -1, -1 }, { 1, 3, 5 } }
    };
    box.faces.insert(box.faces.end(), 6, { });
    box.faces[0].v_indices = { 0, 1, 5, 4 };
    box.faces[1].v_indices = { 2, 3, 7, 6 };
    box.faces[2].v_indices = { 0, 2, 3, 1 };
    box.faces[3].v_indices = { 4, 5, 7, 6 };
    box.faces[4].v_indices = { 0, 4, 6, 2 };
    box.faces[5].v_indices = { 1, 3, 7, 5 };
}

void reshape(int w, int h)
{
    GLfloat aspect = (GLfloat)w / (GLfloat)h;
    GLfloat nRange = 2.0f;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if(w <= h)
    {
        glOrtho(-nRange, nRange, -nRange * aspect, nRange * aspect, -nRange, nRange);
    }
    else
    {
        glOrtho(-nRange, nRange, -nRange / aspect, nRange / aspect, -nRange, nRange);
    }
}

void display(void)
{
    glDisable(GL_DEPTH);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    /*glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //gluPerspective(120, 1, -10, 10);*/
    reshape(500, 500);
    glRotatef(30, 1.0f, 0.0f, 0.0f);
    glRotatef(-30, 0.0f, 1.0f, 0.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // glTranslatef(0, 0, -5);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    box.draw();
    sd.draw();

    glutSwapBuffers();
}

int main(int argc, char **argv)
{
    init_box();
    sd = doo_sabin(box);
    sd.color = { 1, 1, 0 };

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Doo-Sabin");
    glewInit();
    glutDisplayFunc(display);
    glutMainLoop();
}
