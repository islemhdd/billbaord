#include <GL/glut.h>
#include <GL/freeglut.h>
#include <random>
#include <vector>
#include <array>
struct V
{
    float x, y, r, g, b;
};
typedef std::array<V, 4> quad; //?type ta3 tablau de vertex
std::vector<quad> quads;       // list tae quads
// generation de random b seeder bayn :
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(0.0, 1.0);

void keyboard(unsigned char c, int x, int y)
{
    if (c == 23) // hadi ctr+w ,//! ps : ctrl+char=char-64
    {
        exit(0);
    }
}
void mouse(int button, int state, int x, int y)
{ //?add a random shape par le stocker fl vector
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
    {
        quad q;

        for (int i = 0; i < 4; i++)
        {
            q[i] = {dist(gen), dist(gen), dist(gen), dist(gen), dist(gen)};
        }
        quads.push_back(q);

        glutPostRedisplay();
    }
}

void display(void)
{

    glClear(GL_COLOR_BUFFER_BIT); // tnahi previous drawing
    glBegin(GL_QUADS);
    for (quad &q : quads)
    {
        for (V &v : q)
        {
            glColor3f(v.r, v.g, v.b);
            glVertex2f(v.x, v.y);
        }
        }
    glEnd();
    glutSwapBuffers();
}
void startwindow(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA); // mode de screen
    // glutInitWindowPosition(100, 100);
    glutInitWindowSize(640, 400);
    glutCreateWindow("learn");
}

int main(int argc, char **argv)
{
    startwindow(argc, argv);

    glutDisplayFunc(display); // lazm
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMainLoop();

    return 0;
}