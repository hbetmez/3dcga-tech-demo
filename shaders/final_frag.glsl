#version 410

out vec4 fragColor;
in vec2 texCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform bool useBloom;
uniform float exposure;

void main()
{             
    vec3 originalColor = texture(scene, texCoords).rgb;      
    vec3 bloomColor = texture(bloomBlur, texCoords).rgb;
    
    if(useBloom)
        originalColor += bloomColor;
    
    // tone mapping
    vec3 result = vec3(1.0) - exp(-originalColor * exposure);
    
    // gamma correction
    const float gamma = 2.2;
    result = pow(result, vec3(1.0 / gamma));
    
    fragColor = vec4(result, 1.0);
}