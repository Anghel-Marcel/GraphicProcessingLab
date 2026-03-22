#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Window
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

// Camera
float camX = 0.0f, camY = 8.0f, camZ = 15.0f;

// Shader sources
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoord = aTex;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D texture1;

void main() {
    FragColor = texture(texture1, TexCoord);
}
)";

// FULL cube vertices (FIXED)
float cubeVertices[] = {
    // positions          // texcoords
    -0.5f,-0.5f,-0.5f, 0.0f,0.0f,
     0.5f,-0.5f,-0.5f, 1.0f,0.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f,
    -0.5f, 0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,0.0f,

    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,
     0.5f,-0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,1.0f,
     0.5f, 0.5f, 0.5f, 1.0f,1.0f,
    -0.5f, 0.5f, 0.5f, 0.0f,1.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,

    -0.5f, 0.5f, 0.5f, 1.0f,0.0f,
    -0.5f, 0.5f,-0.5f, 1.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,
    -0.5f, 0.5f, 0.5f, 1.0f,0.0f,

     0.5f, 0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f,-0.5f,-0.5f, 0.0f,1.0f,
     0.5f,-0.5f,-0.5f, 0.0f,1.0f,
     0.5f,-0.5f, 0.5f, 0.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,

    -0.5f, 0.5f,-0.5f, 0.0f,1.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,
    -0.5f, 0.5f, 0.5f, 0.0f,0.0f,
    -0.5f, 0.5f,-0.5f, 0.0f,1.0f,

    -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
     0.5f,-0.5f,-0.5f, 1.0f,1.0f,
     0.5f,-0.5f, 0.5f, 1.0f,0.0f,
     0.5f,-0.5f, 0.5f, 1.0f,0.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f
};

// Texture loader
unsigned int loadTexture(const char* path) {
    unsigned int texture;
    glGenTextures(1, &texture);

    int w, h, nrChannels;
    unsigned char* data = stbi_load(path, &w, &h, &nrChannels, 0);

    GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texture;
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Street Circuit", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f); // sky color

    // Shaders
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    // VAO/VBO
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

    // Textures
    unsigned int roadTex = loadTexture("textures/road.jpg");
    unsigned int grassTex = loadTexture("textures/grass.jpg");
    unsigned int buildingTex = loadTexture("textures/building.jpg");

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(glm::vec3(camX, camY, camZ),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            (float)SCR_WIDTH / SCR_HEIGHT,
            0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);

        // GRASS
        glBindTexture(GL_TEXTURE_2D, grassTex);
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(20, 0.1f, 20));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ROAD (fixed)
        glBindTexture(GL_TEXTURE_2D, roadTex);

        glm::mat4 roads[4] = {
            glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0,0.05f,5)), glm::vec3(12,0.1f,1)),
            glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0,0.05f,-5)), glm::vec3(12,0.1f,1)),
            glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-5,0.05f,0)), glm::vec3(1,0.1f,12)),
            glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(5,0.05f,0)), glm::vec3(1,0.1f,12))
        };

        for (int i = 0; i < 4; i++) {
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(roads[i]));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // BUILDINGS
        glBindTexture(GL_TEXTURE_2D, buildingTex);
        float heights[] = { 2.0f, 3.5f, 2.5f, 4.0f, 3.0f };

        for (int i = 0; i < 5; i++) {
            model = glm::translate(glm::mat4(1.0f), glm::vec3(-8 + i * 4, heights[i] / 2, 8));
            model = glm::scale(model, glm::vec3(1.5f, heights[i], 1.5f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // SIMPLE TREES (cube only)
        for (int i = 0; i < 5; i++) {
            // trunk
            glBindTexture(GL_TEXTURE_2D, buildingTex);
            model = glm::translate(glm::mat4(1.0f), glm::vec3(-8 + i * 4, 0.5f, -8));
            model = glm::scale(model, glm::vec3(0.3f, 1.0f, 0.3f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // leaves
            glBindTexture(GL_TEXTURE_2D, grassTex);
            model = glm::translate(glm::mat4(1.0f), glm::vec3(-8 + i * 4, 1.5f, -8));
            model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}