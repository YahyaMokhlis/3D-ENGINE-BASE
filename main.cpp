#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

// -------------------- Error Handling --------------------
void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, 
                            GLsizei length, const GLchar* message, const void* userParam) {
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        std::cerr << "[OpenGL Error] " << message << std::endl;
    }
}

// -------------------- Math Types --------------------
using Vector3 = glm::vec3;
using Vector4 = glm::vec4;
using Matrix4 = glm::mat4;

// -------------------- Scene Object --------------------
struct SceneObject {
    std::string name;
    Vector3 position{0.0f};
    Vector3 rotation{0.0f};
    Vector3 scale{1.0f};
    
    SceneObject(const std::string& name, const Vector3& pos = Vector3(0.0f))
        : name(name), position(pos) {}
    
    Matrix4 getTransform() const {
        Matrix4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = glm::rotate(transform, glm::radians(rotation.y), Vector3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(rotation.x), Vector3(1, 0, 0));
        transform = glm::rotate(transform, glm::radians(rotation.z), Vector3(0, 0, 1));
        transform = glm::scale(transform, scale);
        return transform;
    }
};

// -------------------- Shader Program --------------------
class ShaderProgram {
private:
    unsigned int id;
    
    static unsigned int compileShader(GLenum type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation error: " << infoLog << std::endl;
        }
        return shader;
    }
    
public:
    ShaderProgram(const char* vertexSource, const char* fragmentSource) {
        unsigned int vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
        unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        
        id = glCreateProgram();
        glAttachShader(id, vertex);
        glAttachShader(id, fragment);
        glLinkProgram(id);
        
        int success;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(id, 512, nullptr, infoLog);
            std::cerr << "Shader linking error: " << infoLog << std::endl;
        }
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    
    ~ShaderProgram() {
        glDeleteProgram(id);
    }
    
    void use() const { glUseProgram(id); }
    unsigned int getId() const { return id; }
    
    void setMatrix4(const std::string& name, const Matrix4& matrix) const {
        glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(matrix));
    }
};

// -------------------- Camera --------------------
class Camera {
private:
    Vector3 position;
    Vector3 front;
    Vector3 up;
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 5.0f;
    float sensitivity = 0.1f;
    float fov = 45.0f;
    
public:
    Camera(const Vector3& pos = Vector3(0.0f, 5.0f, 10.0f))
        : position(pos), front(0.0f, 0.0f, -1.0f), up(0.0f, 1.0f, 0.0f) {}
    
    Matrix4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }
    
    Matrix4 getProjectionMatrix(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
    }
    
    void processKeyboard(int key, float deltaTime) {
        float velocity = speed * deltaTime;
        if (key == GLFW_KEY_W) position += velocity * front;
        if (key == GLFW_KEY_S) position -= velocity * front;
        if (key == GLFW_KEY_A) position -= glm::normalize(glm::cross(front, up)) * velocity;
        if (key == GLFW_KEY_D) position += glm::normalize(glm::cross(front, up)) * velocity;
    }
    
    void processMouse(float xoffset, float yoffset) {
        yaw += xoffset * sensitivity;
        pitch -= yoffset * sensitivity;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        
        Vector3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(direction);
    }
};

// -------------------- Renderer --------------------
class Renderer {
private:
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    std::unique_ptr<ShaderProgram> shader;
    
    const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";
    
    const char* fragmentSource = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 color;
        void main() {
            FragColor = vec4(color, 1.0);
        }
    )";
    
public:
    Renderer() {
        setupCubeMesh();
        shader = std::make_unique<ShaderProgram>(vertexSource, fragmentSource);
    }
    
    ~Renderer() {
        glDeleteVertexArrays(1, &cubeVAO);
        glDeleteBuffers(1, &cubeVBO);
    }
    
    void setupCubeMesh() {
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
        };
        
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    
    void drawGrid(int size = 10) const {
        glBegin(GL_LINES);
        glColor3f(0.3f, 0.3f, 0.3f);
        for (int i = -size; i <= size; ++i) {
            glVertex3f(i, 0, -size);
            glVertex3f(i, 0, size);
            glVertex3f(-size, 0, i);
            glVertex3f(size, 0, i);
        }
        glEnd();
    }
    
    void drawGizmo(const Vector3& position, float length = 1.0f) const {
        glBegin(GL_LINES);
        glLineWidth(2.0f);
        
        // X axis (red)
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(position.x, position.y, position.z);
        glVertex3f(position.x + length, position.y, position.z);
        
        // Y axis (green)
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(position.x, position.y, position.z);
        glVertex3f(position.x, position.y + length, position.z);
        
        // Z axis (blue)
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(position.x, position.y, position.z);
        glVertex3f(position.x, position.y, position.z + length);
        
        glEnd();
        glLineWidth(1.0f);
    }
    
    void drawCube(const SceneObject& obj, const Vector3& color) const {
        shader->use();
        shader->setMatrix4("model", obj.getTransform());
        glUniform3fv(glGetUniformLocation(shader->getId(), "color"), 1, glm::value_ptr(color));
        
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
};

// -------------------- Application --------------------
class Application {
private:
    GLFWwindow* window;
    Camera camera;
    Renderer renderer;
    std::vector<SceneObject> sceneObjects;
    int selectedObjectIndex = -1;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    bool initializeGLFW() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        
        return true;
    }
    
    bool createWindow(int width, int height, const char* title) {
        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
            glViewport(0, 0, w, h);
        });
        
        return true;
    }
    
    bool initializeGLAD() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }
        return true;
    }
    
    void setupDebugCallback() {
        int flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
    }
    
    void initializeImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
    
    void processInput() {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_W, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_S, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_A, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_D, deltaTime);
    }
    
    void renderScene() {
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Update shader matrices
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        
        Matrix4 view = camera.getViewMatrix();
        Matrix4 projection = camera.getProjectionMatrix(aspect);
        
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(projection));
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(glm::value_ptr(view));
        
        // Draw grid
        renderer.drawGrid(10);
        
        // Draw objects
        for (size_t i = 0; i < sceneObjects.size(); ++i) {
            Vector3 color = (i == selectedObjectIndex) ? Vector3(1.0f, 0.8f, 0.2f) : Vector3(0.4f, 0.7f, 1.0f);
            renderer.drawCube(sceneObjects[i], color);
        }
        
        // Draw gizmo for selected object
        if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneObjects.size()) {
            renderer.drawGizmo(sceneObjects[selectedObjectIndex].position);
        }
    }
    
    void renderUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Hierarchy panel
        ImGui::Begin("Hierarchy");
        for (size_t i = 0; i < sceneObjects.size(); ++i) {
            bool isSelected = (selectedObjectIndex == static_cast<int>(i));
            if (ImGui::Selectable(sceneObjects[i].name.c_str(), isSelected)) {
                selectedObjectIndex = static_cast<int>(i);
            }
        }
        
        if (ImGui::Button("Add Cube")) {
            std::string name = "Cube " + std::to_string(sceneObjects.size());
            sceneObjects.emplace_back(name);
        }
        ImGui::End();
        
        // Inspector panel
        ImGui::Begin("Inspector");
        if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneObjects.size()) {
            auto& obj = sceneObjects[selectedObjectIndex];
            
            char buffer[256];
            strncpy(buffer, obj.name.c_str(), sizeof(buffer));
            if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                obj.name = buffer;
            }
            
            ImGui::Separator();
            ImGui::DragFloat3("Position", glm::value_ptr(obj.position), 0.1f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(obj.rotation), 1.0f);
            ImGui::DragFloat3("Scale", glm::value_ptr(obj.scale), 0.1f, 0.1f, 10.0f);
            
            ImGui::Separator();
            if (ImGui::Button("Delete")) {
                sceneObjects.erase(sceneObjects.begin() + selectedObjectIndex);
                selectedObjectIndex = -1;
            }
        }
        ImGui::End();
        
        // Statistics panel
        ImGui::Begin("Statistics");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Objects: %zu", sceneObjects.size());
        ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)", 
                    camera.getViewMatrix()[3][0], 
                    camera.getViewMatrix()[3][1], 
                    camera.getViewMatrix()[3][2]);
        ImGui::End();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    
public:
    ~Application() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    bool initialize(int width = 1280, int height = 720, const char* title = "3D Editor") {
        if (!initializeGLFW()) return false;
        if (!createWindow(width, height, title)) return false;
        if (!initializeGLAD()) return false;
        
        setupDebugCallback();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        
        initializeImGui();
        
        // Create default objects
        sceneObjects.emplace_back("Cube 0", Vector3(0, 0, 0));
        sceneObjects.emplace_back("Cube 1", Vector3(2, 0, 0));
        sceneObjects.emplace_back("Cube 2", Vector3(-2, 0, 0));
        
        return true;
    }
    
    void run() {
        while (!glfwWindowShouldClose(window)) {
            processInput();
            renderScene();
            renderUI();
            
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
};

// -------------------- Entry Point --------------------
int main() {
    try {
        Application app;
        if (app.initialize()) {
            app.run();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
