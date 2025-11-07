#version 410
#define MAX_LIGHTS 10
#define PI 3.14159265359

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

// ** for shading **
uniform sampler2D colorMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform sampler2D normalMap;
uniform sampler2D kaMap;
uniform sampler2D emissiveMap;

uniform bool usePBRShading;
uniform bool useAOTex;
uniform bool useEmissive;

uniform bool useNormalMapping;

uniform bool hasTexCoords;
uniform bool hasRoughnessCoords;
uniform bool hasMetallicCoords;
uniform bool hasNormalCoords;
uniform bool hasKaCoords;
uniform bool hasEmissiveCoords;
uniform bool useMaterial;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;

uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];

// ** for shadows **
uniform bool useShadows;
uniform bool usePCF;
uniform int PCFWindowSize;
uniform sampler2D texShadow[MAX_LIGHTS]; 
uniform mat4 lightMVPs[MAX_LIGHTS];

// ** for environment map
uniform samplerCube environmentMap;
uniform bool useEnvironmentMapping;

// ** for bloom
uniform float bloomThreshold;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in mat3 fragTBN;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 bloomColor;

float DGGX(vec3 N, vec3 H, float roughness); //d_ggx trouwbridge-reitz 
float GGGX(vec3 N, vec3 V, vec3 H, float roughness); //g_ggx schlick
float SmithGGX(vec3 N, vec3 V, vec3 L, float roughness); // approximation of smith
vec3 frSchlick(float cosTheta, vec3 F0); //schlick approximation for fresnel

float DGGX(vec3 N, vec3 H, float roughness) 
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
	
    float numerator = a2;
    float denominator = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
	
    return numerator / denominator;

    // dggx as walter et al paper
    /*float chi = (roughness > 0.0) ? 1.0 : 0.0;
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH = max(dot(N, H), 0.0);
    float cosTheta2 = NdotH * NdotH;
    float cosTheta4 = cosTheta2 * cosTheta2;
    float tanTheta2 = (1 - cosTheta2) / cosTheta2;
    float aPlusTan = alpha2 + tanTheta2;

    float D = (alpha2 * chi * NdotH) / (PI * cosTheta4 * aPlusTan * aPlusTan);
    return D;*/
}

float GGGX(vec3 N, vec3 V, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float NdotV = max(dot(N, V), 0.0);
    float numearator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
	
    return numearator / denominator;

    //gggx as walter et al paper
    /*float VdotH = max(dot(V, H), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float vm_over_vn = VdotH / NdotV;
    float cosThetaV2 = NdotV * NdotV;
    float tanThetaV2 = (1 - cosThetaV2) / cosThetaV2;
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float chi = (roughness > 0.0) ? 1.0 : 0.0;

    float denom = 1 + sqrt(1 + (alpha2 * tanThetaV2));
    float G = chi * vm_over_vn * (2 / denom);
    return G;*/
}

float GSmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GGGX(N, V, roughness);
    float ggx1  = GGGX(N, L, roughness);
	
    return ggx1 * ggx2;
}

vec3 frSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // Common vectors for both shading models
    vec3 N = normalize(fragNormal);
    // ** for normal mapping
    if (hasNormalCoords && useNormalMapping) {
        vec3 mapNormal = texture(normalMap, fragTexCoord).rgb;
        // transform from 0/1 range to -1/1
        mapNormal = mapNormal * 2.0 - 1.0;
        // Transform from tangent space to world space
        N = normalize(fragTBN * mapNormal);
    }

    vec3 oldN = N; // for shadows

    // L defined per light below
    vec3 V = normalize(cameraPos - fragPosition);
    // H defined per light below

    // *** environment mapping
    vec3 I = normalize(fragPosition - cameraPos);
    vec3 R = reflect(I, normalize(fragNormal));
    // Sample from cuAbemap
    vec3 reflection = texture(environmentMap, R).rgb;
        
    vec3 baseColor; // Sample textures
    if (hasTexCoords && !useMaterial) baseColor = texture(colorMap, fragTexCoord).rgb; else baseColor = kd;
   
    float rough; // Sample roughness
    if (hasRoughnessCoords && !useMaterial) rough = texture(roughnessMap, fragTexCoord).r; else rough = roughness;
    
    float metal; // Sample metallic
    if (hasMetallicCoords && !useMaterial) metal = texture(metallicMap, fragTexCoord).r; else metal = metallic;

    float ka;
    if (hasKaCoords && !useMaterial) ka = texture(kaMap, fragTexCoord).r; else ka = 1.0;

    vec3 ke;
    if (hasEmissiveCoords) ke = texture(emissiveMap, fragTexCoord).rgb; else ke = vec3(0.0);

    // Sample normal map
    if (hasNormalCoords) {
        vec3 normalFromMap = texture(normalMap, fragTexCoord).rgb;
        normalFromMap = normalFromMap * 2.0 - 1.0;
        // transform this
    }
    
    vec3 finalColor = vec3(0.0f);

    for (int i = 0; i < numLights; i++) {
        vec3 L = normalize(lightPositions[i] - fragPosition);
        vec3 H = normalize(L + V);

         // ** shadows
        float shadow = 1.0; 

        if(useShadows) {
            vec4 fragLightCoord = lightMVPs[i] * vec4(fragPosition, 1.0);

            // Divide by w because fragLightCoord are homogeneous coordinates
            fragLightCoord.xyz /= fragLightCoord.w;

            // The resulting value is in NDC space (-1 to +1),
            //  we transform them to texture space (0 to 1).
            fragLightCoord.xyz = fragLightCoord.xyz * 0.5 + 0.5;

            //if we are not in light frustum all is in shadow
            if (fragLightCoord.x < 0.0 || fragLightCoord.x > 1.0 || 
                fragLightCoord.y < 0.0 || fragLightCoord.y > 1.0 ||
                fragLightCoord.z < 0.0 || fragLightCoord.z > 1.0) {
                shadow = 0.0; 
            }
            // we are in the light frustum
            else {
                // dimming at angles away from lights (to look smooth as we get away from the light direction)
                vec2 center = vec2(0.5, 0.5);
                vec2 toEdge = abs(fragLightCoord.xy - center);  // Distance to center in each axis
    
                float edgeDist = length(toEdge);  // (circular falloff)
    
                // normalise
                float dist = 1.0 - (edgeDist / 0.5);
                dist = clamp(dist, 0.0, 1.0);
                dist = pow(dist, 0.5);  // Adjust exponent for falloff sharpness

                if(usePCF) {
                    // also using pcf
                    vec2 texelSize = 1.0 / textureSize(texShadow[i], 0);

                    int index = (PCFWindowSize - 1) / 2; //pcf size should be odd number

                    for (int k = -index; k <= index; k+=1) {
                        for (int j = -index; j <= index; j+=1) {
                            // Depth of the fragment with respect to the light
                            float fragLightDepth = fragLightCoord.z;

                            // Shadow map coordinate corresponding to this fragment
                            vec2 shadowMapCoord = fragLightCoord.xy + vec2(k, j) * texelSize;

                            // Shadow map value from the corresponding shadow map position
                            float shadowMapDepth = texture(texShadow[i], shadowMapCoord).x;

                            // high bias if almost parallel, low bias if perpendicular
                            float bias = max(0.005 * (1.0 - dot(oldN, L)), 0.001);
                            shadow += fragLightDepth - bias > shadowMapDepth ? 0.0 : 1.0;
                        }
                    }
        
                    shadow /= (PCFWindowSize * PCFWindowSize);
                }
                else {
                    float fragLightDepth = fragLightCoord.z;
                    vec2 shadowMapCoord = fragLightCoord.xy;
                    float shadowMapDepth = texture(texShadow[i], shadowMapCoord).x;
                    float bias = max(0.005 * (1.0 - dot(oldN, L)), 0.001);
                    shadow = fragLightDepth - bias > shadowMapDepth ? 0.0 : 1.0;
                }
                shadow *= dist;
            }
        }
        // *** shadow end

        if (!usePBRShading) {
            // Simple Blinn-Phong Shading
            float NdotL = max(dot(N, L), 0.0);
        
            // Diffuse
            vec3 diffuse = baseColor * lightColors[i] * NdotL;
        
            // Specular
            float NdotH = max(dot(N, H), 0.0);
            vec3 specular = ks * lightColors[i] * pow(NdotH, shininess);
            
            finalColor += shadow * (diffuse + specular);
        
        } else {
            // === PBR Shading GGX/Smith ===
        
            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            float dist = length(lightPositions[i] - fragPosition);
            float attenuation = 1.0 / (dist * dist);
            vec3 radiance = lightColors[i] * attenuation;
                        
            // Normal distribution function GGX (trouwbridge-reitz)
            float D = DGGX(N, H, rough);
        
            // Geometric shadowing GGX (schilick), and smith approximation
            float G = GSmith(N, V, L, rough);

            // Fresnel
            // Calculate F0 (fresnel reflectance at normal incidence)
            vec3 F0 = vec3(0.04); 
            F0 = mix(F0, baseColor, metallic);
            vec3 F = frSchlick(VdotH, F0);
        
            // Overall GGX/Smith 
            vec3 fr = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001); //avoid div by zero // we get the specular with fr

            // energy conservation
            vec3 ks = F; //specular reflectance
            vec3 kd = vec3(1.0) - ks; //diffuse reflectance
            kd *= (1.0 - metallic); //metals have no diffuse component

            vec3 Lo = (kd * baseColor / PI + fr) * radiance * NdotL;
        
            // bringing the whole integral together: fr  * Li * cos(theta)
            finalColor += shadow * Lo;
        }
    }

    // ** environment mapping
    if (useEnvironmentMapping) {
        float reflectionStrength = metal * (1.0 - rough) * 0.5; 
        finalColor = mix(finalColor, reflection, reflectionStrength);
    }
    vec3 ambient = vec3(0.01) * baseColor;
    if (useAOTex) ambient *= ka;
    vec3 color = finalColor + ambient;

    if (useEmissive && hasEmissiveCoords) color += ke;

    // these are now done in the final shader
    //color = color / (color + vec3(1.0)); // tone mapping
    //color = pow(color, vec3(1.0/2.2)); // gamma correction

    //fragColor = vec4(N, 1.0);
    fragColor = vec4(color, 1.0);

    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722)); // relativ luminance
    vec3 bloomContribution = vec3(0.0);
    if (useEmissive && hasEmissiveCoords) bloomContribution += ke;
    if (brightness > bloomThreshold) bloomContribution += color;
    bloomColor = vec4(bloomContribution, 1.0);
}
