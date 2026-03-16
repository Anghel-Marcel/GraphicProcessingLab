#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// -------------------- Ground --------------------

float groundVertices[] = {

    -5.0f,0.0f,-5.0f,   0.0f,0.0f,
     5.0f,0.0f,-5.0f,  10.0f,0.0f,
     5.0f,0.0f, 5.0f,  10.0f,10.0f,

     5.0f,0.0f, 5.0f,  10.0f,10.0f,
    -5.0f,0.0f, 5.0f,   0.0f,10.0f,
    -5.0f,0.0f,-5.0f,   0.0f,0.0f
};

// -------------------- Terrain --------------------

float terrainVertices[] = {

    // Section 1
    -1.0f,0.0f,-1.5f, 0.0f,0.0f,
    -0.5f,0.3f,-1.3f, 0.5f,1.0f,
     0.0f,0.0f,-1.5f, 1.0f,0.0f,

     0.0f,0.0f,-1.5f, 0.0f,0.0f,
    -0.5f,0.3f,-1.3f, 0.5f,1.0f,
     0.7f,0.6f,-1.1f, 1.0f,1.0f,


     // Section 2
      0.0f,0.0f,-1.5f, 0.0f,0.0f,
      0.7f,0.6f,-1.1f, 0.5f,1.0f,
      1.4f,0.0f,-1.4f, 1.0f,0.0f,

      1.4f,0.0f,-1.4f, 0.0f,0.0f,
      0.7f,0.6f,-1.1f, 0.5f,1.0f,
      2.0f,0.9f,-0.9f, 1.0f,1.0f,


      // Section 3
       1.4f,0.0f,-1.4f, 0.0f,0.0f,
       2.0f,0.9f,-0.9f, 0.5f,1.0f,
       2.8f,0.0f,-1.3f, 1.0f,0.0f,

       2.8f,0.0f,-1.3f, 0.0f,0.0f,
       2.0f,0.9f,-0.9f, 0.5f,1.0f,
       3.6f,1.2f,-0.7f, 1.0f,1.0f,


       // Section 4 
        2.8f,0.0f,-1.3f, 0.0f,0.0f,
        3.6f,1.2f,-0.7f, 0.5f,1.0f,
        4.5f,0.0f,-1.2f, 1.0f,0.0f,

        4.5f,0.0f,-1.2f, 0.0f,0.0f,
        3.6f,1.2f,-0.7f, 0.5f,1.0f,
        5.0f,1.4f,-0.5f, 1.0f,1.0f
};

// -------------------- Skybox --------------------

float skyboxVertices[] = {

    // Back wall
    -10.0f,-1.0f,-10.0f, 0.0f,0.0f,
     10.0f,-1.0f,-10.0f, 1.0f,0.0f,
     10.0f,10.0f,-10.0f, 1.0f,1.0f,

     10.0f,10.0f,-10.0f, 1.0f,1.0f,
    -10.0f,10.0f,-10.0f, 0.0f,1.0f,
    -10.0f,-1.0f,-10.0f, 0.0f,0.0f,

    // Left wall
    -10.0f,-1.0f,10.0f, 0.0f,0.0f,
    -10.0f,-1.0f,-10.0f,1.0f,0.0f,
    -10.0f,10.0f,-10.0f,1.0f,1.0f,

    -10.0f,10.0f,-10.0f,1.0f,1.0f,
    -10.0f,10.0f,10.0f, 0.0f,1.0f,
    -10.0f,-1.0f,10.0f, 0.0f,0.0f,

    // Right wall
     10.0f,-1.0f,-10.0f,0.0f,0.0f,
     10.0f,-1.0f,10.0f, 1.0f,0.0f,
     10.0f,10.0f,10.0f, 1.0f,1.0f,

     10.0f,10.0f,10.0f, 1.0f,1.0f,
     10.0f,10.0f,-10.0f,0.0f,1.0f,
     10.0f,-1.0f,-10.0f,0.0f,0.0f,

     // Front wall
     -10.0f,-1.0f,10.0f,0.0f,0.0f,
      10.0f,-1.0f,10.0f,1.0f,0.0f,
      10.0f,10.0f,10.0f,1.0f,1.0f,

      10.0f,10.0f,10.0f,1.0f,1.0f,
     -10.0f,10.0f,10.0f,0.0f,1.0f,
     -10.0f,-1.0f,10.0f,0.0f,0.0f
};

// -------------------- Shaders --------------------

const char* vertexShaderSource = R"(

#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * vec4(aPos,1.0);
    TexCoord = aTex;
}
)";

const char* fragmentShaderSource = R"(

#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;

void main()
{
    FragColor = texture(texture1, TexCoord);
}
)";

// -------------------- Texture Loader --------------------

unsigned int loadTexture(const char* path)
{
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, channels;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (data)
    {
        GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
        std::cout << "Texture failed: " << path << std::endl;

    stbi_image_free(data);

    return texture;
}

// -------------------- MAIN --------------------

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(800, 600, "Scene Cube", NULL, NULL);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    stbi_set_flip_vertically_on_load(true);

    // Shaders
    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vShader);

    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vShader);
    glAttachShader(shaderProgram, fShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vShader);
    glDeleteShader(fShader);

    // ---------------- VAOs ----------------

    unsigned int groundVAO, groundVBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int terrainVAO, terrainVBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);

    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(terrainVertices), terrainVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int skyVAO, skyVBO;
    glGenVertexArrays(1, &skyVAO);
    glGenBuffers(1, &skyVBO);

    glBindVertexArray(skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Textures
    unsigned int grassTex = loadTexture("textures/grass.jpg");
    unsigned int skyTex = loadTexture("textures/sky.jpg");


    // ---------------- Render Loop ----------------

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.8f, 6.0f),   
            glm::vec3(0.0f, 0.8f, -5.0f),  
            glm::vec3(0.0f, 1.0f, 0.0f)    
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            800.0f / 600.0f,
            0.1f,
            100.0f
        );

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // SKY
        glBindTexture(GL_TEXTURE_2D, skyTex);
        glBindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        // GROUND
        glBindTexture(GL_TEXTURE_2D, grassTex);
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // TERRAIN
        glBindTexture(GL_TEXTURE_2D, grassTex);
        glBindVertexArray(terrainVAO);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}