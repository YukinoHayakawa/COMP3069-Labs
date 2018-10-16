#include <GL/glew.h>
#include <GL/glut.h>

void display(void)
{
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Create Empty Window");
    glewInit();
    glutDisplayFunc(display);
    glutMainLoop();
}
