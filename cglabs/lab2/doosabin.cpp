#include <GL/glew.h>
#include <GL/glut.h>
#include <Eigen/Dense>
#include <vector>
#include <set>
#include <ctime>
#include <fstream>
#include <map>
/*#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include <iostream>*/

using namespace Eigen;
using namespace std;
//using namespace fmt;

struct Polyhedron;
using Edge = pair<size_t, size_t>;
using AdjFaces = pair<size_t, size_t>;
using CrossEdge = pair<size_t, size_t>; // face-face

pair<size_t, size_t> make_ordered_pair(size_t vi0, size_t vi1)
{
    assert(vi0 != vi1);
    return vi0 > vi1 ? make_pair(vi1, vi0) : make_pair(vi0, vi1);
}

template <typename T1, typename T2>
ostream & operator <<(ostream &o, const pair<T1, T2> &t)
{
    return o << "(" << t.first << " " << t.second << ")";
}

struct Vertex
{
    Vector3f pos;
    vector<size_t> adj_faces;
    set<CrossEdge> crosses, cross_backup;

    void sort_adj_faces()
    {
        assert(crosses.size() == adj_faces.size());
        vector<size_t> sorted;
        cross_backup = crosses;
        sorted.reserve(adj_faces.size());
        // the order of below two lines determines winding order
        sorted.push_back(crosses.begin()->second);
        sorted.push_back(crosses.begin()->first);
        crosses.erase(crosses.begin());
        while(sorted.size() != adj_faces.size())
        {
            for(auto i = crosses.begin(); i != crosses.end(); ++i)
            {
                if(i->first == sorted.back() || i->first == sorted.front())
                {
                    sorted.push_back(i->second);
                    crosses.erase(i);
                    break;
                }
                if(i->second == sorted.back() || i->second == sorted.front())
                {
                    sorted.push_back(i->first);
                    crosses.erase(i);
                    break;
                }
            }
        }
        /*if(sorted.size() == 4)
            swap(sorted[2], sorted[3]);*/
        //fmt::fprintf(cout, "adj {}\nsrt {}\n crs {}\n", adj_faces, sorted, cross_backup);
        adj_faces = std::move(sorted);
    }
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
    size_t num_edges = 0;

    map<Edge, AdjFaces> adjacent_faces;

    Vector3f color = Vector3f::Ones();

    Vector3f v(size_t i) const
    {
        return vertices[i].pos;
    }

    bool is_edge_processed(size_t vi0, size_t vi1) const
    {
        return adjacent_faces.find(make_ordered_pair(vi0, vi1))
            != adjacent_faces.end();
    }

    void add_adj_faces(size_t vi0, size_t vi1, size_t f0, size_t f1)
    {
        adjacent_faces.insert({ make_ordered_pair(vi0, vi1), { f0, f1 }});
    }

    void draw() const
    {
        for(auto &&f : faces)
        {
            //float t = 0;
            glBegin(GL_POLYGON);
            for(auto &&vi : f.v_indices)
            {
                const auto p = vertices[vi].pos;
                //glColor3fv((p + Vector3f::Ones()).normalized().cwiseAbs().eval().data());
                glColor3fv(color.data());
                glVertex3fv(p.data());
              //  t += 0.25f;
            }
            glEnd();
        }
    }

    void write_obj(const string &path)
    {
        ofstream out(path);
        out << "# e = " << num_edges << "\n";
        for(auto &&v : vertices)
        {
            out << "# adj: ";
            for(auto &&a : v.adj_faces)
                out << a << " ";
            out << "# crs: ";
            for(auto &&c : v.cross_backup)
                out << c.first << "-" << c.second << " ";
            out << "\n";
            out << "v " << v.pos.x() << " " << v.pos.y() << " " << v.pos.z() << "\n";
        }
        for(auto &&f : faces)
        {
            out << "f ";
            for(auto &&v : f.v_indices)
            {
                out << v + 1 << " ";
            }
            out << "\n";
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
            s.vertices.back().adj_faces.push_back(s.faces.size());
        }
        f.new_face_idx = s.faces.size();
        // create the new face
        s.faces.push_back(std::move(nf));
    }

    // second pass on faces to process edges
    for(size_t fi = 0; fi < p.faces.size(); ++fi)
    {
        auto &f = p.faces[fi];
        assert(f.v_indices.size() >= 3);
        // create E-faces for each edge
        for(size_t i = 0; i < f.v_indices.size(); ++i)
        {
            const size_t vi0 = f.v_indices[i], vi1 = i == f.v_indices.size() -1 ? f.v_indices[0] : f.v_indices[i + 1];
            if(p.is_edge_processed(vi0, vi1))
                continue;
            bool found = false;
            // find adjacent face by v0 and v1
            for(auto &&af : p.vertices[vi0].adj_faces) // or equivalently, use vi1
            {
                if(af == fi) continue;
                const auto &cf = p.faces[af];
                // adjacent face is found
                if(cf.has_edge(vi0, vi1))
                {
                    found = true;
                    ++p.num_edges;
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
                    s.vertices[evi0].adj_faces.push_back(s.faces.size());
                    s.vertices[evi1].adj_faces.push_back(s.faces.size());
                    s.vertices[evi2].adj_faces.push_back(s.faces.size());
                    s.vertices[evi3].adj_faces.push_back(s.faces.size());
                    s.faces.push_back(std::move(nf));
                    p.add_adj_faces(vi0, vi1, fi, af);
                    p.vertices[vi0].crosses.insert(make_ordered_pair(fi, af));
                    p.vertices[vi1].crosses.insert(make_ordered_pair(fi, af));
                    break;
                }
            }
            assert(found);
        }
    }

    // for each vertex - gen V-faces
    for(size_t i = 0; i < p.vertices.size(); ++i)
    {
        auto &v = p.vertices[i];
        v.sort_adj_faces();
        Face nf;
        nf.v_indices.reserve(v.adj_faces.size());
        // find all connected faces
        for(auto &&fi : v.adj_faces)
        {
            // find corresponding new vertex in the new face
            size_t nvi = s.faces[p.faces[fi].new_face_idx].new_vertex_idx(i);
            s.vertices[nvi].adj_faces.push_back(s.faces.size());
            nf.v_indices.push_back(nvi);
        }
        // connect all corresponding new vertices to form a V-face
        s.faces.push_back(std::move(nf));
    }

    assert(s.vertices.size() == 2 * p.num_edges);
    assert(s.faces.size() == p.faces.size() + p.vertices.size() + p.num_edges);
    return std::move(s);
}

Polyhedron box, sd, sd2, sd3;

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
    gluPerspective(60, (float)w / h, 1, 100);
}

int xx = 0, yy = 0;

void mouse(int x, int y)
{
    xx += x;
    yy += y;
}

void display(void)
{
    glLineWidth(5);
    glEnable(GL_DEPTH);
    //glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glCullFace(GL_BACK);
    /*glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //gluPerspective(120, 1, -10, 10);*/
    reshape(1500, 1500);
    glMatrixMode(GL_MODELVIEW); // world-to-camera == inv(camera to world)
    glLoadIdentity();
    glTranslatef(0, 0, -5);
    glRotatef(15, 1.0f, 0.0f, 0.0f);
    glRotatef(clock() / (float)CLOCKS_PER_SEC * 10, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    box.draw();
    sd.draw();
    sd2.draw();
    sd3.draw();

    glutSwapBuffers();
    glutPostRedisplay();
}

void Key(unsigned char key, int x, int y)
{

}

int main(int argc, char **argv)
{
    init_box();

    sd = doo_sabin(box);
    box.write_obj("box.obj");

    sd.color = { 1, 1, 0 };
    sd.write_obj("sdp.obj");

    sd2 = doo_sabin(sd);
    sd2.color = { 1, 0, 1 };
    sd.write_obj("sd.obj");

    sd3 = doo_sabin(sd2);
    sd3.color = { 0, 1, 1 };
    sd2.write_obj("sd2.obj");

    sd3.write_obj("sd3.obj");

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1500, 1500);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Doo-Sabin");
    glewInit();
    glutKeyboardFunc(Key);
    glutMotionFunc(mouse);
    glutDisplayFunc(display);
    glutMainLoop();
}
