#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

float camX = 0.0f, camY = 12.0f, camZ = 18.0f;

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
    FragPos    = vec3(model * vec4(aPos, 1.0));
    TexCoord   = aTex;
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
uniform bool isRoad;   // true  → draw lane markings over the texture
                       // false → plain texture, no markings

vec3 lightPos = vec3(5.0, 10.0, 5.0);

void main() {
    vec3 color = texture(texture1, TexCoord).rgb * tint;

    if (isRoad) {
        float centerDist = abs(TexCoord.y - 0.5);
        float dash       = mod(TexCoord.x * 6.0, 1.0);

        bool edgeLine   = (centerDist > 0.42 && centerDist < 0.48);
        bool centerLine = (centerDist < 0.04 && dash < 0.5);

        if (edgeLine || centerLine)
            color = vec3(1.0);
    }

    vec3  lightDir = normalize(lightPos - FragPos);
    float diff     = max(dot(vec3(0.0, 1.0, 0.0), lightDir), 0.3);
    FragColor = vec4(color * diff, 1.0);
}
)";

// -------------------------------------------------------
// Cube (pos + UV, no normals needed — lighting uses world-up)
// -------------------------------------------------------
float cubeVertices[] = {
    -0.5f,-0.5f,-0.5f, 0.0f,0.0f,  0.5f,-0.5f,-0.5f, 1.0f,0.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f, -0.5f, 0.5f,-0.5f, 0.0f,1.0f, -0.5f,-0.5f,-0.5f, 0.0f,0.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,  0.5f,-0.5f, 0.5f, 1.0f,0.0f,  0.5f, 0.5f, 0.5f, 1.0f,1.0f,
     0.5f, 0.5f, 0.5f, 1.0f,1.0f, -0.5f, 0.5f, 0.5f, 0.0f,1.0f, -0.5f,-0.5f, 0.5f, 0.0f,0.0f,
    -0.5f, 0.5f, 0.5f, 1.0f,0.0f, -0.5f, 0.5f,-0.5f, 1.0f,1.0f, -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f, -0.5f,-0.5f, 0.5f, 0.0f,0.0f, -0.5f, 0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,  0.5f,-0.5f,-0.5f, 0.0f,1.0f,
     0.5f,-0.5f,-0.5f, 0.0f,1.0f,  0.5f,-0.5f, 0.5f, 0.0f,0.0f,  0.5f, 0.5f, 0.5f, 1.0f,0.0f,
    -0.5f, 0.5f,-0.5f, 0.0f,1.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,  0.5f, 0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f, -0.5f, 0.5f, 0.5f, 0.0f,0.0f, -0.5f, 0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f,  0.5f,-0.5f,-0.5f, 1.0f,1.0f,  0.5f,-0.5f, 0.5f, 1.0f,0.0f,
     0.5f,-0.5f, 0.5f, 1.0f,0.0f, -0.5f,-0.5f, 0.5f, 0.0f,0.0f, -0.5f,-0.5f,-0.5f, 0.0f,1.0f
};

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

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Street Circuit", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    // glPolygonOffset lets us draw coplanar surfaces without z-fighting.
    // We enable it globally and toggle the actual offset per draw call.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);

    stbi_set_flip_vertically_on_load(1);

    // --- compile shaders ---
    auto compileShader = [](GLenum type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        return s;
        };
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    // --- geometry ---
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

    // --- textures ---
    unsigned int roadTex = loadTexture("textures/road.jpg");
    unsigned int grassTex = loadTexture("textures/grass.jpg");
    unsigned int buildingTex = loadTexture("textures/building.jpg");
    unsigned int roofTex = loadTexture("textures/roof.jpg");

    // --- uniforms ---
    int uModel = glGetUniformLocation(prog, "model");
    int uView = glGetUniformLocation(prog, "view");
    int uProj = glGetUniformLocation(prog, "projection");
    int uTint = glGetUniformLocation(prog, "tint");
    int uRoad = glGetUniformLocation(prog, "isRoad");

    auto draw = [&](const glm::mat4& m) {
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_TRIANGLES, 0, 36);
        };
    auto tint = [&](float r, float g, float b) { glUniform3f(uTint, r, g, b); };
    auto road = [&](bool v) { glUniform1i(uRoad, v ? 1 : 0); };

    // Road dimensions
    // Straights run between the inner corner edges:
    //   horizontal (Z=±6): X from -5 to +5  → length 10
    //   vertical   (X=±6): Z from -5 to +5  → length 10
    // Corners are 2×2 squares at (±6, ±6).
    const float Y0 = 0.05f;   // ground-layer road Y
    const float SW = 2.0f;    // road width
    const float SL = 10.0f;   // straight length

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(VAO);

        glm::mat4 view = glm::lookAt(
            glm::vec3(camX, camY, camZ), glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj));

        road(false);
        tint(1, 1, 1);

        // --------------------------------------------------
        // GROUND
        // --------------------------------------------------
        // No offset needed — nothing is drawn below it
        glPolygonOffset(0.0f, 0.0f);
        glBindTexture(GL_TEXTURE_2D, grassTex);
        draw(glm::scale(glm::mat4(1.0f), glm::vec3(30.0f, 0.1f, 30.0f)));

        // --------------------------------------------------
        // ROAD STRAIGHTS  (layer 1 — slight offset so they
        //                  sit cleanly above the grass)
        // --------------------------------------------------
        glPolygonOffset(-1.0f, -1.0f);   // pull toward camera
        glBindTexture(GL_TEXTURE_2D, roadTex);
        road(true);

        // Top straight
        draw(glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, Y0, 6.0f)),
            glm::vec3(SL, 0.1f, SW)));

        // Bottom straight
        draw(glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, Y0, -6.0f)),
            glm::vec3(SL, 0.1f, SW)));

        // Left straight
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, Y0, 0.0f));
            m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 1, 0));
            m = glm::scale(m, glm::vec3(SL, 0.1f, SW));
            draw(m);
        }

        // Right straight
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, Y0, 0.0f));
            m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 1, 0));
            m = glm::scale(m, glm::vec3(SL, 0.1f, SW));
            draw(m);
        }

        // --------------------------------------------------
        // ROAD CORNERS  (layer 2 — stronger offset so they
        //                always win over the straights)
        // Plain road texture, no markings.
        // --------------------------------------------------
        glPolygonOffset(-2.0f, -2.0f);   // pulled even further
        road(false);
        // corner centres at (±6, ±6), same Y as straights
        glm::vec2 cpos[4] = { {-6,6},{6,6},{-6,-6},{6,-6} };
        for (auto& c : cpos)
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(c.x, Y0, c.y)),
                glm::vec3(SW, 0.1f, SW)));

        // Reset polygon offset for everything else
        glPolygonOffset(0.0f, 0.0f);

        // --------------------------------------------------
        // BUILDINGS
        // --------------------------------------------------
        for (int i = 0; i < 6; i++) {
            float bx = -10.0f + i * 4.0f, bz = 11.0f;
            float h = 3.5f + (i % 3) * 1.0f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(1, 1, 1);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(bx, h * 0.5f, bz)),
                glm::vec3(2.0f, h, 2.0f)));

            glBindTexture(GL_TEXTURE_2D, roofTex);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(bx, h + 0.12f, bz)),
                glm::vec3(2.05f, 0.24f, 2.05f)));
        }

        for (int i = 0; i < 4; i++) {
            float bx = (i < 2) ? -11.0f : 11.0f;
            float bz = -4.0f + (i % 2) * 8.0f;
            float h = 4.5f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(1, 1, 1);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(bx, h * 0.5f, bz)),
                glm::vec3(2.0f, h, 2.0f)));

            glBindTexture(GL_TEXTURE_2D, roofTex);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(bx, h + 0.12f, bz)),
                glm::vec3(2.05f, 0.24f, 2.05f)));
        }

        // --------------------------------------------------
        // TREES
        // --------------------------------------------------
        for (int i = 0; i < 6; i++) {
            float tx = -10.0f + i * 4.0f, tz = -11.0f;

            glBindTexture(GL_TEXTURE_2D, buildingTex);
            tint(0.45f, 0.28f, 0.10f);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(tx, 0.6f, tz)),
                glm::vec3(0.35f, 1.2f, 0.35f)));

            glBindTexture(GL_TEXTURE_2D, grassTex);
            tint(0.15f, 0.70f, 0.15f);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(tx, 1.9f, tz)),
                glm::vec3(1.5f, 0.9f, 1.5f)));

            tint(0.10f, 0.55f, 0.10f);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(tx, 2.75f, tz)),
                glm::vec3(1.0f, 0.9f, 1.0f)));

            tint(0.08f, 0.42f, 0.08f);
            draw(glm::scale(
                glm::translate(glm::mat4(1.0f), glm::vec3(tx, 3.45f, tz)),
                glm::vec3(0.55f, 0.7f, 0.55f)));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(prog);
    glfwTerminate();
    return 0;
}
