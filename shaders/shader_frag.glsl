#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
    float roughness;
    float metallic;
    float ior;
};

uniform sampler2D colorMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform sampler2D normalMap;
uniform sampler2D bumpMap;

uniform bool usePBRShading;

uniform bool hasTexCoords;
uniform bool hasRoughnessCoords;
uniform bool hasMetallicCoords;
uniform bool hasNormalCoords;
uniform bool hasBumpCoords;
uniform bool useMaterial;

uniform vec3 lightPos;
uniform vec3 cameraPos;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

void main()
{
    /*
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(- fragPosition + lightPos);
    float strength = max(dot(normalize(fragNormal), lightDir), 0.0);
    
    vec3 lightColor = vec3(1.0);
    vec3 newKd;
    if(hasTexCoords) {
        newKd = texture(colorMap, fragTexCoord).rgb;
    }
    else {
        newKd = kd;
    }
    vec3 surfaceColor = newKd * lightColor * strength;

    if (hasTexCoords)       { fragColor = vec4(surfaceColor, 1);}
    else if (useMaterial)   { fragColor = vec4(surfaceColor, 1);}
    else                    { fragColor = vec4(normal, 1); } // Output color value, change from (1, 0, 0) to something else
    */

    // Common vectors for both shading models
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPosition);
    vec3 V = normalize(cameraPos - fragPosition); // Assuming camera at origin in view space
    vec3 H = normalize(L + V);
    
    vec3 lightColor = vec3(1.0);
    float pi = 3.14159265359;
    
    // Sample textures
    vec3 baseColor;
    if (hasTexCoords && !useMaterial) {
        baseColor = texture(colorMap, fragTexCoord).rgb;
    } else {
        baseColor = kd;
    }
    
    // Sample roughness
    float rough;
    if (hasRoughnessCoords && !useMaterial) {
        rough = texture(roughnessMap, fragTexCoord).r;
    } else {
        rough = roughness;
    }
    
    // Sample metallic
    float metal;
    if (hasMetallicCoords && !useMaterial) {
        metal = texture(metallicMap, fragTexCoord).r;
    } else {
        metal = metallic;
    }
    
    // Sample normal map
    if (hasNormalCoords) {
        vec3 normalFromMap = texture(normalMap, fragTexCoord).rgb;
        normalFromMap = normalFromMap * 2.0 - 1.0;
        // transform this
    }
    
    // Show normals in default case
    vec3 finalColor = N;
    
    if (!usePBRShading) {
        // Simple Blinn-Phong Shading
        float NdotL = max(dot(N, L), 0.0);
        
        // Diffuse
        vec3 diffuse = baseColor * lightColor * NdotL;
        
        // Specular
        float NdotH = max(dot(N, H), 0.0);
        vec3 specular = ks * lightColor * pow(NdotH, shininess);
        
        finalColor = diffuse + specular;
        
    } else {
        // === PBR Shading GGX/Smith ===
        
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);
                        
        // Normal distribution function GGX: DGGX(m) eqn 33 in walter et al.
        float chi = (rough > 0.0) ? 1.0 : 0.0;
        float alpha = rough * rough;
        float alpha2 = alpha * alpha;
        float cosTheta2 = NdotH * NdotH;
        float cosTheta4 = cosTheta2 * cosTheta2;
        float tanTheta2 = (1 - cosTheta2) / cosTheta2;
        float aPlusTan = alpha2 + tanTheta2;

        float D = (alpha2 * chi * NdotH) / (pi * cosTheta4 * aPlusTan * aPlusTan);
        
        // Geometric shadowing GGX: GGGX(v) eqn 34 in walter et al.
        float vm_over_vn = VdotH / NdotV;
        float cosThetaV2 = NdotV * NdotV;
        float tanThetaV2 = (1 - cosThetaV2) / cosThetaV2;

        float denom = 1 + sqrt(1 + (alpha2 * tanThetaV2));
        float G = chi * vm_over_vn * (2 / denom);
        
        // Fresnel
        // Calculate F0 (fresnel reflectance at normal incidence)
        //vec3 F0 = mix(vec3(0.04), baseColor, metal);
        float n = ior;
        float sqrtF0 = (1 - n) / (1 + n);
        vec3 F0 = vec3(sqrtF0 * sqrtF0); // or use mix function? what to do for metals?

        vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotL, 5.0); //NdotL is already clamped
        
        // Overall GGX/Smith 
        vec3 fr = (D * G * F) / max(4.0 * NdotL * NdotV, 0.001); //avoid div by zero
        
        finalColor = fr;
        
    }
    
    fragColor = vec4(finalColor, 1.0);

}
