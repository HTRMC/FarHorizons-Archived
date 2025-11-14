#version 460

// Vertex attributes from buffer
layout(location = 0) in vec2 inPosition;  // NDC position (-1 to 1)
layout(location = 1) in vec4 inColor;     // Color (white for crosshair)

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

void main() {
    // Position is already in NDC space, so no transformation needed
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
