#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <locale>
#define NOMINMAX
#include <windows.h>
#include <GL/freeglut.h>

using namespace std;

struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Vec2 {
    float u, v;
    Vec2(float u = 0, float v = 0) : u(u), v(v) {}
};

struct FaceVertex {
    int v, vt, vn; // indices 0-based (-1 = ausente)
    FaceVertex(int v = -1, int vt = -1, int vn = -1) : v(v), vt(vt), vn(vn) {}
};

struct Face {
    FaceVertex a, b, c;
};

struct Model {
    vector<Vec3>  vertices;
    vector<Vec3>  normais;
    vector<Vec2>  texcoords;
    vector<Face>  faces;

    // Estado de transformacao
    float transX = 0, transY = 0, transZ = 0;
    float escala = 1.0f;
    float rotX = 20.0f, rotY = 30.0f;
};

Model   modelo;
bool    modeloCarregado = false;
bool    luz[3] = { true, true, true };

vector<string> listaModelos;
int modeloAtualIndex = -1;

// Mouse
int  mouseUltimoX = 0, mouseUltimoY = 0;
bool mouseBotaoPressionado = false;

// Parser OBJ
static FaceVertex parseFV(const string& tok) {
    FaceVertex fv(0, 0, 0); // 0 indica ausente temporariamente
    size_t p1 = tok.find('/');
    if (p1 == string::npos) {
        fv.v = stoi(tok);
    }
    else {
        fv.v = stoi(tok.substr(0, p1));
        size_t p2 = tok.find('/', p1 + 1);
        if (p2 == string::npos) {
            string vt = tok.substr(p1 + 1);
            if (!vt.empty()) fv.vt = stoi(vt);
        }
        else {
            string vt = tok.substr(p1 + 1, p2 - p1 - 1);
            string vn = tok.substr(p2 + 1);
            if (!vt.empty()) fv.vt = stoi(vt);
            if (!vn.empty()) fv.vn = stoi(vn);
        }
    }
    return fv;
}

bool carregarOBJ(const string& path, Model& m) {
    ifstream file(path);
    if (!file.is_open()) {
        cerr << "Erro ao abrir: " << path << endl;
        return false;
    }
    m.vertices.clear(); m.normais.clear();
    m.texcoords.clear(); m.faces.clear();

    string linha;
    while (getline(file, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        istringstream ss(linha);
        ss.imbue(locale("C"));
        string tok; ss >> tok;

        if (tok == "v") {
            Vec3 v; ss >> v.x >> v.y >> v.z; m.vertices.push_back(v);
        }
        else if (tok == "vn") {
            Vec3 n; ss >> n.x >> n.y >> n.z; m.normais.push_back(n);
        }
        else if (tok == "vt") {
            Vec2 t; ss >> t.u >> t.v; m.texcoords.push_back(t);
        }
        else if (tok == "f") {
            vector<FaceVertex> poly;
            while (ss >> tok) {
                FaceVertex fv = parseFV(tok);
                // Resolve índices relativos e converte para 0-based
                if (fv.v != 0)  fv.v = (fv.v < 0) ? (int)m.vertices.size() + fv.v : fv.v - 1;
                else fv.v = -1;

                if (fv.vt != 0) fv.vt = (fv.vt < 0) ? (int)m.texcoords.size() + fv.vt : fv.vt - 1;
                else fv.vt = -1;

                if (fv.vn != 0) fv.vn = (fv.vn < 0) ? (int)m.normais.size() + fv.vn : fv.vn - 1;
                else fv.vn = -1;

                poly.push_back(fv);
            }

            for (size_t i = 1; i + 1 < poly.size(); i++) {
                Face f; f.a = poly[0]; f.b = poly[i]; f.c = poly[i + 1];
                m.faces.push_back(f);
            }
        }
    }
    cout << "OBJ carregado: " << m.vertices.size() << " vertices, "
        << m.normais.size() << " normais, "
        << m.texcoords.size() << " texcoords, "
        << m.faces.size() << " faces." << endl;
    return true;
}

// Calcula normais por vertice se o OBJ nao tiver
void calcularNormais(Model& m) {
    if (!m.normais.empty()) return; // ja tem normais

    m.normais.assign(m.vertices.size(), Vec3(0, 0, 0));

    for (auto& face : m.faces) {
        FaceVertex* fvs[3] = { &face.a, &face.b, &face.c };
        Vec3& v0 = m.vertices[fvs[0]->v];
        Vec3& v1 = m.vertices[fvs[1]->v];
        Vec3& v2 = m.vertices[fvs[2]->v];

        Vec3 e1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        Vec3 e2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
        Vec3 n = { e1.y * e2.z - e1.z * e2.y,
                    e1.z * e2.x - e1.x * e2.z,
                    e1.x * e2.y - e1.y * e2.x };

        for (int i = 0; i < 3; i++) {
            int vi = fvs[i]->v;
            m.normais[vi].x += n.x;
            m.normais[vi].y += n.y;
            m.normais[vi].z += n.z;
            fvs[i]->vn = vi; // aponta para normal do vertice
        }
    }
    for (auto& n : m.normais) {
        float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-6f) { n.x /= len; n.y /= len; n.z /= len; }
    }
}

// Centraliza e normaliza escala do modelo
void normalizarModelo(Model& m) {
    if (m.vertices.empty()) return;

    vector<float> xs, ys, zs;
    xs.reserve(m.vertices.size()); ys.reserve(m.vertices.size()); zs.reserve(m.vertices.size());
    for (const auto& v : m.vertices) {
        xs.push_back(v.x); ys.push_back(v.y); zs.push_back(v.z);
    }
    sort(xs.begin(), xs.end());
    sort(ys.begin(), ys.end());
    sort(zs.begin(), zs.end());

    size_t i0 = xs.size() * 0.02;
    size_t i1 = xs.size() * 0.98;
    if (i1 <= i0 || i1 >= xs.size()) { i0 = 0; i1 = xs.size() - 1; }

    Vec3 mn = { xs[i0], ys[i0], zs[i0] };
    Vec3 mx = { xs[i1], ys[i1], zs[i1] };

    Vec3 centro = { (mn.x + mx.x) / 2.0f, (mn.y + mx.y) / 2.0f, (mn.z + mx.z) / 2.0f };
    float tamanho = max({ mx.x - mn.x, mx.y - mn.y, mx.z - mn.z });
    float fator = (tamanho > 1e-6f) ? 150.0f / tamanho : 1.0f;

    for (auto& v : m.vertices) {
        v.x = (v.x - centro.x) * fator;
        v.y = (v.y - centro.y) * fator;
        v.z = (v.z - centro.z) * fator;
    }
}

void carregarModeloAtual() {
    if (listaModelos.empty() || modeloAtualIndex < 0 || modeloAtualIndex >= (int)listaModelos.size()) return;

    modelo = Model();

    cout << "Carregando " << listaModelos[modeloAtualIndex] << "..." << endl;
    if (carregarOBJ(listaModelos[modeloAtualIndex], modelo)) {
        calcularNormais(modelo);
        normalizarModelo(modelo);
        modeloCarregado = true;
    }
    else {
        modeloCarregado = false;
    }
}

// Iluminacao
void configurarLuzes() {
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // --- Luz 0: Branca, frente cima esquerda ---
    GLfloat l0pos[] = { -150.f, 200.f, 300.f, 1.f };
    GLfloat l0amb[] = { 0.1f,  0.1f,  0.1f, 1.f };
    GLfloat l0dif[] = { 0.9f,  0.9f,  0.9f, 1.f };
    GLfloat l0spe[] = { 1.0f,  1.0f,  1.0f, 1.f };
    glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, l0amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l0dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, l0spe);

    // --- Luz 1: Avermelhada, direita ---
    GLfloat l1pos[] = { 300.f,  50.f, 100.f, 1.f };
    GLfloat l1amb[] = { 0.05f,  0.0f,  0.0f, 1.f };
    GLfloat l1dif[] = { 0.8f,   0.2f,  0.2f, 1.f };
    GLfloat l1spe[] = { 1.0f,   0.4f,  0.4f, 1.f };
    glLightfv(GL_LIGHT1, GL_POSITION, l1pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT, l1amb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, l1dif);
    glLightfv(GL_LIGHT1, GL_SPECULAR, l1spe);

    // --- Luz 2: Azulada, baixo ---
    GLfloat l2pos[] = { 0.f, -250.f, 150.f, 1.f };
    GLfloat l2amb[] = { 0.0f,  0.0f, 0.05f, 1.f };
    GLfloat l2dif[] = { 0.2f,  0.3f,  0.9f, 1.f };
    GLfloat l2spe[] = { 0.5f,  0.5f,  1.0f, 1.f };
    glLightfv(GL_LIGHT2, GL_POSITION, l2pos);
    glLightfv(GL_LIGHT2, GL_AMBIENT, l2amb);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, l2dif);
    glLightfv(GL_LIGHT2, GL_SPECULAR, l2spe);

    // material
    GLfloat matSpe[] = { 0.8f, 0.8f, 0.8f, 1.f };
    GLfloat matBrilho[] = { 64.f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpe);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matBrilho);
}

void atualizarLuzes() {
    luz[0] ? glEnable(GL_LIGHT0) : glDisable(GL_LIGHT0);
    luz[1] ? glEnable(GL_LIGHT1) : glDisable(GL_LIGHT1);
    luz[2] ? glEnable(GL_LIGHT2) : glDisable(GL_LIGHT2);
}

// Render
void desenharModelo() {
    glColor3f(0.75f, 0.75f, 0.75f);
    glBegin(GL_TRIANGLES);
    for (const auto& face : modelo.faces) {
        const FaceVertex* fvs[3] = { &face.a, &face.b, &face.c };
        for (int i = 0; i < 3; i++) {
            // Normal
            if (fvs[i]->vn >= 0 && fvs[i]->vn < (int)modelo.normais.size()) {
                const Vec3& n = modelo.normais[fvs[i]->vn];
                glNormal3f(n.x, n.y, n.z);
            }
            // Textura
            if (fvs[i]->vt >= 0 && fvs[i]->vt < (int)modelo.texcoords.size()) {
                const Vec2& t = modelo.texcoords[fvs[i]->vt];
                glTexCoord2f(t.u, t.v);
            }
            // Vertice
            if (fvs[i]->v >= 0 && fvs[i]->v < (int)modelo.vertices.size()) {
                const Vec3& v = modelo.vertices[fvs[i]->v];
                glVertex3f(v.x, v.y, v.z);
            }
        }
    }
    glEnd();
}

// Texto HUD na tela (2D)
void renderizarTexto(int x, int y, const string& texto, void* fonte = GLUT_BITMAP_8_BY_13) {
    glRasterPos2i(x, y);
    for (char c : texto) glutBitmapCharacter(fonte, c);
}

void desenharHUD(int w, int h) {
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); glLoadIdentity();

    glColor3f(1.f, 1.f, 1.f);
    renderizarTexto(10, h - 20, "=== Controles ===");
    renderizarTexto(10, h - 36, "Mouse: arrastar -> rotacao");
    renderizarTexto(10, h - 52, "W/S/A/D: transladar  Q/E/Scroll do Mouse: profundidade");
    renderizarTexto(10, h - 68, "R/F: rot Y    T/G: rot X");
    renderizarTexto(10, h - 84, "+/-: escala   ESC: sair");
    renderizarTexto(10, h - 100, "1/2/3: toggle luzes");
    renderizarTexto(10, h - 116, "Espaco/N: proximo OBJ");

    // luzes
    string l1 = string("Luz 1 (branca):   ") + (luz[0] ? "ON " : "OFF");
    string l2 = string("Luz 2 (vermelha): ") + (luz[1] ? "ON " : "OFF");
    string l3 = string("Luz 3 (azul):     ") + (luz[2] ? "ON " : "OFF");
    glColor3f(luz[0] ? 1.f : 0.4f, luz[0] ? 1.f : 0.4f, luz[0] ? 0.6f : 0.4f);
    renderizarTexto(10, 60, l1);
    glColor3f(luz[1] ? 1.f : 0.4f, luz[1] ? 0.4f : 0.4f, luz[1] ? 0.4f : 0.4f);
    renderizarTexto(10, 44, l2);
    glColor3f(luz[2] ? 0.4f : 0.4f, luz[2] ? 0.4f : 0.4f, luz[2] ? 1.f : 0.4f);
    renderizarTexto(10, 28, l3);

    if (!modeloCarregado) {
        glColor3f(1.f, 0.8f, 0.2f);
        renderizarTexto(10, 10, "Nenhum OBJ carregado.");
    }
    else if (modeloAtualIndex >= 0 && modeloAtualIndex < (int)listaModelos.size()) {
        string nomeArquivo = listaModelos[modeloAtualIndex];
        size_t pos = nomeArquivo.find_last_of("/\\");
        if (pos != string::npos) nomeArquivo = nomeArquivo.substr(pos + 1);
        glColor3f(1.f, 1.f, 1.f);
        renderizarTexto(10, 10, "Modelo: " + nomeArquivo + " (" + to_string(modeloAtualIndex + 1) + "/" + to_string(listaModelos.size()) + ")");
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

// Janela 
int janW = 800, janH = 600;

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera
    gluLookAt(0, 0, 400, 0, 0, 0, 0, 1, 0);

    // Posiciona luzes 
    GLfloat l0pos[] = { -150.f, 200.f, 300.f, 1.f };
    GLfloat l1pos[] = { 300.f,  50.f, 100.f, 1.f };
    GLfloat l2pos[] = { 0.f,-250.f, 150.f, 1.f };
    glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
    glLightfv(GL_LIGHT1, GL_POSITION, l1pos);
    glLightfv(GL_LIGHT2, GL_POSITION, l2pos);
    atualizarLuzes();

    // Transformacoes do modelo
    glTranslatef(modelo.transX, modelo.transY, modelo.transZ);
    glRotatef(modelo.rotX, 1.f, 0.f, 0.f);
    glRotatef(modelo.rotY, 0.f, 1.f, 0.f);
    glScalef(modelo.escala, modelo.escala, modelo.escala);

    if (modeloCarregado) {
        desenharModelo();
    }

    desenharHUD(janW, janH);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    janW = w; janH = (h == 0 ? 1 : h);
    glViewport(0, 0, janW, janH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)janW / janH, 1.0, 5000.0);
    glMatrixMode(GL_MODELVIEW);
}

// Controles
void keyboard(unsigned char key, int x, int y) {
    const float passo = 10.f;
    const float angulo = 5.f;

    switch (key) {
        // Translacao
    case 'w': case 'W': modelo.transY += passo; break;
    case 's': case 'S': modelo.transY -= passo; break;
    case 'a': case 'A': modelo.transX -= passo; break;
    case 'd': case 'D': modelo.transX += passo; break;
    case 'q': case 'Q': modelo.transZ += passo; break;
    case 'e': case 'E': modelo.transZ -= passo; break;

        // Escala
    case '+': case '=': modelo.escala *= 1.1f; break;
    case '-': case '_': modelo.escala *= 0.9f; break;

        // Rotacao com teclado
    case 'r': case 'R': modelo.rotY += angulo; break;
    case 'f': case 'F': modelo.rotY -= angulo; break;
    case 't': case 'T': modelo.rotX += angulo; break;
    case 'g': case 'G': modelo.rotX -= angulo; break;

        // luzes
    case '1': luz[0] = !luz[0]; break;
    case '2': luz[1] = !luz[1]; break;
    case '3': luz[2] = !luz[2]; break;

        // Proximo modelo
    case ' ': case 'n': case 'N':
        if (!listaModelos.empty()) {
            modeloAtualIndex = (modeloAtualIndex + 1) % (int)listaModelos.size();
            carregarModeloAtual();
        }
        break;

    case 27: exit(0); // ESC
    }
    glutPostRedisplay();
}

void teclaEspecial(int key, int x, int y) {
    const float passo = 10.f;
    switch (key) {
    case GLUT_KEY_UP:    modelo.transY += passo; break;
    case GLUT_KEY_DOWN:  modelo.transY -= passo; break;
    case GLUT_KEY_LEFT:  modelo.transX -= passo; break;
    case GLUT_KEY_RIGHT: modelo.transX += passo; break;
    }
    glutPostRedisplay();
}

void mouse(int botao, int estado, int x, int y) {
    if (botao == GLUT_LEFT_BUTTON) {
        mouseBotaoPressionado = (estado == GLUT_DOWN);
        mouseUltimoX = x;
        mouseUltimoY = y;
    }
    // 3 e 4 = scroll
    if (botao == 3) { modelo.escala *= 1.05f; glutPostRedisplay(); }
    if (botao == 4) { modelo.escala *= 0.95f; glutPostRedisplay(); }
}

void mouseMover(int x, int y) {
    if (mouseBotaoPressionado) {
        modelo.rotY += (x - mouseUltimoX) * 0.5f;
        modelo.rotX += (y - mouseUltimoY) * 0.5f;
        mouseUltimoX = x;
        mouseUltimoY = y;
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("CG M1.2 - Modelos 3D com Iluminacao");

    glClearColor(0.12f, 0.12f, 0.15f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    configurarLuzes();

    string pathObj = "obj";
    DWORD attr = GetFileAttributesA(pathObj.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        pathObj = "ComputacaoGrafica/obj";
        attr = GetFileAttributesA(pathObj.c_str());
    }

    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        string searchPath = pathObj + "/*.obj";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    listaModelos.push_back(pathObj + "/" + fd.cFileName);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    if (argc > 1) {
        string arg = argv[1];
        auto it = find(listaModelos.begin(), listaModelos.end(), arg);
        if (it == listaModelos.end()) {
            listaModelos.insert(listaModelos.begin(), arg);
            modeloAtualIndex = 0;
        }
        else {
            modeloAtualIndex = (int)distance(listaModelos.begin(), it);
        }
        carregarModeloAtual();
    }
    else if (!listaModelos.empty()) {
        modeloAtualIndex = 0;
        carregarModeloAtual();
    }
    else {
        cout << "Uso: " << argv[0] << " <arquivo.obj>" << endl;
        cout << "Mostrando teapot de demonstracao." << endl;
    }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(teclaEspecial);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMover);

    glutMainLoop();
    return 0;
}