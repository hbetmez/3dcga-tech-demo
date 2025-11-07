#version 410
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

in vec3 texCoords;

uniform samplerCube skybox;

void main()
{    
    fragColor = texture(skybox, texCoords);
    brightColor = vec4(0.0);
}