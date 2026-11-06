// Учебный замок: C++17, GLFW 3, GLAD 1, OpenGL 2.1.
// Y направлена вверх. Передние ворота находятся со стороны +Z.
#include <glad/glad.h> // GLAD обязательно раньше GLFW
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// [01] НАСТРОЙКИ. Меняйте здесь размеры и скорость, затем пересобирайте проект.
// Единицы длины условные. Углы glRotatef — в градусах, sin/cos — в радианах.
constexpr float towerHeight = 6.0f;      // высота корпуса башни
constexpr float squareTowerWidth = 2.8f; // сторона квадратной башни
constexpr float roundTowerRadius = 1.5f; // радиус круглой башни
constexpr float roofHeight = 2.5f;
constexpr float roundRoofRadius = 1.85f; // небольшой свес над стеной
constexpr float squareRoofRadius = 2.1f; // покрывает углы квадратной башни
constexpr float toothHeight = 0.8f;
constexpr float toothWidthFraction = 0.6f; // 60% шага занято камнем, 40% — промежутком
constexpr float bridgeSpeed = 35.0f;       // градусов в секунду
constexpr float bridgeOpenAngle = 78.0f;
constexpr float bridgeWidth = 2.5f;
constexpr float bridgeThickness = 0.20f;
constexpr float bankMargin = 2.0f;    // земля между стеной и водой
constexpr float bridgeLanding = 0.3f; // запас длины на внешнем берегу
constexpr float hingeHeight = 0.12f;
constexpr float hingeOffset = 0.45f; // шарнир чуть снаружи передней стены
constexpr float plankSpacing = 0.32f;
constexpr float plankLength = 0.26f;
constexpr float plankThickness = 0.045f;
constexpr float firstPlankCenter = 0.18f;
constexpr float textureWorldSize = 2.0f; // один повтор кладки на 2 единицы длины
constexpr float fullTurnDegrees = 360.0f;
constexpr float fieldOfViewDegrees = 45.0f;
constexpr float nearClip = 0.5f;
constexpr float farClip = 180.0f;
constexpr float maximumFrameTime = 0.05f; // защита от скачка после паузы

constexpr float PI = 3.14159265359f;
// Основные параметры сцены. L меняет длину стен, P — шаг зубцов.
float wallLength = 18.0f;
float toothStep = 1.2f;
constexpr float wallHeight = 4.0f;
constexpr float wallThickness = 0.8f;
constexpr float moatWidth = 3.5f;
constexpr float gateRadius = 1.35f;
constexpr float gateSpring = 2.0f; // высота начала полукруглой арки
float bridgeAngle = 0.0f;
bool bridgeRaised = false;
constexpr float initialCameraYaw = 28.0f;
constexpr float initialCameraPitch = 28.0f;
constexpr float initialCameraDistance = 36.0f;
float cameraYaw = initialCameraYaw;
float cameraPitch = initialCameraPitch;
float cameraDistance = initialCameraDistance;
bool dragging = false;
double mouseX = 0, mouseY = 0;
GLuint stoneTexture = 0;

// [11] СТРЕЛЫ. Размеры условные, время в секундах, скорость в единицах/секунду.
constexpr float arrowOpeningBottom = 3.8f;
constexpr float arrowOpeningHeight = 0.9f;
constexpr float arrowOpeningDepth = 0.55f;
constexpr int towerSideSegments = 40;
// Сегменты 8, 9, 10, 11 смотрят вперёд (+Z), симметрично относительно оси башни.
constexpr int openingFirstSegment = 8;
constexpr int openingEndSegment = 12;
constexpr float arrowShotInterval = 0.22f; // каждая башня выпускает стрелу раз в 0.22 с
constexpr float arrowForwardSpeed = 10.0f;
constexpr float arrowUpwardSpeed = 5.0f;
constexpr float arrowGravity = 9.8f;
constexpr float arrowLength = 0.8f;
constexpr float arrowHeadLength = 0.20f;
constexpr float arrowHeadHalfWidth = 0.10f;
constexpr float arrowLineWidth = 2.0f; // толщина линии в пикселях
constexpr float arrowGroundLevel = 0.0f;
float timeUntilNextShot = 0.0f;

struct Arrow
{
    float startX;
    float startY;
    float startZ;
    float age; // сколько секунд прошло после выстрела
};
std::vector<Arrow> arrows; // только стрелы, которые ещё летят

bool isArrowFireActive()
{
    // Стрельба начинается только в самом верхнем положении моста.
    return bridgeRaised && bridgeAngle >= bridgeOpenAngle;
}

struct Vec3
{
    float x, y, z;
};
// ============================================================
// [02] ПРИМИТИВЫ: вершина, грань, блок
// ============================================================
void drawVertex(Vec3 p, float u, float v)
{
    glTexCoord2f(u, v);
    glVertex3f(p.x, p.y, p.z);
}
// Одна грань с нормалью и текстурными координатами.
void drawFace(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n, float u, float v)
{
    glBegin(GL_QUADS);
    glNormal3f(n.x, n.y, n.z);
    drawVertex(a, 0, 0);
    drawVertex(b, u, 0);
    drawVertex(c, u, v);
    drawVertex(d, 0, v);
    glEnd();
}
void useStoneTexture(bool enabled)
{
    if (enabled)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, stoneTexture);
        glColor3f(0.95f, 0.92f, 0.84f);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }
}
// Параллелепипед по центру и размерам. Размер текстуры привязан к метрам.
// Центр блока — НЕ его нижний угол. Для блока на земле centerY = height / 2.
// width идёт вдоль X, height вдоль Y, depth вдоль Z.
// Деление на 2 ниже означает половину размера, это геометрия, не настройка.
void drawBox(float centerX, float centerY, float centerZ, float width, float height, float depth)
{
    float leftX = centerX - width / 2;
    float rightX = centerX + width / 2;
    float bottomY = centerY - height / 2;
    float topY = centerY + height / 2;
    float frontZ = centerZ + depth / 2;
    float backZ = centerZ - depth / 2;
    float textureWidth = width / textureWorldSize;
    float textureHeight = height / textureWorldSize;
    float textureDepth = depth / textureWorldSize;
    drawFace({leftX, bottomY, frontZ}, {rightX, bottomY, frontZ}, {rightX, topY, frontZ},
             {leftX, topY, frontZ}, {0, 0, 1}, textureWidth, textureHeight);
    drawFace({rightX, bottomY, backZ}, {leftX, bottomY, backZ}, {leftX, topY, backZ},
             {rightX, topY, backZ}, {0, 0, -1}, textureWidth, textureHeight);
    drawFace({rightX, bottomY, frontZ}, {rightX, bottomY, backZ}, {rightX, topY, backZ},
             {rightX, topY, frontZ}, {1, 0, 0}, textureDepth, textureHeight);
    drawFace({leftX, bottomY, backZ}, {leftX, bottomY, frontZ}, {leftX, topY, frontZ},
             {leftX, topY, backZ}, {-1, 0, 0}, textureDepth, textureHeight);
    drawFace({leftX, topY, frontZ}, {rightX, topY, frontZ}, {rightX, topY, backZ},
             {leftX, topY, backZ}, {0, 1, 0}, textureWidth, textureDepth);
    drawFace({leftX, bottomY, backZ}, {rightX, bottomY, backZ}, {rightX, bottomY, frontZ},
             {leftX, bottomY, frontZ}, {0, -1, 0}, textureWidth, textureDepth);
}
// Цилиндр и конус имеют общую функцию: верхний радиус конуса равен нулю.
// Цикл идёт по окружности. cos даёт X, sin даёт Z.
// Верхний радиус 0 превращает цилиндр в конус; сегменты приближают круг.
void drawRoundShape(float bottomRadius, float topRadius, float height)
{
    constexpr int segments = 40;
    float sideSlope = (bottomRadius - topRadius) / height;
    float normalLength = std::sqrt(1 + sideSlope * sideSlope);
    glBegin(GL_QUAD_STRIP);
    for (int segmentIndex = 0; segmentIndex <= segments; ++segmentIndex)
    {
        float angleRadians = 2 * PI * segmentIndex / segments;
        float cosine = std::cos(angleRadians);
        float sine = std::sin(angleRadians);
        glNormal3f(cosine / normalLength, sideSlope / normalLength, sine / normalLength);
        float textureU = bottomRadius * angleRadians / textureWorldSize;
        drawVertex({bottomRadius * cosine, 0, bottomRadius * sine}, textureU, 0);
        drawVertex({topRadius * cosine, height, topRadius * sine}, textureU,
                   height / textureWorldSize);
    }
    glEnd();
    // Верхняя крышка цилиндра. У конуса она не нужна.
    if (topRadius > 0)
    {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, 1, 0);
        drawVertex({0, height, 0}, 0.5f, 0.5f);
        for (int segmentIndex = 0; segmentIndex <= segments; ++segmentIndex)
        {
            float angleRadians = -2 * PI * segmentIndex / segments;
            drawVertex(
                {topRadius * std::cos(angleRadians), height, topRadius * std::sin(angleRadians)},
                0.5f + 0.5f * std::cos(angleRadians), 0.5f + 0.5f * std::sin(angleRadians));
        }
        glEnd();
    }
}
// Каменная кладка создаётся один раз на CPU и загружается в GPU.
// ============================================================
// [03] ТЕКСТУРА: изображение кладки в памяти
// ============================================================
GLuint createStoneTexture()
{
    constexpr int size = 256;   // размер изображения в пикселях
    constexpr int channels = 3; // красный, зелёный, синий
    constexpr int brickWidth = 64;
    constexpr int brickHeight = 32;
    constexpr int mortarWidth = 3;
    constexpr int stoneShade = 154;
    constexpr int mortarShade = 75;
    std::vector<unsigned char> pixels(size * size * channels);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            int row = y / brickHeight;
            // Каждый второй ряд сдвинут на половину камня.
            int rowOffset = (row % 2) * (brickWidth / 2);
            int positionInBrick = (x + rowOffset) % brickWidth;
            bool horizontalJoint = y % brickHeight < mortarWidth;
            bool verticalJoint = positionInBrick < mortarWidth;
            int shade = stoneShade;
            if (horizontalJoint || verticalJoint)
            {
                shade = mortarShade;
            }
            // Вектор одномерный: сначала строка y, затем пиксель x, затем канал.
            int pixelIndex = (y * size + x) * channels;
            pixels[pixelIndex] = static_cast<unsigned char>(shade);
            pixels[pixelIndex + 1] = static_cast<unsigned char>(shade);
            pixels[pixelIndex + 2] = static_cast<unsigned char>(shade);
        }
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}
// Стена вдоль локальной X. Зубцы рассчитываются по длине и шагу.
// ============================================================
// [04] СТЕНА: повторение зубцов циклом
// ============================================================
void drawWall(float length)
{
    drawBox(0, wallHeight / 2, 0, length, wallHeight, wallThickness);
    // Отбрасываем дробную часть: неполный шаг не создаёт дополнительный зубец.
    int toothCount = static_cast<int>(length / toothStep);
    if (toothCount < 1)
    {
        toothCount = 1;
    }
    float firstToothX = -0.5f * (toothCount - 1) * toothStep;
    float toothWidth = std::min(toothWidthFraction * toothStep, length);
    for (int toothIndex = 0; toothIndex < toothCount; ++toothIndex)
    {
        float toothX = firstToothX + toothIndex * toothStep;
        float toothCenterY = wallHeight + toothHeight / 2;
        drawBox(toothX, toothCenterY, 0, toothWidth, toothHeight, wallThickness);
    }
}
// [11] Одна вертикальная полоска цилиндра между двумя углами.
// Текстурные координаты считаются от общей окружности, чтобы кладка не обрывалась.
void drawTowerPanel(float startAngle, float endAngle, float bottomY, float topY)
{
    glBegin(GL_QUADS);
    for (int corner = 0; corner < 4; ++corner)
    {
        float angle = startAngle;
        if (corner == 1 || corner == 2)
        {
            angle = endAngle;
        }
        float y = bottomY;
        if (corner >= 2)
        {
            y = topY;
        }
        float x = roundTowerRadius * std::cos(angle);
        float z = roundTowerRadius * std::sin(angle);
        float u = roundTowerRadius * angle / textureWorldSize;
        glNormal3f(std::cos(angle), 0, std::sin(angle));
        drawVertex({x, y, z}, u, y / textureWorldSize);
    }
    glEnd();
}

// [11] Круглый корпус с настоящим углублением: наружная поверхность в проёме отсутствует.
void drawTowerWithOpening()
{
    float openingTop = arrowOpeningBottom + arrowOpeningHeight;
    float backZ = roundTowerRadius - arrowOpeningDepth;
    for (int segment = 0; segment < towerSideSegments; ++segment)
    {
        float startAngle = 2 * PI * segment / towerSideSegments;
        float endAngle = 2 * PI * (segment + 1) / towerSideSegments;
        bool insideOpening = segment >= openingFirstSegment && segment < openingEndSegment;
        if (!insideOpening)
        {
            drawTowerPanel(startAngle, endAngle, 0, towerHeight);
            continue; // эта полоска готова, переходим к следующей
        }
        drawTowerPanel(startAngle, endAngle, 0, arrowOpeningBottom);
        drawTowerPanel(startAngle, endAngle, openingTop, towerHeight);
        float startX = roundTowerRadius * std::cos(startAngle);
        float startZ = roundTowerRadius * std::sin(startAngle);
        float endX = roundTowerRadius * std::cos(endAngle);
        float endZ = roundTowerRadius * std::sin(endAngle);
        // Подоконник и потолок соединяют изогнутую поверхность с задней стенкой.
        drawFace({startX, arrowOpeningBottom, startZ}, {endX, arrowOpeningBottom, endZ},
                 {endX, arrowOpeningBottom, backZ}, {startX, arrowOpeningBottom, backZ}, {0, 1, 0},
                 1, 1);
        drawFace({startX, openingTop, startZ}, {startX, openingTop, backZ},
                 {endX, openingTop, backZ}, {endX, openingTop, endZ}, {0, -1, 0}, 1, 1);
    }
    float edgeAngle = 2 * PI * openingFirstSegment / towerSideSegments;
    float halfWidth = roundTowerRadius * std::cos(edgeAngle);
    float edgeZ = roundTowerRadius * std::sin(edgeAngle);
    for (int side : {-1, 1})
    {
        float x = side * halfWidth;
        drawFace({x, arrowOpeningBottom, edgeZ}, {x, openingTop, edgeZ}, {x, openingTop, backZ},
                 {x, arrowOpeningBottom, backZ}, {static_cast<float>(-side), 0, 0}, 1, 1);
    }
    // Тёмная задняя стенка расположена ВНУТРИ башни, а не поверх её корпуса.
    useStoneTexture(false);
    glColor3f(0.16f, 0.14f, 0.11f);
    drawFace({-halfWidth, arrowOpeningBottom, backZ}, {halfWidth, arrowOpeningBottom, backZ},
             {halfWidth, openingTop, backZ}, {-halfWidth, openingTop, backZ}, {0, 0, 1}, 1, 1);
}

// ============================================================
// [05] БАШНЯ: корпус плюс крыша
// ============================================================
void drawTower(float x, float z, bool circular, bool showOpening = false)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    useStoneTexture(true);
    if (circular)
    {
        if (showOpening)
        {
            drawTowerWithOpening();
        }
        else
        {
            drawRoundShape(roundTowerRadius, roundTowerRadius, towerHeight);
        }
    }
    else
    {
        drawBox(0, towerHeight / 2, 0, squareTowerWidth, towerHeight, squareTowerWidth);
    }
    useStoneTexture(false);
    glColor3f(0.30f, 0.12f, 0.08f);
    glTranslatef(0, towerHeight, 0);
    float roofRadius = squareRoofRadius;
    if (circular)
    {
        roofRadius = roundRoofRadius;
    }
    drawRoundShape(roofRadius, 0, roofHeight);
    glPopMatrix();
}
// Настоящий пустой проём: камень рисуется только НАД полукругом.
// Поэтому сквозь ворота виден двор; чёрная картинка вместо арки не используется.
// ============================================================
// [06] ВОРОТА: камень над дугой
// ============================================================
void drawGate()
{
    constexpr float lintelOffset = 0.2f;
    constexpr float lintelHeight = 0.6f;
    constexpr float lintelOverhang = 0.2f;
    float lintelBottomY = wallHeight + lintelOffset;
    float halfDepth = wallThickness / 2;
    constexpr int segments = 24;
    for (int segmentIndex = 0; segmentIndex < segments; ++segmentIndex)
    {
        float startAngle = PI * segmentIndex / segments;
        float endAngle = PI * (segmentIndex + 1) / segments;
        float startX = gateRadius * std::cos(startAngle);
        float startY = gateSpring + gateRadius * std::sin(startAngle);
        float endX = gateRadius * std::cos(endAngle);
        float endY = gateSpring + gateRadius * std::sin(endAngle);
        drawFace({endX, endY, halfDepth}, {startX, startY, halfDepth},
                 {startX, lintelBottomY, halfDepth}, {endX, lintelBottomY, halfDepth}, {0, 0, 1},
                 (startX - endX) / 2, (lintelBottomY - (startY + endY) / 2) / 2);
        drawFace({startX, startY, -halfDepth}, {endX, endY, -halfDepth},
                 {endX, lintelBottomY, -halfDepth}, {startX, lintelBottomY, -halfDepth}, {0, 0, -1},
                 (startX - endX) / 2, (lintelBottomY - (startY + endY) / 2) / 2);
        float middleAngle = (startAngle + endAngle) / 2;
        drawFace({startX, startY, halfDepth}, {endX, endY, halfDepth}, {endX, endY, -halfDepth},
                 {startX, startY, -halfDepth}, {-std::cos(middleAngle), -std::sin(middleAngle), 0},
                 (endAngle - startAngle) * gateRadius / 2, halfDepth);
    }
    drawBox(0, lintelBottomY + lintelHeight / 2, 0, 2 * (gateRadius + lintelOverhang), lintelHeight,
            wallThickness);
}
// ============================================================
// [07] СЦЕНА: расположение готовых объектов
// ============================================================
// Push запоминает преобразование; Pop возвращает его для следующего объекта.
// Translate переносит локальное начало координат; Rotate разворачивает объект.
void drawCastle()
{
    float halfWallLength = wallLength / 2;
    useStoneTexture(true);
    glPushMatrix();
    glTranslatef(0, 0, -halfWallLength);
    drawWall(wallLength);
    glPopMatrix();
    for (int side : {-1, 1})
    {
        glPushMatrix();
        glTranslatef(side * halfWallLength, 0, 0);
        glRotatef(90, 0, 1, 0);
        drawWall(wallLength);
        glPopMatrix();
    }
    // Передняя стена состоит из двух участков и арочного портала.
    float frontSectionLength = halfWallLength - gateRadius;
    for (int side : {-1, 1})
    {
        glPushMatrix();
        glTranslatef(side * (gateRadius + frontSectionLength / 2), 0, halfWallLength);
        drawWall(frontSectionLength);
        glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(0, 0, halfWallLength);
    drawGate();
    glPopMatrix();
    drawTower(-halfWallLength, halfWallLength, true, isArrowFireActive());
    drawTower(halfWallLength, halfWallLength, true, isArrowFireActive());
    drawTower(-halfWallLength, -halfWallLength, false);
    drawTower(halfWallLength, -halfWallLength, false);
    // Небольшая центральная башня оживляет пустой двор.
    constexpr float courtyardTowerZ = -2.5f;
    drawTower(0, courtyardTowerZ, false);
}
void drawGroundAndMoat()
{
    constexpr float groundThickness = 0.9f;
    constexpr float bankWidth = 24.0f;
    constexpr float bottomDepth = 1.1f;
    constexpr float bottomThickness = 0.4f;
    constexpr float groundExtent = 90.0f;
    constexpr float waterCenterY = -0.56f;
    constexpr float waterThickness = 0.08f;
    constexpr float roadLength = 14.0f;
    constexpr float roadThickness = 0.03f;
    float islandHalfSize = wallLength / 2 + bankMargin;
    float outerBankDistance = islandHalfSize + moatWidth;
    float groundCenterY = -groundThickness / 2;
    float bankCenter = outerBankDistance + bankWidth / 2;
    float outerSize = 2 * (outerBankDistance + bankWidth);
    useStoneTexture(false);
    // Цвета задаются каналами RGB от 0 до 1.
    glColor3f(0.30f, 0.43f, 0.22f); // трава
    drawBox(0, -bottomDepth, 0, groundExtent, bottomThickness, groundExtent);
    glColor3f(0.42f, 0.45f, 0.31f); // земля двора
    drawBox(0, groundCenterY, 0, 2 * islandHalfSize, groundThickness, 2 * islandHalfSize);
    glColor3f(0.30f, 0.43f, 0.22f);
    // side = -1 рисует одну сторону, side = 1 — противоположную.
    for (int side : {-1, 1})
    {
        drawBox(side * bankCenter, groundCenterY, 0, bankWidth, groundThickness, outerSize);
        drawBox(0, groundCenterY, side * bankCenter, 2 * outerBankDistance, groundThickness,
                bankWidth);
    }
    glColor3f(0.12f, 0.40f, 0.53f); // вода
    float waterStripCenter = (islandHalfSize + outerBankDistance) / 2;
    for (int side : {-1, 1})
    {
        drawBox(side * waterStripCenter, waterCenterY, 0, moatWidth, waterThickness,
                2 * outerBankDistance);
        drawBox(0, waterCenterY, side * waterStripCenter, 2 * islandHalfSize, waterThickness,
                moatWidth);
    }
    glColor3f(0.57f, 0.49f, 0.35f); // дорога
    float roadCenterZ = outerBankDistance + roadLength / 2;
    drawBox(0, roadThickness / 2, roadCenterZ, bridgeWidth, roadThickness, roadLength);
}
// ============================================================
// [08] МОСТ: локальное вращение вокруг шарнира
// ============================================================
void drawBridge()
{
    float length = bankMargin + moatWidth + bridgeLanding;
    useStoneTexture(false);
    glPushMatrix();
    glTranslatef(0, hingeHeight, wallLength / 2 + hingeOffset); // шарнир у ворот
    glRotatef(-bridgeAngle, 1, 0, 0);                           // +Z поднимается в +Y
    glColor3f(0.38f, 0.22f, 0.10f);
    drawBox(0, 0, length / 2, bridgeWidth, bridgeThickness, length);
    // Поперечные доски, остаются частью того же преобразования.
    glColor3f(0.53f, 0.33f, 0.17f);
    for (float z = firstPlankCenter; z < length; z += plankSpacing)
    {
        float plankCenterY = bridgeThickness / 2 + plankThickness / 2;
        drawBox(0, plankCenterY, z, bridgeWidth, plankThickness, plankLength);
    }
    glPopMatrix();
}
// ============================================================
// Скорость не зависит от частоты кадров: угол = скорость × время.
void updateBridge(float frameTime)
{
    float targetAngle = 0.0f;
    if (bridgeRaised)
    {
        targetAngle = bridgeOpenAngle;
    }
    // [08] АНИМАЦИЯ. Скорость умножается на время кадра, а не на число кадров.
    float angleStep = bridgeSpeed * frameTime;
    if (bridgeAngle < targetAngle)
    {
        bridgeAngle = std::min(bridgeAngle + angleStep, targetAngle);
    }
    else
    {
        bridgeAngle = std::max(bridgeAngle - angleStep, targetAngle);
    }
}

// [11] Полёт стрел: одинаковая парабола, разные точки старта и время выстрела.
Vec3 getArrowPosition(const Arrow &arrow)
{
    float time = arrow.age;
    float x = arrow.startX; // влево и вправо стрела не движется
    float y = arrow.startY + arrowUpwardSpeed * time - arrowGravity * time * time / 2;
    float z = arrow.startZ + arrowForwardSpeed * time;
    return {x, y, z};
}

void updateArrows(float frameTime)
{
    // Сначала двигаем уже выпущенные стрелы. Они летят и при опускании моста.
    int index = 0;
    while (index < static_cast<int>(arrows.size()))
    {
        arrows[index].age += frameTime;
        Vec3 position = getArrowPosition(arrows[index]);
        if (position.y <= arrowGroundLevel)
        {
            // После удаления следующий элемент сдвигается на это же место.
            arrows.erase(arrows.begin() + index);
        }
        else
        {
            ++index;
        }
    }
    if (!isArrowFireActive())
    {
        timeUntilNextShot = 0; // при следующем подъёме первый выстрел сразу
        return;
    }
    timeUntilNextShot -= frameTime;
    while (timeUntilNextShot <= 0)
    {
        float startY = arrowOpeningBottom + arrowOpeningHeight / 2;
        float startZ = wallLength / 2 + roundTowerRadius - arrowOpeningDepth;
        for (int side : {-1, 1})
        {
            float startX = side * wallLength / 2;
            arrows.push_back({startX, startY, startZ, 0});
        }
        timeUntilNextShot += arrowShotInterval;
    }
}

void drawArrows()
{
    useStoneTexture(false);
    glDisable(GL_LIGHTING); // тонким линиям не нужны нормали и объёмное освещение
    glLineWidth(arrowLineWidth);
    for (const Arrow &arrow : arrows)
    {
        Vec3 position = getArrowPosition(arrow);
        // Вертикальная скорость уменьшается: стрела постепенно направляет нос вниз.
        float verticalSpeed = arrowUpwardSpeed - arrowGravity * arrow.age;
        float pitchRadians = std::atan2(verticalSpeed, arrowForwardSpeed);
        float pitchDegrees = pitchRadians * 180.0f / PI;
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(-pitchDegrees, 1, 0, 0);
        // Начало координат — наконечник. Древко тянется назад вдоль -Z.
        glColor3f(0.30f, 0.16f, 0.06f);
        glBegin(GL_LINES);
        glVertex3f(0, 0, -arrowLength);
        glVertex3f(0, 0, 0);
        glEnd();
        glColor3f(0.15f, 0.15f, 0.17f);
        glBegin(GL_LINES);
        glVertex3f(-arrowHeadHalfWidth, 0, -arrowHeadLength);
        glVertex3f(0, 0, 0);
        glVertex3f(arrowHeadHalfWidth, 0, -arrowHeadLength);
        glVertex3f(0, 0, 0);
        glEnd();
        glPopMatrix();
    }
    glLineWidth(1.0f); // вернуть обычное состояние для следующего кадра
    glEnable(GL_LIGHTING);
}

// [09] ВВОД: события клавиатуры и мыши
// ============================================================
void onKeyPress(GLFWwindow *window, int key, int, int action, int)
{
    if (action != GLFW_PRESS)
    {
        return; // удержание не переключает мост многократно
    }
    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_SPACE)
    {
        bridgeRaised = !bridgeRaised;
    }
    if (key == GLFW_KEY_L)
    {
        constexpr float minimumLength = 14;
        constexpr float maximumLength = 24;
        constexpr float lengthIncrement = 2;
        wallLength += lengthIncrement;
        if (wallLength > maximumLength)
        {
            wallLength = minimumLength;
        }
    }
    if (key == GLFW_KEY_P)
    {
        // Целочисленный индекс исключает ошибки сравнения дробей.
        constexpr float minimumStep = 0.8f;
        constexpr float stepIncrement = 0.2f;
        constexpr int numberOfSteps = 6;
        static int stepIndex = 2; // начальный шаг 1.2 = 0.8 + 2 * 0.2
        stepIndex = (stepIndex + 1) % numberOfSteps;
        toothStep = minimumStep + stepIndex * stepIncrement;
    }
    if (key == GLFW_KEY_R)
    {
        cameraYaw = initialCameraYaw;
        cameraPitch = initialCameraPitch;
        cameraDistance = initialCameraDistance;
    }
    if (key == GLFW_KEY_L || key == GLFW_KEY_P)
    {
        std::cout << "Wall length: " << wallLength << "; tooth step: " << toothStep << '\n';
    }
}
void onMouseButton(GLFWwindow *window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT)
    {
        return;
    }
    dragging = action == GLFW_PRESS;
    glfwGetCursorPos(window, &mouseX, &mouseY);
}
void onMouseMove(GLFWwindow *, double x, double y)
{
    if (dragging)
    {
        constexpr float horizontalSensitivity = 0.35f;
        constexpr float verticalSensitivity = 0.25f;
        constexpr float minimumPitch = 8.0f;
        constexpr float maximumPitch = 75.0f;
        float horizontalMovement = static_cast<float>(x - mouseX);
        float verticalMovement = static_cast<float>(y - mouseY);
        cameraYaw += horizontalMovement * horizontalSensitivity;
        cameraPitch += verticalMovement * verticalSensitivity;
        cameraPitch = std::clamp(cameraPitch, minimumPitch, maximumPitch);
    }
    mouseX = x;
    mouseY = y;
}
void onMouseWheel(GLFWwindow *, double, double y)
{
    constexpr float zoomSpeed = 2.0f;
    constexpr float minimumDistance = 15.0f;
    constexpr float maximumDistance = 65.0f;
    cameraDistance -= static_cast<float>(y) * zoomSpeed;
    cameraDistance = std::clamp(cameraDistance, minimumDistance, maximumDistance);
}
// ============================================================
// [10] ПРОГРАММА: подготовка, кадры, завершение
// ============================================================
// GLFW создаёт окно и получает события. GLAD загружает функции драйвера.
// OpenGL получает команды рисования. CMake только собирает программу.
int main()
{
    glfwSetErrorCallback([](int code, const char *text)
                         { std::cerr << "GLFW " << code << ": " << text << '\n'; });
    if (!glfwInit())
    {
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    // Core Profile не задаём: glBegin, матрицы и GL_LIGHTING требуют старого API.
    GLFWwindow *window = glfwCreateWindow(
        1100, 750, "Castle | Space: bridge | L: walls | P: teeth | R: camera", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "GLAD initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, onKeyPress);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetScrollCallback(window, onMouseWheel);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat ambient[] = {0.36f, 0.36f, 0.36f, 1}, diffuse[] = {0.85f, 0.82f, 0.75f, 1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    stoneTexture = createStoneTexture();
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    std::cout
        << "Drag LMB: orbit; wheel: zoom; Space: bridge; L/P: geometry; R: reset; Esc: exit\n";
    double previousTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        float frameTime = static_cast<float>(currentTime - previousTime);
        frameTime = std::min(frameTime, maximumFrameTime);
        previousTime = currentTime;
        updateBridge(frameTime);
        updateArrows(frameTime);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0)
        {
            glViewport(0, 0, width, height);
            glClearColor(0.65f, 0.78f, 0.87f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            // Половина угла обзора: градусы -> радианы.
            double halfViewAngle = fieldOfViewDegrees * PI / fullTurnDegrees;
            double nearPlane = nearClip;
            double top = nearPlane * std::tan(halfViewAngle);
            double right = top * double(width) / height;
            glFrustum(-right, right, -top, top, nearPlane, farClip);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslatef(0, 0, -cameraDistance);
            glRotatef(cameraPitch, 1, 0, 0);
            glRotatef(cameraYaw, 0, 1, 0);
            glTranslatef(0, -2, 0);
            GLfloat light[] = {-12, 20, 14, 0}; // направленный свет в координатах мира
            glLightfv(GL_LIGHT0, GL_POSITION, light);
            drawGroundAndMoat();
            drawCastle();
            drawBridge();
            drawArrows();
            glfwSwapBuffers(window);
        }
        glfwPollEvents();
    }
    glDeleteTextures(1, &stoneTexture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
