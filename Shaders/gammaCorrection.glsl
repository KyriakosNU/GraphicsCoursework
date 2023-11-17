#version 330 core

uniform sampler2D sceneTex ;

in Vertex {
	vec2 texCoord;
} IN ;

out vec4 fragColor ;

void main ( void ) {
	fragColor =  texture2D ( sceneTex , IN.texCoord.xy);

	float gamma = 2.2;
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/gamma));
}