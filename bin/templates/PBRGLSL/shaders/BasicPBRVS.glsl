#version 330

uniform mat4 matVP;
uniform mat4 matGeo;
uniform bool flipTextureY = false;
#if 1

layout(location=0) in vec3 positionIn;
layout(location=1) in vec3 normalIn;
layout(location=2) in vec2 texcoordIn;
layout(location=3) in vec3 tangentIn;
layout(location=4) in vec3 bitangentIn;


out vec4 color;
out vec3 position;
out vec3 normal;
out vec2 texcoord;
out mat3 tangentMatrix;

void main() {
	color = vec4(abs(normalIn), 1.0);
	position = vec3(matGeo * vec4(positionIn, 1.0));
	normal = normalIn;
   
	if(flipTextureY)
		texcoord = vec2(texcoordIn.x, 1-texcoordIn.y);
	else
		texcoord = vec2(texcoordIn.xy);
   
   tangentMatrix = mat3(tangentIn, bitangentIn, normalIn);
   
   gl_Position = matVP * matGeo * vec4(positionIn, 1);
}


#else //Simple shader to test
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

out vec4 color;

void main() {
   color = vec4(abs(normal), 1.0);
   gl_Position = matVP * matGeo * vec4(pos, 1);
}
#endif
