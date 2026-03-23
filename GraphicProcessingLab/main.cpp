#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

float camX = -0.0f, camY = 14.0f, camZ = 24.0f;

// -------------------------------------------------------
// VERTEX SHADER
// -------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos     = vec3(model * vec4(aPos, 1.0));
    TexCoord    = aTex;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// -------------------------------------------------------
// FRAGMENT SHADER
// -------------------------------------------------------
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;

uniform sampler2D texture1;
uniform vec3 tint;
uniform bool isRoad;
uniform bool isSky;

vec3 lightPos = vec3(5.0, 10.0, 5.0);

void main() {
    vec3 color = texture(texture1, TexCoord).rgb * tint;

    if (isRoad) {
        float centerDist = abs(TexCoord.y - 0.5);
        float dash       = mod(TexCoord.x * 6.0, 1.0);
        if ((centerDist > 0.42 && centerDist < 0.48) ||
            (centerDist < 0.04 && dash < 0.5))
            color = vec3(1.0);
    }

    vec3  lightDir = normalize(lightPos - FragPos);
    float diff     = max(dot(vec3(0,1,0), lightDir), 0.3);
    FragColor = isSky ? vec4(color, 1.0) : vec4(color * diff, 1.0);
}
)";

// -------------------------------------------------------
// Cube geometry
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
        float angle = t * glm::half_pi<float>();   // 0 → 90 degrees
        float c = cosf(angle), s = sinf(angle);

        float outer = radius + width * 0.5f;
        float inner = radius - width * 0.5f;

        // outer vertex
        v.insert(v.end(), { outer * c, 0.05f, outer * s,  t, 1.0f });
        // inner vertex
        v.insert(v.end(), { inner * c, 0.05f, inner * s,  t, 0.0f });
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
// Main
// -------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Street Circuit", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    stbi_set_flip_vertically_on_load(1);

    // --- Shaders ---
    auto compile = [](GLenum type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        return s;
        };
    unsigned int vs = compile(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    // --- Cube VAO ---
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    
    const float SW = 2.0f;   // road width
    const float SL = 10.0f;  // straight length (between corner pockets)
    const float Y0 = 0.05f;
    const int   SEG = 32;     // smoothness of the curve
    const float CR = SW / 2.0f;  // curve radius = 1.0 (half road width)

    auto curveData = createCurve(CR, SW, SEG);
    int  curveVerts = (int)curveData.size() / 5;   // 5 floats per vertex

    unsigned int cVAO, cVBO;
    glGenVertexArrays(1, &cVAO);
    glGenBuffers(1, &cVBO);
    glBindVertexArray(cVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cVBO);
    glBufferData(GL_ARRAY_BUFFER, curveData.size() * sizeof(float), curveData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- Textures ---
    unsigned int roadTex = loadTexture("textures/road.jpg");
    unsigned int grassTex = loadTexture("textures/grass.jpg");
    unsigned int buildingTex = loadTexture("textures/building.jpg");
    unsigned int roofTex = loadTexture("textures/roof.jpg");
    unsigned int skyTex = loadTexture("textures/sky.jpg");

    // --- Uniforms ---
    int uModel = glGetUniformLocation(prog, "model");
    int uView = glGetUniformLocation(prog, "view");
    int uProj = glGetUniformLocation(prog, "projection");
    int uTint = glGetUniformLocation(prog, "tint");
    int uRoad = glGetUniformLocation(prog, "isRoad");
    int uSky = glGetUniformLocation(prog, "isSky");

    // Helper lambdas
    auto drawCube = [&](const glm::mat4& m) {
        glBindVertexArray(VAO);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_TRIANGLES, 0, 36);
        };
    auto drawCurve = [&](const glm::mat4& m) {
        glBindVertexArray(cVAO);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, curveVerts);
        };
    auto tint = [&](float r, float g, float b) { glUniform3f(uTint, r, g, b); };
    auto road = [&](bool v) { glUniform1i(uRoad, v ? 1 : 0); };

    

    struct CornerDef { float ox; float oz; float rotDeg; };
    CornerDef cornerDefs[4] = {
        {  5.0f,  5.0f,  0.0f },   // top-right
        { -5.0f,  5.0f, 270.0f },   // top-left
        { -5.0f, -5.0f, 180.0f },   // bottom-left
        {  5.0f, -5.0f, 90.0f },   // bottom-right
    };

    // -------------------------------------------------------
    // Render loop
    // -------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);

        glm::mat4 view = glm::lookAt(
            glm::vec3(camX, camY, camZ), glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 500.0f);
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj));

        road(false);
        tint(1, 1, 1);

        // --------------------------------------------------
        // SKY BOX
        // --------------------------------------------------
        glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, skyTex);
        road(false);
        tint(1, 1, 1);
        glUniform1i(uSky, 1);  // disable lighting for sky
        drawCube(glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 55.0f, 0.0f)),
            glm::vec3(200.0f, 200.0f, 200.0f)));
        glUniform1i(uSky, 0);  // re-enable lighting for everything else
        glDepthMask(GL_TRUE);

        // --------------------------------------------------
        // GROUND
        // --------------------------------------------------
        glPolygonOffset(0, 0);
        glBindTexture(GL_TEXTURE_2D, grassTex);
        drawCube(glm::scale(glm::mat4(1.0f), glm::vec3(30, 0.1f, 30)));

        // --------------------------------------------------
        // ROAD STRAIGHTS
        // --------------------------------------------------
        glPolygonOffset(-1.0f, -1.0f);
        glBindTexture(GL_TEXTURE_2D, roadTex);
        road(true);

        // Top straight    (Z=+6, along X, length=SL)
        drawCube(glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(0, Y0, 6)),
            glm::vec3(SL, 0.1f, SW)));

        // Bottom straight (Z=-6)
        drawCube(glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(0, Y0, -6)),
            glm::vec3(SL, 0.1f, SW)));

        // Left straight   (X=-6, along Z)
        {
            glm::mat4 m = glm::translate(glm::mat4(1), glm::vec3(-6, Y0, 0));
            m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 1, 0));
            m = glm::scale(m, glm::vec3(SL, 0.1f, SW));
            drawCube(m);
        }

        // Right straight  (X=+6, along Z)
        {
            glm::mat4 m = glm::translate(glm::mat4(1), glm::vec3(6, Y0, 0));
            m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 1, 0));
            m = glm::scale(m, glm::vec3(SL, 0.1f, SW));
            drawCube(m);
        }

        // --------------------------------------------------
        // ROAD CORNERS 
        // --------------------------------------------------
        glPolygonOffset(-2.0f, -2.0f);
        road(true);   

        for (auto& cd : cornerDefs) {
            glm::mat4 m = glm::translate(glm::mat4(1), glm::vec3(cd.ox, 0, cd.oz));
            m = glm::rotate(m, glm::radians(cd.rotDeg), glm::vec3(0, 1, 0));
            drawCurve(m);
        }

        glPolygonOffset(0, 0);
        road(false);
        tint(1, 1, 1);

        // --------------------------------------------------
        // BUILDINGS
        // --------------------------------------------------
        for (int i = 0; i < 6; i++) {
            float bx = -10.0f + i * 4.0f, bz = 11.0f;
            float h = 3.5f + (i % 3) * 1.0f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(1, 1, 1);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(bx, h * 0.5f, bz)),
                glm::vec3(2, h, 2)));

            glBindTexture(GL_TEXTURE_2D, roofTex);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(bx, h + 0.12f, bz)),
                glm::vec3(2.05f, 0.24f, 2.05f)));
        }

        for (int i = 0; i < 4; i++) {
            float bx = (i < 2) ? -11.0f : 11.0f;
            float bz = -4.0f + (i % 2) * 8.0f;
            float h = 4.5f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(1, 1, 1);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(bx, h * 0.5f, bz)),
                glm::vec3(2, h, 2)));

            glBindTexture(GL_TEXTURE_2D, roofTex);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(bx, h + 0.12f, bz)),
                glm::vec3(2.05f, 0.24f, 2.05f)));
        }

        // --------------------------------------------------
        // TREES
        // --------------------------------------------------
        for (int i = 0; i < 6; i++) {
            float tx = -10.0f + i * 4.0f, tz = -11.0f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(0.45f, 0.28f, 0.10f);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(tx, 0.6f, tz)),
                glm::vec3(0.35f, 1.2f, 0.35f)));

            glBindTexture(GL_TEXTURE_2D, grassTex);
            tint(0.15f, 0.70f, 0.15f);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(tx, 1.9f, tz)),
                glm::vec3(1.5f, 0.9f, 1.5f)));

            tint(0.10f, 0.55f, 0.10f);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(tx, 2.75f, tz)),
                glm::vec3(1.0f, 0.9f, 1.0f)));

            tint(0.08f, 0.42f, 0.08f);
            drawCube(glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(tx, 3.45f, tz)),
                glm::vec3(0.55f, 0.7f, 0.55f)));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);  glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &cVAO); glDeleteBuffers(1, &cVBO);
    glDeleteProgram(prog);
    glfwTerminate();
    return 0;
}