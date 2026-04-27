#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>   // rand, srand
#include <ctime>     // time
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// -------------------------------------------------------
// Window & camera state
// -------------------------------------------------------
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

glm::vec3 camPos(0.0f, 5.0f, 20.0f);
glm::vec3 camFront(0.0f, 0.0f, -1.0f);
glm::vec3 camUp(0.0f, 1.0f, 0.0f);
float camYaw = -90.0f;
float camPitch = 0.0f;

bool  firstMouse = true;
float lastMouseX = SCR_WIDTH / 2.0f;
float lastMouseY = SCR_HEIGHT / 2.0f;
bool  mouseDown = false;

// -------------------------------------------------------
// Shadow-map config
// -------------------------------------------------------
const unsigned int SHADOW_WIDTH = 1024;
const unsigned int SHADOW_HEIGHT = 1024;
const int MAX_LIGHTS = 4;

// -------------------------------------------------------
// AABB helper
// -------------------------------------------------------
struct AABB { float minX, maxX, minZ, maxZ; };

bool aabbOverlap(const AABB& a, const AABB& b) {
    return (a.minX < b.maxX && a.maxX > b.minX &&
        a.minZ < b.maxZ && a.maxZ > b.minZ);
}
AABB makeAABB(float cx, float cz, float hx, float hz) {
    return { cx - hx, cx + hx, cz - hz, cz + hz };
}

// -------------------------------------------------------
// Player car state
// -------------------------------------------------------
glm::vec3 carPos(0.0f, 0.0f, 6.0f);
float     carAngle = 180.0f;
bool      carColliding = false;

// -------------------------------------------------------
// *** RANDOM NPC PEDESTRIANS ***
// Each pedestrian wanders inside a grass patch, changes
// direction randomly every few seconds.
// -------------------------------------------------------
struct NPC {
    glm::vec3 pos;        // current position
    float     angle;      // heading in degrees
    float     speed;      // world units / second
    float     timer;      // countdown to next direction change
    float     interval;   // how often to change direction (seconds)
    glm::vec3 color;      // tint colour for variety
    // Wandering bounds (axis-aligned)
    float bMinX, bMaxX, bMinZ, bMaxZ;
};

std::vector<NPC> npcs;

void initNPCs() {
    srand((unsigned)time(nullptr));

    // Helper: random float in [lo, hi]
    auto rf = [](float lo, float hi) -> float {
        return lo + (float)rand() / RAND_MAX * (hi - lo);
        };

    // Colour palette for pedestrians
    glm::vec3 palette[] = {
        {0.9f,0.2f,0.2f}, {0.2f,0.5f,0.9f}, {0.9f,0.7f,0.1f},
        {0.1f,0.8f,0.4f}, {0.8f,0.2f,0.8f}, {0.2f,0.8f,0.8f}
    };

    // Front grass strip (z ~ -11), 6 pedestrians
    for (int i = 0; i < 6; i++) {
        NPC n;
        n.bMinX = -12.0f; n.bMaxX = 12.0f;
        n.bMinZ = -13.0f; n.bMaxZ = -9.0f;
        n.pos = { rf(n.bMinX, n.bMaxX), 0.0f, rf(n.bMinZ, n.bMaxZ) };
        n.angle = rf(0.0f, 360.0f);
        n.speed = rf(0.8f, 2.0f);
        n.timer = rf(1.0f, 3.5f);
        n.interval = rf(1.5f, 4.0f);
        n.color = palette[i % 6];
        npcs.push_back(n);
    }

    // Back grass strip (z ~ 11), 4 pedestrians
    for (int i = 0; i < 4; i++) {
        NPC n;
        n.bMinX = -12.0f; n.bMaxX = 12.0f;
        n.bMinZ = 9.0f; n.bMaxZ = 13.0f;
        n.pos = { rf(n.bMinX, n.bMaxX), 0.0f, rf(n.bMinZ, n.bMaxZ) };
        n.angle = rf(0.0f, 360.0f);
        n.speed = rf(0.8f, 2.0f);
        n.timer = rf(1.0f, 3.5f);
        n.interval = rf(1.5f, 4.0f);
        n.color = palette[(i + 2) % 6];
        npcs.push_back(n);
    }
}

void updateNPCs(float dt) {
    auto rf = [](float lo, float hi) -> float {
        return lo + (float)rand() / RAND_MAX * (hi - lo);
        };

    for (auto& n : npcs) {
        n.timer -= dt;
        if (n.timer <= 0.0f) {
            // Pick a new random heading
            n.angle = rf(0.0f, 360.0f);
            n.speed = rf(0.8f, 2.0f);
            n.timer = rf(1.0f, n.interval);
        }

        float rad = glm::radians(n.angle);
        glm::vec3 newPos = n.pos;
        newPos.x += sinf(rad) * n.speed * dt;
        newPos.z += cosf(rad) * n.speed * dt;

        // Keep inside bounds (bounce off walls)
        if (newPos.x < n.bMinX || newPos.x > n.bMaxX) {
            n.angle = 180.0f - n.angle;
            newPos = n.pos; // don't cross boundary
        }
        if (newPos.z < n.bMinZ || newPos.z > n.bMaxZ) {
            n.angle = -n.angle;
            newPos = n.pos;
        }

        n.pos = newPos;
    }
}

// -------------------------------------------------------
// *** RULE-BASED AI CARS (circuit followers) ***
// Each AI car follows a sequence of waypoints that trace
// the oval circuit.  On reaching a waypoint it advances
// to the next one.  Speed is constant.
// -------------------------------------------------------
struct AICar {
    glm::vec3 pos;
    float     angle;      // current heading (degrees)
    float     speed;      // world units / second
    int       wpIdx;      // current target waypoint
    glm::vec3 color;      // body tint
};

// Circuit waypoints (centre-line of the road, clockwise)
// Front straight -> right curve -> right straight -> back-right curve
// -> back straight -> left curve -> left straight -> front-left curve
std::vector<glm::vec3> circuitWaypoints = {
    { 4.0f, 0.0f,  6.0f},   // 0 front-right
    { 5.0f, 0.0f,  5.0f},   // 1 entering right-front curve
    { 6.0f, 0.0f,  4.0f},   // 2
    { 6.0f, 0.0f,  0.0f},   // 3 right straight mid
    { 6.0f, 0.0f, -4.0f},   // 4
    { 5.0f, 0.0f, -5.0f},   // 5 entering back-right curve
    { 4.0f, 0.0f, -6.0f},   // 6
    { 0.0f, 0.0f, -6.0f},   // 7 back straight mid
    {-4.0f, 0.0f, -6.0f},   // 8
    {-5.0f, 0.0f, -5.0f},   // 9 entering back-left curve
    {-6.0f, 0.0f, -4.0f},   // 10
    {-6.0f, 0.0f,  0.0f},   // 11 left straight mid
    {-6.0f, 0.0f,  4.0f},   // 12
    {-5.0f, 0.0f,  5.0f},   // 13 entering front-left curve
    {-4.0f, 0.0f,  6.0f},   // 14
    { 0.0f, 0.0f,  6.0f},   // 15 front straight mid
};

std::vector<AICar> aiCars;

void initAICars() {
    // Spread 3 AI cars around the track at different starting waypoints
    struct Def { int wp; float speed; glm::vec3 color; };
    Def defs[] = {
        { 0, 5.0f, {0.1f, 0.3f, 0.9f}},   // blue
        { 5, 5.0f, {0.1f, 0.8f, 0.2f}},   // green
        {10, 5.0f, {0.9f, 0.5f, 0.0f}},   // orange
    };
    for (auto& d : defs) {
        AICar a;
        a.pos = circuitWaypoints[d.wp];
        a.wpIdx = (d.wp + 1) % (int)circuitWaypoints.size();
        a.speed = d.speed;
        a.color = d.color;
        // Compute initial heading toward first waypoint
        glm::vec3 dir = circuitWaypoints[a.wpIdx] - a.pos;
        a.angle = glm::degrees(atan2f(dir.x, dir.z));
        aiCars.push_back(a);
    }
}

void updateAICars(float dt) {
    int n = (int)circuitWaypoints.size();
    for (auto& a : aiCars) {
        glm::vec3 target = circuitWaypoints[a.wpIdx];
        glm::vec3 toTarget = target - a.pos;
        float dist = glm::length(toTarget);

        if (dist < 0.3f) {
            // Advance to next waypoint
            a.wpIdx = (a.wpIdx + 1) % n;
            target = circuitWaypoints[a.wpIdx];
            toTarget = target - a.pos;
        }

        // Smoothly turn toward target
        glm::vec3 dir = glm::normalize(toTarget);
        float targetAngle = glm::degrees(atan2f(dir.x, dir.z));

        // Shortest-path angle interpolation
        float diff = targetAngle - a.angle;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        a.angle += diff * glm::clamp(dt * 8.0f, 0.0f, 1.0f);

        // Move forward
        float rad = glm::radians(a.angle);
        a.pos.x += sinf(rad) * a.speed * dt;
        a.pos.z += cosf(rad) * a.speed * dt;

    }
}

// -------------------------------------------------------
// DEPTH (SHADOW) SHADERS
// -------------------------------------------------------
const char* depthVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
void main(){
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

const char* depthFS = R"(
#version 330 core
void main(){}
)";

// -------------------------------------------------------
// MAIN VERTEX SHADER
// -------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 FragNormal;
out vec4 FragPosLightSpace[4];

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix[4];
uniform int  numLights;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos       = worldPos.xyz;
    TexCoord      = aTex;

    mat3 normalMat = transpose(inverse(mat3(model)));
    FragNormal     = normalize(normalMat * vec3(0,1,0));

    for(int i = 0; i < numLights; i++)
        FragPosLightSpace[i] = lightSpaceMatrix[i] * worldPos;

    gl_Position = projection * view * worldPos;
}
)";

// -------------------------------------------------------
// MAIN FRAGMENT SHADER  (Phong + PCF shadows)
// -------------------------------------------------------
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 FragNormal;
in vec4 FragPosLightSpace[4];

uniform sampler2D texture1;
uniform sampler2D shadowMap[4];
uniform vec3  lightPos[4];
uniform vec3  lightColor[4];
uniform int   numLights;
uniform vec3  tint;
uniform bool  isRoad;
uniform bool  isSky;

float shadowFactor(sampler2D smap, vec4 fragPosLS){
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj = proj * 0.5 + 0.5;
    if(proj.z > 1.0) return 0.0;

    float shadow    = 0.0;
    float bias      = 0.003;
    vec2  texelSize = 1.0 / vec2(textureSize(smap, 0));
    for(int x = -1; x <= 1; x++)
        for(int y = -1; y <= 1; y++){
            float pcfDepth = texture(smap, proj.xy + vec2(x,y)*texelSize).r;
            shadow += (proj.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

void main(){
    vec3 albedo = texture(texture1, TexCoord).rgb * tint;

    if(isRoad){
        float centerDist = abs(TexCoord.y - 0.5);
        float dash = mod(TexCoord.x * 6.0, 1.0);
        if((centerDist > 0.42 && centerDist < 0.48) ||
           (centerDist < 0.04 && dash < 0.5))
            albedo = vec3(1.0);
    }

    if(isSky){ FragColor = vec4(albedo, 1.0); return; }

    vec3 norm    = normalize(FragNormal);
    vec3 ambient = vec3(0.12);
    vec3 diffuse = vec3(0.0);

    for(int i = 0; i < numLights; i++){
        vec3  dir   = normalize(lightPos[i] - FragPos);
        float diff  = max(dot(norm, dir), 0.0);
        float dist  = length(lightPos[i] - FragPos);
        float atten = 1.0 / (1.0 + 0.09*dist + 0.032*dist*dist);
        float shad  = shadowFactor(shadowMap[i], FragPosLightSpace[i]);
        diffuse += (1.0 - shad) * diff * lightColor[i] * atten;
    }

    FragColor = vec4((ambient + diffuse) * albedo, 1.0);
}
)";

// -------------------------------------------------------
// Geometry helpers
// -------------------------------------------------------
float cubeVertices[] = {
    -0.5f,-0.5f,-0.5f,0,0,  0.5f,-0.5f,-0.5f,1,0,  0.5f,0.5f,-0.5f,1,1,
     0.5f, 0.5f,-0.5f,1,1, -0.5f,0.5f,-0.5f,0,1,  -0.5f,-0.5f,-0.5f,0,0,
    -0.5f,-0.5f, 0.5f,0,0,  0.5f,-0.5f,0.5f,1,0,   0.5f,0.5f,0.5f,1,1,
     0.5f, 0.5f, 0.5f,1,1, -0.5f,0.5f,0.5f,0,1,   -0.5f,-0.5f,0.5f,0,0,
    -0.5f, 0.5f, 0.5f,1,0, -0.5f,0.5f,-0.5f,1,1,  -0.5f,-0.5f,-0.5f,0,1,
    -0.5f,-0.5f,-0.5f,0,1, -0.5f,-0.5f,0.5f,0,0,  -0.5f,0.5f,0.5f,1,0,
     0.5f, 0.5f, 0.5f,1,0,  0.5f,0.5f,-0.5f,1,1,   0.5f,-0.5f,-0.5f,0,1,
     0.5f,-0.5f,-0.5f,0,1,  0.5f,-0.5f,0.5f,0,0,   0.5f,0.5f,0.5f,1,0,
    -0.5f, 0.5f,-0.5f,0,1,  0.5f,0.5f,-0.5f,1,1,   0.5f,0.5f,0.5f,1,0,
     0.5f, 0.5f, 0.5f,1,0, -0.5f,0.5f,0.5f,0,0,   -0.5f,0.5f,-0.5f,0,1,
    -0.5f,-0.5f,-0.5f,0,1,  0.5f,-0.5f,-0.5f,1,1,  0.5f,-0.5f,0.5f,1,0,
     0.5f,-0.5f, 0.5f,1,0, -0.5f,-0.5f,0.5f,0,0,  -0.5f,-0.5f,-0.5f,0,1
};

std::vector<float> createCurve(float radius, float width, int segments) {
    std::vector<float> v;
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float angle = t * glm::half_pi<float>();
        float c = cosf(angle), s = sinf(angle);
        float outer = radius + width * 0.5f;
        float inner = radius - width * 0.5f;
        v.insert(v.end(), { outer * c, 0.05f, outer * s, t, 1.0f });
        v.insert(v.end(), { inner * c, 0.05f, inner * s, t, 0.0f });
    }
    return v;
}

// -------------------------------------------------------
// Texture loader
// -------------------------------------------------------
unsigned int loadTexture(const char* path) {
    unsigned int tex;
    glGenTextures(1, &tex);
    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (!data) { std::cerr << "Missing texture: " << path << "\n"; return tex; }
    GLenum fmt = (n == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

// -------------------------------------------------------
// Shader compiler / linker
// -------------------------------------------------------
unsigned int compileShader(GLenum type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
        std::cerr << "Shader error:\n" << log << "\n";
    }
    return s;
}
unsigned int linkProgram(const char* vs, const char* fs) {
    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    unsigned int p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// -------------------------------------------------------
// GLFW callbacks
// -------------------------------------------------------
void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        mouseDown = (action == GLFW_PRESS);
}

void cursor_pos_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastMouseX = (float)xpos; lastMouseY = (float)ypos; firstMouse = false; }
    float dx = ((float)xpos - lastMouseX) * 0.1f;
    float dy = (lastMouseY - (float)ypos) * 0.1f;
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;
    if (!mouseDown) return;

    camYaw += dx;
    camPitch = glm::clamp(camPitch + dy, -89.0f, 89.0f);

    camFront = glm::normalize(glm::vec3(
        cosf(glm::radians(camYaw)) * cosf(glm::radians(camPitch)),
        sinf(glm::radians(camPitch)),
        sinf(glm::radians(camYaw)) * cosf(glm::radians(camPitch))
    ));
}

void scroll_callback(GLFWwindow*, double, double yoff) {
    camPos += camFront * (float)yoff * 1.5f;
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    const float speed = 0.4f;
    glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_W) camPos += speed * camFront;
        if (key == GLFW_KEY_S) camPos -= speed * camFront;
        if (key == GLFW_KEY_A) camPos -= speed * right;
        if (key == GLFW_KEY_D) camPos += speed * right;
        if (key == GLFW_KEY_Q) camPos += speed * camUp;
        if (key == GLFW_KEY_E) camPos -= speed * camUp;
    }
}

// -------------------------------------------------------
// Helper: draw a mini car at given position, angle, tint
// -------------------------------------------------------
// (defined after VAO/shader setup – see lambda inside main)

// -------------------------------------------------------
// Main
// -------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "Street Circuit",
        NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    stbi_set_flip_vertically_on_load(1);

    // Shaders
    unsigned int prog = linkProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int depthProg = linkProgram(depthVS, depthFS);

    // Lights
    int numLights = 4;
    glm::vec3 lightPositions[MAX_LIGHTS] = {
        { 7.0f, 6.0f,  7.0f}, {-7.0f, 6.0f,  7.0f},
        {-7.0f, 6.0f, -7.0f}, { 7.0f, 6.0f, -7.0f},
    };
    glm::vec3 lightColors[MAX_LIGHTS] = {
        {1.0f,0.95f,0.7f},{1.0f,0.95f,0.7f},
        {1.0f,0.95f,0.7f},{1.0f,0.95f,0.7f},
    };

    // Shadow FBOs
    unsigned int shadowFBO[MAX_LIGHTS], shadowTex[MAX_LIGHTS];
    for (int i = 0; i < numLights; i++) {
        glGenFramebuffers(1, &shadowFBO[i]);
        glGenTextures(1, &shadowTex[i]);
        glBindTexture(GL_TEXTURE_2D, shadowTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
            SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float bCol[] = { 1,1,1,1 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bCol);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, shadowTex[i], 0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Cube VAO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    const float SW = 2.0f, SL = 10.0f, Y0 = 0.05f, CR = SW / 2.0f;
    const int   SEG = 32;
    auto curveData = createCurve(CR, SW, SEG);
    int  curveVerts = (int)curveData.size() / 5;

    unsigned int cVAO, cVBO;
    glGenVertexArrays(1, &cVAO); glGenBuffers(1, &cVBO);
    glBindVertexArray(cVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cVBO);
    glBufferData(GL_ARRAY_BUFFER, curveData.size() * sizeof(float), curveData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Textures
    unsigned int roadTex = loadTexture("textures/road.jpg");
    unsigned int grassTex = loadTexture("textures/grass.jpg");
    unsigned int buildingTex = loadTexture("textures/building.jpg");
    unsigned int roofTex = loadTexture("textures/roof.jpg");
    unsigned int skyTex = loadTexture("textures/sky.jpg");

    // Uniform locations (main prog)
    auto uLoc = [&](const char* n) { return glGetUniformLocation(prog, n); };
    int uModel = uLoc("model");
    int uView = uLoc("view");
    int uProj = uLoc("projection");
    int uTint = uLoc("tint");
    int uRoad = uLoc("isRoad");
    int uSky = uLoc("isSky");
    int uNumL = uLoc("numLights");

    int dModel = glGetUniformLocation(depthProg, "model");
    int dLSM = glGetUniformLocation(depthProg, "lightSpaceMatrix");

    // Draw helpers
    auto setModel = [&](const glm::mat4& m) {
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(m));
        };
    auto drawCube = [&](const glm::mat4& m) {
        glBindVertexArray(VAO); setModel(m); glDrawArrays(GL_TRIANGLES, 0, 36);
        };
    auto drawCurve = [&](const glm::mat4& m) {
        glBindVertexArray(cVAO); setModel(m); glDrawArrays(GL_TRIANGLE_STRIP, 0, curveVerts);
        };
    auto setModel_d = [&](const glm::mat4& m) {
        glUniformMatrix4fv(dModel, 1, GL_FALSE, glm::value_ptr(m));
        };
    auto drawCube_d = [&](const glm::mat4& m) {
        glBindVertexArray(VAO); setModel_d(m); glDrawArrays(GL_TRIANGLES, 0, 36);
        };
    auto drawCurve_d = [&](const glm::mat4& m) {
        glBindVertexArray(cVAO); setModel_d(m); glDrawArrays(GL_TRIANGLE_STRIP, 0, curveVerts);
        };

    // -------------------------------------------------------
    // Helper: draw a car-shaped object (body + cabin + wheels)
    // Used for both player car and AI cars
    // -------------------------------------------------------
    auto drawCarShape = [&](
        glm::vec3 pos, float angle, glm::vec3 bodyColor, bool depth)
        {
            glm::mat4 base = glm::rotate(
                glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, 0.25f, pos.z)),
                glm::radians(angle), glm::vec3(0, 1, 0));

            if (!depth) {
                glBindTexture(GL_TEXTURE_2D, buildingTex);
                glUniform3f(uTint, bodyColor.r, bodyColor.g, bodyColor.b);
            }
            // Body
            if (depth) drawCube_d(glm::scale(base, glm::vec3(1.2f, 0.35f, 2.0f)));
            else       drawCube(glm::scale(base, glm::vec3(1.2f, 0.35f, 2.0f)));

            // Cabin
            glm::vec3 cabinColor = depth ? bodyColor : bodyColor * 0.9f + glm::vec3(0.1f);
            if (!depth) glUniform3f(uTint, cabinColor.r, cabinColor.g, cabinColor.b);
            glm::mat4 cabinM = glm::scale(
                glm::translate(base, glm::vec3(0.0f, 0.28f, -0.1f)),
                glm::vec3(0.8f, 0.32f, 1.0f));
            if (depth) drawCube_d(cabinM); else drawCube(cabinM);

            // Wheels
            if (!depth) glUniform3f(uTint, 0.15f, 0.15f, 0.15f);
            float wx = 0.72f, wz = 0.7f;
            glm::vec2 wOff[4] = { {wx,wz},{-wx,wz},{wx,-wz},{-wx,-wz} };
            for (auto& wo : wOff) {
                glm::mat4 wm = glm::scale(
                    glm::translate(base, glm::vec3(wo.x, -0.18f, wo.y)),
                    glm::vec3(0.22f, 0.22f, 0.45f));
                if (depth) drawCube_d(wm); else drawCube(wm);
            }
            if (!depth) { glUniform3f(uTint, 1, 1, 1); glUniform1i(uRoad, 0); }
        };

    // -------------------------------------------------------
    // Helper: draw a pedestrian (capsule-ish: body + head)
    // -------------------------------------------------------
    auto drawNPC = [&](const NPC& n, bool depth) {
        glm::mat4 base = glm::rotate(
            glm::translate(glm::mat4(1.0f), glm::vec3(n.pos.x, 0.4f, n.pos.z)),
            glm::radians(n.angle), glm::vec3(0, 1, 0));

        // Body
        if (!depth) {
            glBindTexture(GL_TEXTURE_2D, buildingTex);
            glUniform3f(uTint, n.color.r, n.color.g, n.color.b);
        }
        glm::mat4 bodyM = glm::scale(base, glm::vec3(0.25f, 0.6f, 0.18f));
        if (depth) drawCube_d(bodyM); else drawCube(bodyM);

        // Head
        if (!depth) glUniform3f(uTint, 0.9f, 0.75f, 0.6f);
        glm::mat4 headM = glm::scale(
            glm::translate(base, glm::vec3(0.0f, 0.45f, 0.0f)),
            glm::vec3(0.18f, 0.18f, 0.18f));
        if (depth) drawCube_d(headM); else drawCube(headM);

        if (!depth) { glUniform3f(uTint, 1, 1, 1); glUniform1i(uRoad, 0); }
        };

    // Scene data
    struct CornerDef { float ox, oz, rotDeg; };
    CornerDef cornerDefs[4] = {
        { 5.0f,  5.0f,   0.0f},
        {-5.0f,  5.0f, 270.0f},
        {-5.0f, -5.0f, 180.0f},
        { 5.0f, -5.0f,  90.0f},
    };

    struct BldgDef { float x, z, h; };
    std::vector<BldgDef> buildings;
    for (int i = 0; i < 6; i++)
        buildings.push_back({ -10.0f + i * 4.0f, 11.0f, 3.5f + (i % 3) * 1.0f });
    for (int i = 0; i < 4; i++) {
        float bx = (i < 2) ? -11.0f : 11.0f;
        float bz = -4.0f + (i % 2) * 8.0f;
        buildings.push_back({ bx, bz, 4.5f });
    }

    std::vector<AABB> buildingAABBs;
    for (auto& b : buildings)
        buildingAABBs.push_back(makeAABB(b.x, b.z, 1.0f, 1.0f));

    const float CAR_HX = 0.6f;
    const float CAR_HZ = 1.0f;
    const float CAR_SPEED = 5.0f;
    const float CAR_TURN = 60.0f;

    std::vector<glm::vec2> trees;
    for (int i = 0; i < 6; i++)
        trees.push_back({ -10.0f + i * 4.0f, -11.0f });

    // -------------------------------------------------------
    // Scene draw lambda
    // -------------------------------------------------------
    auto drawScene = [&](bool depth) {
        auto doCube = [&](const glm::mat4& m) { depth ? drawCube_d(m) : drawCube(m);  };
        auto doCurve = [&](const glm::mat4& m) { depth ? drawCurve_d(m) : drawCurve(m); };

        // Ground
        if (!depth) {
            glPolygonOffset(0, 0); glBindTexture(GL_TEXTURE_2D, grassTex);
            glUniform3f(uTint, 1, 1, 1); glUniform1i(uRoad, 0);
        }
        doCube(glm::scale(glm::mat4(1.0f), glm::vec3(30, 0.1f, 30)));

        // Road straights
        if (!depth) {
            glPolygonOffset(-1.0f, -1.0f); glBindTexture(GL_TEXTURE_2D, roadTex);
            glUniform1i(uRoad, 1);
        }
        doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(0, Y0, 6)), glm::vec3(SL, 0.1f, SW)));
        doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(0, Y0, -6)), glm::vec3(SL, 0.1f, SW)));
        {
            glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(-6, Y0, 0)),
                glm::radians(90.0f), glm::vec3(0, 1, 0));
            doCube(glm::scale(m, glm::vec3(SL, 0.1f, SW)));
        }
        {
            glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(6, Y0, 0)),
                glm::radians(90.0f), glm::vec3(0, 1, 0));
            doCube(glm::scale(m, glm::vec3(SL, 0.1f, SW)));
        }

        // Road corners
        if (!depth) glPolygonOffset(-2.0f, -2.0f);
        for (auto& cd : cornerDefs) {
            glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(cd.ox, 0, cd.oz)),
                glm::radians(cd.rotDeg), glm::vec3(0, 1, 0));
            doCurve(m);
        }
        if (!depth) { glPolygonOffset(0, 0); glUniform1i(uRoad, 0); }

        // Buildings
        for (auto& b : buildings) {
            if (!depth) { glBindTexture(GL_TEXTURE_2D, buildingTex); glUniform3f(uTint, 1, 1, 1); }
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(b.x, b.h * 0.5f, b.z)),
                glm::vec3(2, b.h, 2)));
            if (!depth) glBindTexture(GL_TEXTURE_2D, roofTex);
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(b.x, b.h + 0.12f, b.z)),
                glm::vec3(2.05f, 0.24f, 2.05f)));
        }

        // Trees
        for (auto& t : trees) {
            if (!depth) { glBindTexture(GL_TEXTURE_2D, buildingTex); glUniform3f(uTint, 0.45f, 0.28f, 0.10f); }
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(t.x, 0.6f, t.y)), glm::vec3(0.35f, 1.2f, 0.35f)));
            if (!depth) { glBindTexture(GL_TEXTURE_2D, grassTex); glUniform3f(uTint, 0.15f, 0.70f, 0.15f); }
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(t.x, 1.65f, t.y)), glm::vec3(1.5f, 0.9f, 1.5f)));
            if (!depth) glUniform3f(uTint, 0.10f, 0.55f, 0.10f);
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(t.x, 2.55f, t.y)), glm::vec3(1.0f, 0.9f, 1.0f)));
            if (!depth) glUniform3f(uTint, 0.08f, 0.42f, 0.08f);
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(t.x, 3.35f, t.y)), glm::vec3(0.55f, 0.7f, 0.55f)));
        }

        // Streetlights
        for (int i = 0; i < numLights; i++) {
            glm::vec3 lp = lightPositions[i];
            float poleTop = lp.y * 0.9f;
            float armDir = (lp.x > 0) ? -1.0f : 1.0f;
            if (!depth) { glBindTexture(GL_TEXTURE_2D, buildingTex); glUniform3f(uTint, 0.6f, 0.6f, 0.65f); }
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(lp.x, lp.y * 0.45f, lp.z)),
                glm::vec3(0.18f, lp.y * 0.9f, 0.18f)));
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(lp.x + armDir * 0.5f, poleTop, lp.z)),
                glm::vec3(1.2f, 0.12f, 0.12f)));
            if (!depth) glUniform3f(uTint, 1.0f, 0.95f, 0.5f);
            doCube(glm::scale(glm::translate(glm::mat4(1), glm::vec3(lp.x + armDir * 1.1f, poleTop - 0.1f, lp.z)),
                glm::vec3(0.5f, 0.22f, 0.5f)));
        }

        // *** PLAYER CAR ***
        {
            glm::vec3 playerColor = carColliding
                ? glm::vec3(1.0f, 0.0f, 0.8f)
                : glm::vec3(0.85f, 0.05f, 0.05f);
            drawCarShape(carPos, carAngle, playerColor, depth);
        }

        // *** RULE-BASED AI CARS ***
        for (auto& a : aiCars)
            drawCarShape(a.pos, a.angle, a.color, depth);

        // *** RANDOM NPC PEDESTRIANS ***
        for (auto& n : npcs)
            drawNPC(n, depth);

        if (!depth) { glUniform3f(uTint, 1, 1, 1); glUniform1i(uRoad, 0); }
        };

    // Init moving objects
    initNPCs();
    initAICars();

    // -------------------------------------------------------
    // Render loop
    // -------------------------------------------------------
    double prevTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double nowTime = glfwGetTime();
        float  dt = (float)(nowTime - prevTime);
        prevTime = nowTime;

        // *** UPDATE MOVING OBJECTS ***
        updateNPCs(dt);
        updateAICars(dt);

        // *** PLAYER CAR CONTROLS ***
        bool turnLeft = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS;
        bool turnRight = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        bool driveF = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
        bool driveB = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS;

        if (turnLeft)  carAngle += CAR_TURN * dt;
        if (turnRight) carAngle -= CAR_TURN * dt;

        float rad = glm::radians(carAngle);
        glm::vec3 forward(sinf(rad), 0.0f, cosf(rad));

        glm::vec3 newPos = carPos;
        if (driveF) newPos += forward * CAR_SPEED * dt;
        if (driveB) newPos -= forward * CAR_SPEED * dt;

        AABB carAABB = makeAABB(newPos.x, newPos.z, CAR_HX, CAR_HZ);
        carColliding = false;
        for (auto& bAABB : buildingAABBs)
            if (aabbOverlap(carAABB, bAABB)) { carColliding = true; break; }
        if (!carColliding) carPos = newPos;

        static bool prevColliding = false;
        if (carColliding && !prevColliding)
            std::cout << "[COLLISION] Car hit a building at ("
            << carPos.x << ", " << carPos.z << ")\n";
        prevColliding = carColliding;

        // Light-space matrices
        glm::mat4 lightSpaceMat[MAX_LIGHTS];
        float orthoSize = 20.0f;
        glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < numLights; i++) {
            glm::mat4 lView = glm::lookAt(lightPositions[i], sceneCenter, glm::vec3(0, 1, 0));
            glm::mat4 lProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 60.0f);
            lightSpaceMat[i] = lProj * lView;
        }

        // PASS 1 – Shadow maps
        glUseProgram(depthProg);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glCullFace(GL_FRONT);
        for (int i = 0; i < numLights; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO[i]);
            glClear(GL_DEPTH_BUFFER_BIT);
            glUniformMatrix4fv(dLSM, 1, GL_FALSE, glm::value_ptr(lightSpaceMat[i]));
            drawScene(true);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glCullFace(GL_BACK);

        // PASS 2 – Colour render
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);

        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
            (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 500.0f);
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj));

        glUniform1i(uNumL, numLights);
        for (int i = 0; i < numLights; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "lightPos[%d]", i); glUniform3fv(glGetUniformLocation(prog, buf), 1, glm::value_ptr(lightPositions[i]));
            snprintf(buf, sizeof(buf), "lightColor[%d]", i); glUniform3fv(glGetUniformLocation(prog, buf), 1, glm::value_ptr(lightColors[i]));
            snprintf(buf, sizeof(buf), "lightSpaceMatrix[%d]", i);
            glUniformMatrix4fv(glGetUniformLocation(prog, buf), 1, GL_FALSE, glm::value_ptr(lightSpaceMat[i]));
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_2D, shadowTex[i]);
            snprintf(buf, sizeof(buf), "shadowMap[%d]", i);
            glUniform1i(glGetUniformLocation(prog, buf), 1 + i);
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "texture1"), 0);

        // Skybox
        glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, skyTex);
        glUniform1i(uRoad, 0); glUniform3f(uTint, 1, 1, 1); glUniform1i(uSky, 1);
        drawCube(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 55, 0)), glm::vec3(200, 200, 200)));
        glUniform1i(uSky, 0);
        glDepthMask(GL_TRUE);

        drawScene(false);
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);  glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &cVAO); glDeleteBuffers(1, &cVBO);
    for (int i = 0; i < numLights; i++) {
        glDeleteFramebuffers(1, &shadowFBO[i]);
        glDeleteTextures(1, &shadowTex[i]);
    }
    glDeleteProgram(prog);
    glDeleteProgram(depthProg);
    glfwTerminate();
    return 0;
}