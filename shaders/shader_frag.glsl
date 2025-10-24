#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;
uniform vec3 lightPos;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(- fragPosition + lightPos);
    float strength = max(dot(normalize(fragNormal), lightDir), 0.0);
    
    vec3 lightColor = vec3(1.0);
    vec3 surfaceColor = kd * lightColor * strength;

    if (hasTexCoords)       { fragColor = vec4(texture(colorMap, fragTexCoord).rgb, 1);}
    else if (useMaterial)   { fragColor = vec4(kd, 1);}
    else                    { fragColor = vec4(surfaceColor, 1); } // Output color value, change from (1, 0, 0) to something else
}
