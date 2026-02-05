#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cstdlib>

// Position de la sphère
static float sx = 0.0f, sy = 0.0f, sz = 0.0f;

// Vitesse de déplacement
static const float step = 0.2f;

static GLUquadric *quad = nullptr;

void initGL()
{
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // Lumière simple (optionnel mais joli)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPos[] = {3.0f, 4.0f, 6.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
}

void reshape(int w, int h)
{
    if (h == 0)
        h = 1;
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w / (double)h, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void drawAxes(float len = 3.0f)
{
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    // X
    glColor3f(1, 0, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(len, 0, 0);
    // Y
    glColor3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0, len, 0);
    // Z
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, len);
    glEnd();
    glEnable(GL_LIGHTING);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Caméra (place-la comme tu veux)
    gluLookAt(
        0.0, 1.5, 8.0, // eye
        0.0, 0.0, 0.0, // center
        0.0, 1.0, 0.0  // up
    );

    // Objet: sphère
    glPushMatrix();
    glTranslatef(sx, sy, sz);
    glColor3f(0.8f, 0.85f, 1.0f);
    gluSphere(quad, 1.0, 40, 30);
    glPopMatrix();

    glutSwapBuffers();
}

// Flèches : X/Y
void specialKeys(int key, int /*x*/, int /*y*/)
{

    switch (key)
    {
    case GLUT_KEY_LEFT:
        sx -= step;
        break; // X-
    case GLUT_KEY_RIGHT:
        sx += step;
        break; // X+
    case GLUT_KEY_UP:
        sy += step;
        break; // Y+
    case GLUT_KEY_DOWN:
        sy -= step;
        break; // Y-
    }
    glutPostRedisplay();
}

// A/Q : Z
void keyboard(unsigned char key, int /*x*/, int /*y*/)
{

    switch (key)
    {
    case 27: // ESC
        if (quad)
            gluDeleteQuadric(quad);
        std::exit(0);
        break;
    case 'a':
    case 'A':
        sz += step; // Z+
        break;
    case 'q':
    case 'Q':
        sz -= step; // Z-
        break;
    }
    glutPostRedisplay();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(900, 700);
    glutCreateWindow("Sphere GLUT + GLU (Arrows + A/Q)");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    glutMainLoop();
    return 0;
}
