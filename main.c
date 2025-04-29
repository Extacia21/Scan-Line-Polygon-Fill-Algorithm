#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SCREEN_WIDTH 1500
#define SCREEN_HEIGHT 900
#define SCALE_FACTOR 2.5f
#define MAX_VERTICES 1000
#define MAX_PIXELS 100000

typedef struct {
    int yMin;
    int yMax;
    float x;
    float slope;
} Edge;

typedef struct {
    int x, y;
} Point;

// Global variables
Point polygonVertices[MAX_VERTICES];
int polygonVerticesCount = 0;

Point filledPixels[MAX_PIXELS];
int filledPixelsCount = 0;

int polygonComplete = 0;
int showScanLines = 0;
int showFilledPolygon = 0;
int showEdgeTable = 0;
int rainbowMode = 0;
int currentScanLine = -10;
float hueShift = 0.0f;

// Function prototypes
void DrawPolygon();
void FillPolygon();
void ScanLineFill();
void DrawEdgeTable(Edge* edgeTable, int edgeCount);
Point ScaleAndCenterPoint(Point pt);
void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b);
void display();
void keyboard(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void specialKeys(int key, int x, int y);
void update(int value);

void ScanLineFill() {
    filledPixelsCount = 0;
    if (polygonVerticesCount < 3) return;

    // Step 1: Find polygon's min and max y-coordinates
    int yMin = polygonVertices[0].y;
    int yMax = polygonVertices[0].y;
    for (int i = 0; i < polygonVerticesCount; i++) {
        if (polygonVertices[i].y < yMin) yMin = polygonVertices[i].y;
        if (polygonVertices[i].y > yMax) yMax = polygonVertices[i].y;
    }

    // Step 2: Build Edge Table (ET)
    Edge edgeTable[MAX_VERTICES];
    int edgeCount = 0;

    for (int i = 0; i < polygonVerticesCount; i++) {
        int j = (i + 1) % polygonVerticesCount;
        Point p1 = polygonVertices[i];
        Point p2 = polygonVertices[j];

        // Skip horizontal edges
        if (p1.y == p2.y) continue;

        Edge edge;
        edge.yMin = p1.y < p2.y ? p1.y : p2.y;
        edge.yMax = p1.y < p2.y ? p2.y : p1.y;
        edge.x = p1.y < p2.y ? p1.x : p2.x;
        edge.slope = (p1.y < p2.y) ?
            (float)(p2.x - p1.x) / (p2.y - p1.y) :
            (float)(p1.x - p2.x) / (p1.y - p2.y);

        edgeTable[edgeCount++] = edge;
    }

    // Step 3: Sort ET by yMin (scan line order)
    for (int i = 0; i < edgeCount - 1; i++) {
        for (int j = i + 1; j < edgeCount; j++) {
            if (edgeTable[i].yMin > edgeTable[j].yMin) {
                Edge temp = edgeTable[i];
                edgeTable[i] = edgeTable[j];
                edgeTable[j] = temp;
            }
        }
    }

    // Step 4: Initialize Active Edge Table (AET)
    Edge activeEdges[MAX_VERTICES];
    int activeCount = 0;

    // Step 5: Process each scan line
    for (int y = yMin; y <= yMax; y++) {
        // Step 5a: Move edges from ET to AET where yMin == current y
        while (edgeCount > 0 && edgeTable[0].yMin == y) {
            activeEdges[activeCount++] = edgeTable[0];
            for (int i = 0; i < edgeCount - 1; i++) {
                edgeTable[i] = edgeTable[i + 1];
            }
            edgeCount--;
        }

        // Step 5b: Remove edges from AET where y >= yMax
        for (int i = 0; i < activeCount; ) {
            if (y >= activeEdges[i].yMax) {
                for (int j = i; j < activeCount - 1; j++) {
                    activeEdges[j] = activeEdges[j + 1];
                }
                activeCount--;
            } else {
                i++;
            }
        }

        // Step 5c: Sort AET by x and slope
        for (int i = 0; i < activeCount - 1; i++) {
            for (int j = i + 1; j < activeCount; j++) {
                if (activeEdges[i].x > activeEdges[j].x ||
                   (activeEdges[i].x == activeEdges[j].x && activeEdges[i].slope > activeEdges[j].slope)) {
                    Edge temp = activeEdges[i];
                    activeEdges[i] = activeEdges[j];
                    activeEdges[j] = temp;
                }
            }
        }

        // Step 5d: Fill between pairs of edges
        for (int i = 0; i < activeCount; i += 2) {
            if (i + 1 >= activeCount) break;

            int xStart = (int)activeEdges[i].x;
            int xEnd = (int)activeEdges[i + 1].x;

            if (xStart > xEnd) {
                int temp = xStart;
                xStart = xEnd;
                xEnd = temp;
            }

            for (int x = xStart; x <= xEnd; x++) {
                if (filledPixelsCount < MAX_PIXELS) {
                    filledPixels[filledPixelsCount++] = (Point){x, y};
                }
            }
        }

        // Step 5e: Update x for next scan line (x = x + 1/m)
        for (int i = 0; i < activeCount; i++) {
            activeEdges[i].x += activeEdges[i].slope;
        }
    }
}

Point ScaleAndCenterPoint(Point pt) {
    if (polygonVerticesCount == 0) {
        return (Point){SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2};
    }

    float cx = 0, cy = 0;
    for (int i = 0; i < polygonVerticesCount; i++) {
        cx += polygonVertices[i].x;
        cy += polygonVertices[i].y;
    }
    cx /= polygonVerticesCount;
    cy /= polygonVerticesCount;

    Point scaled;
    scaled.x = (int)((pt.x - cx) * SCALE_FACTOR + SCREEN_WIDTH / 2);
    scaled.y = (int)((pt.y - cy) * SCALE_FACTOR + SCREEN_HEIGHT / 2);
    return scaled;
}

void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b) {
    int i;
    float f, p, q, t;

    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    h *= 6.0f;
    i = (int)floor(h);
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch (i % 6) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
    }
}

void DrawPolygon() {
    // Draw vertices
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < polygonVerticesCount; i++) {
        float hue = (float)i / polygonVerticesCount + hueShift;
        if (hue > 1.0f) hue -= 1.0f;

        float r, g, b;
        HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
        glColor3f(r, g, b);

        Point scaled = ScaleAndCenterPoint(polygonVertices[i]);
        glVertex2i(scaled.x, scaled.y);
    }
    glEnd();

    // Draw edges
    if (polygonVerticesCount > 1) {
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < polygonVerticesCount; i++) {
            float hue = (float)i / polygonVerticesCount + hueShift;
            if (hue > 1.0f) hue -= 1.0f;

            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);

            Point scaled = ScaleAndCenterPoint(polygonVertices[i]);
            glVertex2i(scaled.x, scaled.y);
        }
        if (polygonComplete) {
            float hue = hueShift;
            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);

            Point scaled = ScaleAndCenterPoint(polygonVertices[0]);
            glVertex2i(scaled.x, scaled.y);
        }
        glEnd();
    }

    // Draw scan lines
    if (showScanLines && polygonComplete) {
        int yMin = SCREEN_HEIGHT, yMax = 0;
        for (int i = 0; i < polygonVerticesCount; i++) {
            Point scaled = ScaleAndCenterPoint(polygonVertices[i]);
            if (scaled.y < yMin) yMin = scaled.y;
            if (scaled.y > yMax) yMax = scaled.y;
        }

        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
        for (int y = yMin; y <= yMax; y++) {
            glVertex2i(0, y);
            glVertex2i(SCREEN_WIDTH, y);
        }
        glEnd();

        if (currentScanLine >= 0) {
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex2i(0, currentScanLine);
            glVertex2i(SCREEN_WIDTH, currentScanLine);
            glEnd();
        }
    }
}

void FillPolygon() {
    glPointSize(1.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < filledPixelsCount; i++) {
        if (rainbowMode) {
            float hue = fmod((filledPixels[i].y % 360) / 360.0f + hueShift, 1.0f);
            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);
        } else {
            float r, g, b;
            HSVtoRGB(0.7f, 0.8f, 0.9f, &r, &g, &b);
            glColor3f(r, g, b);
        }

        Point scaled = ScaleAndCenterPoint(filledPixels[i]);
        glVertex2i(scaled.x, scaled.y);
    }
    glEnd();
}

void DrawEdgeTable(Edge* edgeTable, int edgeCount) {
    glColor3f(1, 1, 0);
    glRasterPos2i(650, 30);
    char* text = "Edge Table (yMin, yMax, x, slope)";
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);
    }

    for (int i = 0; i < edgeCount; i++) {
        Edge e = edgeTable[i];
        glRasterPos2i(650, 50 + i * 20);

        char buffer[100];
        sprintf(buffer, "%4d %4d %6.1f %6.2f", e.yMin, e.yMax, e.x, e.slope);
        text = buffer;

        if (currentScanLine >= e.yMin && currentScanLine < e.yMax) {
            glColor3f(0, 1, 0);
        } else {
            glColor3f(1, 1, 1);
        }

        while (*text) {
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);
        }
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    DrawPolygon();
    if (showFilledPolygon) FillPolygon();

    if (showEdgeTable && polygonComplete) {
        Edge edgeTable[MAX_VERTICES];
        int edgeCount = 0;
        for (int i = 0; i < polygonVerticesCount; i++) {
            int j = (i + 1) % polygonVerticesCount;
            Point p1 = ScaleAndCenterPoint(polygonVertices[i]);
            Point p2 = ScaleAndCenterPoint(polygonVertices[j]);

            if (p1.y == p2.y) continue;

            Edge edge;
            edge.yMin = p1.y < p2.y ? p1.y : p2.y;
            edge.yMax = p1.y < p2.y ? p2.y : p1.y;
            edge.x = p1.y < p2.y ? p1.x : p2.x;
            edge.slope = (p1.y < p2.y) ?
                (float)(p2.x - p1.x) / (p2.y - p1.y) :
                (float)(p1.x - p2.x) / (p1.y - p2.y);

            edgeTable[edgeCount++] = edge;
        }
        DrawEdgeTable(edgeTable, edgeCount);
    }

    // Draw instructions
    glColor3f(1, 1, 1);
    glRasterPos2i(10, 20);
    char* text = "Left-click: Add vertex | Right-click: Complete polygon";
    while (*text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);

    glRasterPos2i(10, 40);
    text = "F: Toggle fill | S: Toggle scan lines | E: Toggle edge table";
    while (*text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);

    glRasterPos2i(10, 60);
    text = "Up/Down: Move scan line | C: Clear | R: Rainbow mode | Esc: Quit";
    while (*text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);

    glRasterPos2i(10, 80);
    text = rainbowMode ? "Rainbow mode: ON" : "Rainbow mode: OFF";
    glColor3f(rainbowMode ? 0 : 1, rainbowMode ? 1 : 0, 0);
    while (*text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;
        case 'f': case 'F':
            if (polygonComplete) {
                showFilledPolygon = !showFilledPolygon;
                if (showFilledPolygon) ScanLineFill();
            }
            break;
        case 's': case 'S':
            showScanLines = !showScanLines;
            if (!showScanLines) currentScanLine = -1;
            break;
        case 'e': case 'E':
            if (polygonComplete) {
                showEdgeTable = !showEdgeTable;
            }
            break;
        case 'r': case 'R':
            rainbowMode = !rainbowMode;
            break;
        case 'c': case 'C':
            polygonVerticesCount = 0;
            filledPixelsCount = 0;
            polygonComplete = 0;
            showFilledPolygon = 0;
            showScanLines = 0;
            showEdgeTable = 0;
            currentScanLine = -1;
            break;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !polygonComplete) {
        if (polygonVerticesCount < MAX_VERTICES) {
            polygonVertices[polygonVerticesCount++] = (Point){x, y};
        }
    }
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN && polygonVerticesCount > 2 && !polygonComplete) {
        polygonComplete = 1;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    if (showScanLines && polygonComplete) {
        if (currentScanLine < 0) {
            currentScanLine = ScaleAndCenterPoint(polygonVertices[0]).y;
            for (int i = 0; i < polygonVerticesCount; i++) {
                int y = ScaleAndCenterPoint(polygonVertices[i]).y;
                if (key == GLUT_KEY_UP && y > currentScanLine) currentScanLine = y;
                if (key == GLUT_KEY_DOWN && y < currentScanLine) currentScanLine = y;
            }
        } else {
            currentScanLine += (key == GLUT_KEY_UP) ? -1 : 1;
        }
    }
    glutPostRedisplay();
}

void update(int value) {
    if (rainbowMode) {
        hueShift += 0.005f;
        if (hueShift > 1.0f) hueShift -= 1.0f;
        glutPostRedisplay();
    }
    glutTimerFunc(16, update, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    glutCreateWindow("Colorful Scan Line Polygon Fill Algorithm");

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    srand((unsigned int)time(NULL));

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(0, update, 0);

    glutMainLoop();
    return 0;
}
