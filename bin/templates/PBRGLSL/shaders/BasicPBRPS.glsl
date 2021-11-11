#version 330

#if 1
in vec4 color;
in vec3 position;
in vec3 normal;
in vec2 texcoord;
in mat3 tangentMatrix;

uniform bool flipNormalY = false;

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
uniform int debugValue = 0;
uniform float debugMip = 0;
#endif

uniform vec3 cameraPosition = vec3(1,0,0);//Pass by the IDE itself
uniform vec3 mainLightDir = vec3(1,0,0);
uniform vec3 mainLightColor = vec3(1,1,1);
uniform float mainLightIntensity = 3.14;
uniform vec3 ambientColor = vec3(0,0,0);

uniform sampler2D albedoTexture; //0
uniform sampler2D normalTexture; //1
uniform sampler2D metalnessTexture; //2
uniform sampler2D roughnessTexture; //3
uniform sampler2D ambientOcclusionTexture; //4
uniform samplerCube specularTexture; //5
uniform samplerCube irradianceTexture; //6
uniform sampler2D specularBRDF_LUT; //7

#define saturate(a) clamp(a, 0.0, 1.0)

#define half float
#define half2 vec2
#define half3 vec3
#define half4 vec4

out vec4 outColor;

//////////////////////////////////////////////////////////////////
const float PI = 3.1415926535897932f;
const float INV_PI = 1.0f/3.1415926535897932f;
const float EPSILON = 6.10352e-5f;
// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

vec4 getTextureValue(sampler2D s, vec2 coord)
{
	if(debugMip < EPSILON)
		return texture(s, coord);
	else
		return textureLod(s, coord, debugMip);
}

half3 LinearToSrgbBranchless(half3 lin) 
{
	lin = max(half3(6.10352e-5,6.10352e-5,6.10352e-5), lin); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	return min(lin * 12.92f, pow(max(lin, half3(0.00313067,0.00313067,0.00313067)), half3(1.0f/2.4f, 1.0f/2.4f, 1.0f/2.4f)) * 1.055f - 0.055f);
	// Possible that mobile GPUs might have native pow() function?
	//return min(lin * 12.92, exp2(log2(max(lin, 0.00313067)) * (1.0/2.4) + log2(1.055)) - 0.055);
}
half3 sRGBToLinear( half3 Color ) 
{
	Color = max(half3(6.10352e-5, 6.10352e-5, 6.10352e-5), Color); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	half3 ColorPowered = pow( Color * (1.0f / 1.055f) + 0.0521327f, half3(2.4f,2.4f,2.4f) );
	return half3((Color.r > 0.04045f ? ColorPowered.r : Color.r * (1.0f / 12.92f)),
		(Color.g > 0.04045f ? ColorPowered.g : Color.g * (1.0f / 12.92f)),
		(Color.b > 0.04045f ? ColorPowered.b : Color.b * (1.0f / 12.92f))
	);
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

vec3 acesToneMapping(vec3 color, float adapted_lum)
{
	const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;

	color *= adapted_lum;
	return (color * (A * color + B)) / (color * (C * color + D) + E);
}

vec3 finalColorCorrection(vec3 color){
	return acesToneMapping(color, 1) ;
}

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

//////////////////////////////////////////////////////////////////

void main() {
	float ndl = dot(mainLightDir, normalize(normal));
	vec3 albedo = sRGBToLinear(getTextureValue(albedoTexture, texcoord).rgb);
	float metalness = getTextureValue(metalnessTexture, texcoord).r;
	float roughness = getTextureValue(roughnessTexture, texcoord).r;
	float ao = getTextureValue(ambientOcclusionTexture, texcoord).r;
	
	//ViewDirection
	vec3 V = normalize(cameraPosition - position);
	
	// Get current fragment's normal and transform to world space.
	vec3 normalValue = getTextureValue(normalTexture, texcoord).rgb;
	if(flipNormalY)
		normalValue.g = 1-normalValue.g;
	vec3 N = normalize(2.0 * normalValue  - 1.0);
	N = normalize(tangentMatrix * N);
	
	// Angle between surface normal and outgoing light direction.
	float cosLo = max(0.0, dot(N, V));

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	vec3 F0 = mix(Fdielectric, albedo, metalness);
	
	vec3 finalColor = vec3(0,0,0);
	
	vec3 directLighting = vec3(0);
	vec3 directDiffuseLighting = vec3(0);
	vec3 directSpecularLighting = vec3(0);
	#if 1 //Directional
	{
		vec3 Li = mainLightDir;
		vec3 Lradiance = mainLightColor * mainLightIntensity;

		// Half-vector between Li and Lo.
		vec3 Lh = normalize(Li + V);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(N, Li));
		float cosLh = max(0.0, dot(N, Lh));

		// Calculate Fresnel term for direct lighting. 
		vec3 F  = fresnelSchlick(F0, max(0.0f, dot(Lh, V)));
		// Calculate normal distribution for specular BRDF.
		float D = ndfGGX(cosLh, roughness);
		// Calculate geometric attenuation for specular BRDF.
		float G = gaSchlickGGX(cosLi, cosLo, roughness);

		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
		vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);

		// Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
		vec3 diffuseBRDF = kd * albedo;

		// Cook-Torrance specular microfacet BRDF.
		vec3 specularBRDF = (F * D * G) / max(EPSILON, 4.0f * cosLi * cosLo);

		// Total contribution for this light.
		directLighting += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
		
		directDiffuseLighting = diffuseBRDF * Lradiance * cosLi;
		directSpecularLighting = specularBRDF * Lradiance * cosLi;
	}
	finalColor += directLighting;
	#endif
	

	vec3 totalIBL = vec3(0);
	vec3 diffuseIBL = vec3(0);
	vec3 specularIBL = vec3(0);
	vec3 Lr = vec3(0);
	#if 1
	{
		// Ambient lighting (IBL).
		// Specular reflection vector.
		Lr = 2.0 * cosLo * N - V;
		{
			// Sample diffuse irradiance at normal direction.
			vec3 irradiance = texture(irradianceTexture, N).rgb;
	
			// Calculate Fresnel term for ambient lighting.
			// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
			// use cosLo instead of angle with light's half-vector (cosLh above).
			// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
			vec3 F = fresnelSchlick(F0, cosLo);

			// Get diffuse contribution factor (as with direct lighting).
			vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);
	
			// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
			diffuseIBL = kd * albedo * irradiance;
	
			// Sample pre-filtered specular reflection environment at correct mipmap level.
			int specularTextureLevels = 11;
			vec3 specularIrradiance = textureLod(specularTexture, Lr, roughness * specularTextureLevels).rgb;
			// Split-sum approximation factors for Cook-Torrance specular BRDF.
			vec2 specularBRDF = textureLod(specularBRDF_LUT, vec2(cosLo, roughness), 0).rg; //We need only the lod 0

			// Total specular IBL contribution.
			specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;
	
			// Total ambient lighting contribution.
			totalIBL = diffuseIBL + specularIBL;
		}
		finalColor+=totalIBL;
	}
	#endif
	
	
	#if 0
	//Diffuse
	vec3 directLighting = albedo.rgb * saturate(dot(N, mainLightDir));
	directLighting *= mainLightColor * mainLightIntensity;
	directLighting *= INV_PI;
	finalColor += directLighting;
	
	#endif
	
	//Ambient
	finalColor += ambientColor;
	vec3 finalColorInsRGB = finalColor;
	if(debugValue != 0)
	{
		//Adopt substance painter's preview, only albedo will be restored to srgb
		if(debugValue == 1)
			finalColor = finalColorCorrection(albedo.rgb);
		else if(debugValue == 2)
			finalColor = normalValue.rgb;
		else if(debugValue == 3)
			finalColor = vec3(metalness, metalness, metalness);
		else if(debugValue == 4)
			finalColor = vec3(roughness, roughness, roughness);
		else if(debugValue == 5)
			finalColor = N * 0.5f + vec3(0.5f);
		else if(debugValue == 6) //Tangent
			finalColor = tangentMatrix[0]* 0.5f + vec3(0.5f);
		else if(debugValue == 7)//Bi-Normal
			finalColor = tangentMatrix[1]* 0.5f + vec3(0.5f);
		else if(debugValue == 8)
			finalColor = Lr.rgb* 0.5f + vec3(0.5f);
		else if(debugValue == 10)
			finalColor = finalColorCorrection(directLighting);
		else if(debugValue == 11)
			finalColor = finalColorCorrection(directSpecularLighting);
		else if(debugValue == 12)
			finalColor = finalColorCorrection(directLighting);
		else if(debugValue == 13)
			finalColor = finalColorCorrection(diffuseIBL);
		else if(debugValue == 14)
			finalColor = finalColorCorrection(specularIBL);
		else if(debugValue == 15)
			finalColor = finalColorCorrection(totalIBL);
		//else if(debugValue == 16)
		//	finalColor = finalColorCorrection(translucencyColor);
						
		//finalColorInsRGB = finalColorCorrection(finalColor);
		finalColorInsRGB = finalColor;		
	}
	else
	{
		finalColorInsRGB = finalColorCorrection(finalColor);
	}
	
	outColor = vec4(finalColorInsRGB, 1);
}
#else

in vec4 color;
out vec4 outColor;

void main() {
   outColor = vec4(color);
}
#endif
