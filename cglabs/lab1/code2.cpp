#include <GL/glew.h>
#include <GL/glut.h>

void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(0.0, 1.0, 0.0);
    glColor3f(0.0, 1.0, 0.0);
    glVertex3f(-0.5, 0.0, 0.0);
    glColor3f(1.0, 0.0, 0.0);
    glVertex3f(0.5, 0.0, 0.0);
    glEnd();
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
