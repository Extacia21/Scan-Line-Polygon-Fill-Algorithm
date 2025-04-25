#include <windows.h>
#include <gl/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SCREEN_WIDTH 1400
#define SCREEN_HEIGHT 900
#define SCALE_FACTOR 2.5f

typedef struct {
    int yMin;
    int yMax;
    float x;
    float slope;
} Edge;

// Global variables
POINT* polygonVertices = NULL;
int polygonVerticesCount = 0;
int polygonVerticesCapacity = 0;

POINT* filledPixels = NULL;
int filledPixelsCount = 0;
int filledPixelsCapacity = 0;

BOOL polygonComplete = FALSE;
BOOL showScanLines = FALSE;
BOOL showFilledPolygon = FALSE;
BOOL showEdgeTable = FALSE;
BOOL rainbowMode = FALSE;
int currentScanLine = -10;
float hueShift = 0.0f;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND, HDC, HGLRC);
void DrawPolygon(HDC hDC);
void FillPolygon();
void ScanLineFill();
void DrawEdgeTable(HDC hDC, Edge* edgeTable, int edgeCount);
POINT ScaleAndCenterPoint(POINT pt);
void AddVertex(POINT pt);
void AddFilledPixel(POINT pt);
int CompareEdges(const void* a, const void* b);
void DrawCustomText(HDC hDC, int x, int y, const char* text, COLORREF color);
void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = "ScanLineFill";

    if (!RegisterClassEx(&wcex)) return 0;

    HWND hwnd = CreateWindowEx(0, "ScanLineFill", "Colorful Scan Line Polygon Fill Algorithm",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    HDC hDC;
    HGLRC hRC;
    EnableOpenGL(hwnd, &hDC, &hRC);
    ShowWindow(hwnd, nCmdShow);

    srand((unsigned int)time(NULL));

    MSG msg;
    BOOL bQuit = FALSE;
    while (!bQuit)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) bQuit = TRUE;
            else { TranslateMessage(&msg); DispatchMessage(&msg); }
        }
        else
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            if (rainbowMode) {
                hueShift += 0.005f;
                if (hueShift > 1.0f) hueShift -= 1.0f;
            }

            DrawPolygon(hDC);
            if (showFilledPolygon) FillPolygon();

            SwapBuffers(hDC);
        }
    }

    DisableOpenGL(hwnd, hDC, hRC);
    if (polygonVertices) free(polygonVertices);
    if (filledPixels) free(filledPixels);
    DestroyWindow(hwnd);

    return msg.wParam;
}

/* SCAN-LINE ALGORITHM IMPLEMENTATION */
void ScanLineFill()
{
    filledPixelsCount = 0;
    if (polygonVerticesCount < 3) return;

    // Step 1: Find polygon's min and max y-coordinates
    int yMin = polygonVertices[0].y;
    int yMax = polygonVertices[0].y;
    for (int i = 0; i < polygonVerticesCount; i++)
    {
        if (polygonVertices[i].y < yMin) yMin = polygonVertices[i].y;
        if (polygonVertices[i].y > yMax) yMax = polygonVertices[i].y;
    }

    // Step 2: Build Edge Table (ET)
    Edge* edgeTable = malloc(polygonVerticesCount * sizeof(Edge));
    int edgeCount = 0;

    for (int i = 0; i < polygonVerticesCount; i++)
    {
        int j = (i + 1) % polygonVerticesCount;
        POINT p1 = polygonVertices[i];
        POINT p2 = polygonVertices[j];

        // Skip horizontal edges
        if (p1.y == p2.y) continue;

        Edge edge;
        edge.yMin = min(p1.y, p2.y);
        edge.yMax = max(p1.y, p2.y);

        // Calculate inverse slope (1/m)
        if (p1.y < p2.y) {
            edge.x = p1.x;
            edge.slope = (float)(p2.x - p1.x) / (p2.y - p1.y);
        } else {
            edge.x = p2.x;
            edge.slope = (float)(p1.x - p2.x) / (p1.y - p2.y);
        }

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
    Edge* activeEdges = malloc(edgeCount * sizeof(Edge));
    int activeCount = 0;

    // Step 5: Process each scan line
    for (int y = yMin; y <= yMax; y++)
    {
        // Step 5a: Move edges from ET to AET where yMin == current y
        while (edgeCount > 0 && edgeTable[0].yMin == y)
        {
            activeEdges[activeCount++] = edgeTable[0];
            for (int i = 0; i < edgeCount - 1; i++) {
                edgeTable[i] = edgeTable[i + 1];
            }
            edgeCount--;
        }

        // Step 5b: Remove edges from AET where y >= yMax
        for (int i = 0; i < activeCount; i++) {
            if (y >= activeEdges[i].yMax) {
                for (int j = i; j < activeCount - 1; j++) {
                    activeEdges[j] = activeEdges[j + 1];
                }
                activeCount--;
                i--;
            }
        }

        // Step 5c: Sort AET by x and slope
        qsort(activeEdges, activeCount, sizeof(Edge), CompareEdges);

        // Step 5d: Fill between pairs of edges
        for (int i = 0; i < activeCount; i += 2)
        {
            if (i + 1 >= activeCount) break;

            int xStart = (int)activeEdges[i].x;
            int xEnd = (int)activeEdges[i + 1].x;

            if (xStart > xEnd) {
                int temp = xStart;
                xStart = xEnd;
                xEnd = temp;
            }

            for (int x = xStart; x <= xEnd; x++)
            {
                AddFilledPixel((POINT){x, y});
            }
        }

        // Step 5e: Update x for next scan line (x = x + 1/m)
        for (int i = 0; i < activeCount; i++)
        {
            activeEdges[i].x += activeEdges[i].slope;
        }
    }

    free(edgeTable);
    free(activeEdges);
}

/* HELPER FUNCTIONS */
void AddVertex(POINT pt) {
    if (polygonVerticesCount >= polygonVerticesCapacity) {
        polygonVerticesCapacity = polygonVerticesCapacity ? polygonVerticesCapacity * 2 : 16;
        polygonVertices = realloc(polygonVertices, polygonVerticesCapacity * sizeof(POINT));
    }
    polygonVertices[polygonVerticesCount++] = pt;
}

void AddFilledPixel(POINT pt) {
    if (filledPixelsCount >= filledPixelsCapacity) {
        filledPixelsCapacity = filledPixelsCapacity ? filledPixelsCapacity * 2 : 1024;
        filledPixels = realloc(filledPixels, filledPixelsCapacity * sizeof(POINT));
    }
    filledPixels[filledPixelsCount++] = pt;
}

POINT ScaleAndCenterPoint(POINT pt) {
    if (polygonVerticesCount == 0) {
        return (POINT){SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2};
    }

    float cx = 0, cy = 0;
    for (int i = 0; i < polygonVerticesCount; i++) {
        cx += polygonVertices[i].x;
        cy += polygonVertices[i].y;
    }
    cx /= polygonVerticesCount;
    cy /= polygonVerticesCount;

    POINT scaled;
    scaled.x = (int)((pt.x - cx) * SCALE_FACTOR + SCREEN_WIDTH / 2);
    scaled.y = (int)((pt.y - cy) * SCALE_FACTOR + SCREEN_HEIGHT / 2);
    return scaled;
}

int CompareEdges(const void* a, const void* b) {
    const Edge* edgeA = (const Edge*)a;
    const Edge* edgeB = (const Edge*)b;
    if (edgeA->x != edgeB->x) return (edgeA->x < edgeB->x) ? -1 : 1;
    return (edgeA->slope < edgeB->slope) ? -1 : 1;
}

void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b) {
    int i;
    float f, p, q, t;

    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    h *= 6.0f; // sector 0 to 5
    i = (int)floor(h);
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch (i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/* VISUALIZATION FUNCTIONS */
void DrawPolygon(HDC hDC)
{
    // Draw vertices
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < polygonVerticesCount; i++)
    {
        float hue = (float)i / polygonVerticesCount + hueShift;
        if (hue > 1.0f) hue -= 1.0f;

        float r, g, b;
        HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
        glColor3f(r, g, b);

        POINT scaled = ScaleAndCenterPoint(polygonVertices[i]);
        glVertex2i(scaled.x, scaled.y);
    }
    glEnd();

    // Draw edges
    if (polygonVerticesCount > 1)
    {
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < polygonVerticesCount; i++)
        {
            float hue = (float)i / polygonVerticesCount + hueShift;
            if (hue > 1.0f) hue -= 1.0f;

            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);

            POINT scaled = ScaleAndCenterPoint(polygonVertices[i]);
            glVertex2i(scaled.x, scaled.y);
        }
        if (polygonComplete)
        {
            float hue = hueShift;
            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);

            POINT scaled = ScaleAndCenterPoint(polygonVertices[0]);
            glVertex2i(scaled.x, scaled.y);
        }
        glEnd();
    }

    // Draw scan lines
    if (showScanLines && polygonComplete)
    {
        int yMin = SCREEN_HEIGHT, yMax = 0;
        for (int i = 0; i < polygonVerticesCount; i++)
        {
            POINT scaled = ScaleAndCenterPoint(polygonVertices[i]);
            if (scaled.y < yMin) yMin = scaled.y;
            if (scaled.y > yMax) yMax = scaled.y;
        }

        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
        for (int y = yMin; y <= yMax; y++)
        {
            glVertex2i(0, y);
            glVertex2i(SCREEN_WIDTH, y);
        }
        glEnd();

        if (currentScanLine >= 0)
        {
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex2i(0, currentScanLine);
            glVertex2i(SCREEN_WIDTH, currentScanLine);
            glEnd();
        }
    }

    // Draw edge table
    if (showEdgeTable && polygonComplete)
    {
        Edge* edgeTable = malloc(polygonVerticesCount * sizeof(Edge));
        int edgeCount = 0;

        for (int i = 0; i < polygonVerticesCount; i++)
        {
            int j = (i + 1) % polygonVerticesCount;
            POINT p1 = ScaleAndCenterPoint(polygonVertices[i]);
            POINT p2 = ScaleAndCenterPoint(polygonVertices[j]);

            if (p1.y == p2.y) continue;

            Edge edge;
            edge.yMin = min(p1.y, p2.y);
            edge.yMax = max(p1.y, p2.y);
            edge.x = p1.y < p2.y ? p1.x : p2.x;
            edge.slope = (p1.y < p2.y) ?
                (float)(p2.x - p1.x) / (p2.y - p1.y) :
                (float)(p1.x - p2.x) / (p1.y - p2.y);

            edgeTable[edgeCount++] = edge;
        }
        DrawEdgeTable(hDC, edgeTable, edgeCount);
        free(edgeTable);
    }

    // Draw instructions
    DrawCustomText(hDC, 10, 10, "Left-click: Add vertex | Right-click: Complete polygon", RGB(255, 255, 255));
    DrawCustomText(hDC, 10, 30, "F: Toggle fill | S: Toggle scan lines | E: Toggle edge table", RGB(255, 255, 255));
    DrawCustomText(hDC, 10, 50, "Up/Down: Move scan line | C: Clear | R: Rainbow mode | Esc: Quit", RGB(255, 255, 255));
    DrawCustomText(hDC, 10, 70, rainbowMode ? "Rainbow mode: ON" : "Rainbow mode: OFF", rainbowMode ? RGB(0, 255, 0) : RGB(255, 0, 0));
}

void FillPolygon()
{
    glPointSize(1.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < filledPixelsCount; i++)
    {
        if (rainbowMode) {
            float hue = (float)(filledPixels[i].y % 360) / 360.0f + hueShift;
            if (hue > 1.0f) hue -= 1.0f;

            float r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            glColor3f(r, g, b);
        } else {
            float hue = (float)(filledPixels[i].y % 100) / 100.0f;
            float r, g, b;
            HSVtoRGB(0.7f, 0.8f, 0.9f, &r, &g, &b);
            glColor3f(r, g, b);
        }

        POINT scaled = ScaleAndCenterPoint(filledPixels[i]);
        glVertex2i(scaled.x, scaled.y);
    }
    glEnd();
}

void DrawEdgeTable(HDC hDC, Edge* edgeTable, int edgeCount)
{
    int tableX = 650;
    int tableY = 50;
    int rowHeight = 20;

    DrawCustomText(hDC, tableX, tableY - 20, "Edge Table (yMin, yMax, x, slope)", RGB(255, 255, 0));

    for (int i = 0; i < edgeCount; i++)
    {
        const Edge* e = &edgeTable[i];
        int yPos = tableY + i * rowHeight;
        COLORREF color = (currentScanLine >= e->yMin && currentScanLine < e->yMax) ?
            RGB(0, 255, 0) : RGB(255, 255, 255);

        char buffer[100];
        sprintf(buffer, "%4d %4d %6.1f %6.2f", e->yMin, e->yMax, e->x, e->slope);
        DrawCustomText(hDC, tableX, yPos, buffer, color);
    }
}

void DrawCustomText(HDC hDC, int x, int y, const char* text, COLORREF color)
{
    SetBkColor(hDC, RGB(0, 0, 0));
    SetTextColor(hDC, color);
    TextOut(hDC, x, y, text, strlen(text));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_LBUTTONDOWN:
            if (!polygonComplete) {
                AddVertex((POINT){LOWORD(lParam), HIWORD(lParam)});
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (!polygonComplete && polygonVerticesCount > 2) {
                polygonComplete = TRUE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;

        case WM_KEYDOWN:
            switch (wParam)
            {
                case VK_ESCAPE: PostQuitMessage(0); break;
                case 'F': case 'f':
                    if (polygonComplete) {
                        showFilledPolygon = !showFilledPolygon;
                        if (showFilledPolygon) ScanLineFill();
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case 'S': case 's':
                    showScanLines = !showScanLines;
                    if (!showScanLines) currentScanLine = -1;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 'E': case 'e':
                    if (polygonComplete) {
                        showEdgeTable = !showEdgeTable;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case 'R': case 'r':
                    rainbowMode = !rainbowMode;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case VK_UP: case VK_DOWN:
                    if (showScanLines && polygonComplete) {
                        if (currentScanLine < 0) {
                            currentScanLine = ScaleAndCenterPoint(polygonVertices[0]).y;
                            for (int i = 0; i < polygonVerticesCount; i++) {
                                int y = ScaleAndCenterPoint(polygonVertices[i]).y;
                                if (wParam == VK_UP && y > currentScanLine) currentScanLine = y;
                                if (wParam == VK_DOWN && y < currentScanLine) currentScanLine = y;
                            }
                        } else {
                            currentScanLine += (wParam == VK_UP) ? -1 : 1;
                        }
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case 'C': case 'c':
                    polygonVerticesCount = 0;
                    filledPixelsCount = 0;
                    polygonComplete = FALSE;
                    showFilledPolygon = FALSE;
                    showScanLines = FALSE;
                    showEdgeTable = FALSE;
                    currentScanLine = -1;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
            }
            return 0;

        case WM_CLOSE: PostQuitMessage(0); return 0;
        case WM_DESTROY: return 0;
        default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR) };
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    *hDC = GetDC(hwnd);
    int iFormat = ChoosePixelFormat(*hDC, &pfd);
    SetPixelFormat(*hDC, iFormat, &pfd);
    *hRC = wglCreateContext(*hDC);
    wglMakeCurrent(*hDC, *hRC);
}

void DisableOpenGL(HWND hwnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hwnd, hDC);
}
