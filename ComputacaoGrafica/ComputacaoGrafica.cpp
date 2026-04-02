#include <iostream>
#include <GL/freeglut.h>
#include <vector>
#include <cmath>

// Estrutura para 3 dimensões [cite: 2, 3]
struct Vertice3D {
    double x, y, z;
};

using aresta = std::pair<int, int>;
using lista_arestas = std::vector<aresta>;
using lista_vertices = std::vector<Vertice3D>;

struct Cubo {
    Vertice3D posicao; // Centro do polígono 
    lista_vertices vertices;
    lista_arestas arestas;
};

// Protótipos
void display();
void keyboard(unsigned char key, int x, int y);
void movimentar(Cubo& c, double dx, double dy, double dz);
void escalar(Cubo& c, double fator);
void rotacionarY(Cubo& c, double angulo);
void rotacionarX(Cubo& c, double angulo);
void desenhar(Cubo c);

// Global
Cubo meuCubo;

// Descrição direta de vértices e arestas [cite: 2]
Cubo criar_cubo(double x, double y, double z, double tam) {
    Cubo c;
    c.posicao = { x, y, z };
    double d = tam / 2.0;

    // 8 vértices [cite: 2]
    c.vertices = {
        {x - d, y - d, z - d}, {x + d, y - d, z - d}, {x + d, y + d, z - d}, {x - d, y + d, z - d},
        {x - d, y - d, z + d}, {x + d, y - d, z + d}, {x + d, y + d, z + d}, {x - d, y + d, z + d}
    };

    // 12 arestas para wireframe 
    c.arestas = {
        {0,1}, {1,2}, {2,3}, {3,0}, // Fundo
        {4,5}, {5,6}, {6,7}, {7,4}, // Frente
        {0,4}, {1,5}, {2,6}, {3,7}  // Conectores
    };
    return c;
}

int main(int argc, char** argv) {
    meuCubo = criar_cubo(0, 0, 0, 100);

    glutInit(&argc, argv);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Trabalho CG - Cubo 3D");

    glClearColor(1.0, 1.0, 1.0, 1.0);
    // Volume de visualização para 3D
    glOrtho(-300, 300, -300, 300, -300, 300);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    desenhar(meuCubo);
    glFlush();
}

// Eventos de teclado para operações 
void keyboard(unsigned char key, int x, int y) {
    double passo = 10.0;
    double angulo = 0.05;

    switch (key) {
        // Translação [cite: 4]
    case 'w': movimentar(meuCubo, 0, passo, 0); break;
    case 's': movimentar(meuCubo, 0, -passo, 0); break;
    case 'a': movimentar(meuCubo, -passo, 0, 0); break;
    case 'd': movimentar(meuCubo, passo, 0, 0); break;

        // Escala [cite: 4]
    case '+': escalar(meuCubo, 1.1); break;
    case '-': escalar(meuCubo, 0.9); break;

        // Rotação manual [cite: 4, 7]
    case 'r': rotacionarY(meuCubo, angulo); break;
    case 't': rotacionarX(meuCubo, angulo); break;

    case 27: exit(0); break;
    }
    glutPostRedisplay();
}

// Implementação manual sem glTranslate [cite: 7]
void movimentar(Cubo& c, double dx, double dy, double dz) {
    c.posicao.x += dx;
    c.posicao.y += dy;
    c.posicao.z += dz;
    for (auto& v : c.vertices) {
        v.x += dx; v.y += dy; v.z += dz;
    }
}

// Implementação manual sem glScale [cite: 7]
void escalar(Cubo& c, double fator) {
    for (auto& v : c.vertices) {
        v.x = (v.x - c.posicao.x) * fator + c.posicao.x;
        v.y = (v.y - c.posicao.y) * fator + c.posicao.y;
        v.z = (v.z - c.posicao.z) * fator + c.posicao.z;
    }
}

// Implementação manual sem glRotate [cite: 7]
void rotacionarY(Cubo& c, double angulo) {
    for (auto& v : c.vertices) {
        double x_rel = v.x - c.posicao.x;
        double z_rel = v.z - c.posicao.z;
        v.x = x_rel * cos(angulo) - z_rel * sin(angulo) + c.posicao.x;
        v.z = x_rel * sin(angulo) + z_rel * cos(angulo) + c.posicao.z;
    }
}

void rotacionarX(Cubo& c, double angulo) {
    for (auto& v : c.vertices) {
        double y_rel = v.y - c.posicao.y;
        double z_rel = v.z - c.posicao.z;
        v.y = y_rel * cos(angulo) - z_rel * sin(angulo) + c.posicao.y;
        v.z = y_rel * sin(angulo) + z_rel * cos(angulo) + c.posicao.z;
    }
}

// Função de desenho respeitando 3D e GL_LINES [cite: 3, 5]
void desenhar(Cubo c) {
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_LINES);
    for (const auto& a : c.arestas) {
        glVertex3f(c.vertices[a.first].x, c.vertices[a.first].y, c.vertices[a.first].z);
        glVertex3f(c.vertices[a.second].x, c.vertices[a.second].y, c.vertices[a.second].z);
    }
    glEnd();
}