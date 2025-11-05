#version 410

// Global variables for lighting calculations
//uniform vec3 viewPos;
uniform sampler2D texShadow;

uniform mat4 lightMVP;
uniform int peelingMode = 0;

// Output for on-screen color
out vec4 outColor;

// Interpolated output data from vertex shader
in vec3 fragPos; // World-space position
in vec3 fragNormal; // World-space normal

void main() {
    // Output the normal as color

    vec4 fragLightCoord = lightMVP * vec4(fragPos, 1.0);

    // Depth of the fragment with respect to the light
    float fragLightDepth = fragLightCoord.z;

    // Shadow map coordinate corresponding to this fragment
    vec2 shadowMapCoord = fragLightCoord.xy;

    // Shadow map value from the corresponding shadow map position
    float shadowMapDepth = texture(texShadow, shadowMapCoord).x;


    //PEELING
    if (peelingMode == 1 && 
        distance(fragLightCoord.xy, vec2(0.5)) <= 0.3 && 
        abs(shadowMapDepth - fragLightDepth) < 0.0001 ) 
    {
        discard;
    }

    outColor = vec4(abs(fragNormal), 1.0);
}