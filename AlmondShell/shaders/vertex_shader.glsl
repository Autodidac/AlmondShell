(
                #version 330 core
                layout(location = 0) in vec3 aPos; // position from the vertex buffer
                void main() {
                    gl_Position = vec4(aPos, 1.0); // transform vertex position
                }
            )