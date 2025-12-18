#include <glad/glad.h>
#include <glm.hpp>
#include <GLFW/glfw3.h>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

struct Cube{
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    Cube(glm::vec3 pos): position(pos), rotation(0.0f), scale(1.0f,1.0f,1.0f){}
};

std::vector<Cube> cubes;
int selectedCube = -1;

// Camera
glm::vec3 cameraPos   = glm::vec3(0.0f, 5.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    glViewport(0,0,width,height);
}

// WASD camera
void processInput(GLFWwindow* window){
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    float speed = 5.0f * deltaTime;

    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront,cameraUp))*speed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront,cameraUp))*speed;
}

// Grid
void drawGrid(int size=10){
    glBegin(GL_LINES);
    glColor3f(0.6f,0.6f,0.6f);
    for(int i=-size;i<=size;i++){
        glVertex3f(i,0,-size);
        glVertex3f(i,0,size);
        glVertex3f(-size,0,i);
        glVertex3f(size,0,i);
    }
    glEnd();
}

// Gizmo
void drawGizmo(glm::vec3 pos,float len=1.0f){
    glBegin(GL_LINES);
    // X axis red
    glColor3f(1,0,0); glVertex3f(pos.x,pos.y,pos.z); glVertex3f(pos.x+len,pos.y,pos.z);
    // Y axis green
    glColor3f(0,1,0); glVertex3f(pos.x,pos.y,pos.z); glVertex3f(pos.x,pos.y+len,pos.z);
    // Z axis blue
    glColor3f(0,0,1); glVertex3f(pos.x,pos.y,pos.z); glVertex3f(pos.x,pos.y,pos.z+len);
    glEnd();
}

// Cube vertices
float cubeVertices[] = {
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,
     0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f,

    -0.5f,-0.5f,0.5f,   0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,
     0.5f,0.5f,0.5f,  -0.5f,0.5f,0.5f,   -0.5f,-0.5f,0.5f,

    -0.5f,0.5f,0.5f,   -0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f,  -0.5f,0.5f,0.5f,

     0.5f,0.5f,0.5f,    0.5f,0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
     0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,

    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,
     0.5f,-0.5f,0.5f,  -0.5f,-0.5f,0.5f,  -0.5f,-0.5f,-0.5f,

    -0.5f,0.5f,-0.5f,   0.5f,0.5f,-0.5f,   0.5f,0.5f,0.5f,
     0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,  -0.5f,0.5f,-0.5f
};

int main(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280,720,"Mini 3D Editor",NULL,NULL);
    if(!window){ std::cout<<"Failed to create window\n"; glfwTerminate(); return -1;}
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cout<<"Failed to init GLAD\n"; return -1;}

    glEnable(GL_DEPTH_TEST);

    // VAO/VBO
    unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVertices),cubeVertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // Shader
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main(){ gl_Position = projection * view * model * vec4(aPos,1.0); }
    )";
    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        void main(){ FragColor = vec4(0.4,0.7,1.0,1.0); }
    )";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader,1,&vertexShaderSource,NULL);
    glCompileShader(vertexShader);
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader,1,&fragmentShaderSource,NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram,vertexShader);
    glAttachShader(shaderProgram,fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window,true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Cubes
    cubes.push_back(Cube(glm::vec3(0,0,0)));
    cubes.push_back(Cube(glm::vec3(2,0,0)));
    cubes.push_back(Cube(glm::vec3(-2,0,0)));

    while(!glfwWindowShouldClose(window)){
        processInput(window);
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),1280.0f/720.0f,0.1f,100.0f);
        glm::mat4 view = glm::lookAt(cameraPos,cameraPos+cameraFront,cameraUp);
        unsigned int viewLoc = glGetUniformLocation(shaderProgram,"view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram,"projection");
        glUniformMatrix4fv(viewLoc,1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(projLoc,1,GL_FALSE,glm::value_ptr(projection));

        // Draw grid
        drawGrid(10);

        // Draw cubes
        for(int i=0;i<cubes.size();i++){
            Cube &c = cubes[i];
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model,c.position);
            model = glm::rotate(model,glm::radians(c.rotation.y),glm::vec3(0,1,0));
            model = glm::scale(model,c.scale);
            unsigned int modelLoc = glGetUniformLocation(shaderProgram,"model");
            glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(model));
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES,0,36);
        }

        // Draw gizmo for selected cube
        if(selectedCube>=0) drawGizmo(cubes[selectedCube].position);

        // ImGui start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Hierarchy panel
        ImGui::Begin("Hierarchy");
        for(int i=0;i<cubes.size();i++){
            if(ImGui::Selectable(("Cube "+std::to_string(i)).c_str(),selectedCube==i)){
                selectedCube=i;
            }
        }
        ImGui::End();

        // Inspector panel
        ImGui::Begin("Inspector");
        if(selectedCube>=0){
            ImGui::DragFloat3("Position",glm::value_ptr(cubes[selectedCube].position),0.1f);
            ImGui::DragFloat3("Rotation",glm::value_ptr(cubes[selectedCube].rotation),1.0f);
            ImGui::DragFloat3("Scale",glm::value_ptr(cubes[selectedCube].scale),0.1f,0.1f,10.0f);
        }
        ImGui::End();

        // ImGui render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
