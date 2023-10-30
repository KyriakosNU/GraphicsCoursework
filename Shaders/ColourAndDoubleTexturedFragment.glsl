# version 330 core
uniform sampler2D diffuseTex;

vec4 combTex;

in Vertex {
	vec4 colour ;
	vec2 texCoord1 ;
	vec2 texCoord2 ;
} IN ;

out vec4 fragColour ;
void main ( void ) {
	combTex = mix( texture ( diffuseTex , IN.texCoord1 ) , texture ( diffuseTex , IN.texCoord2 ) , 0.5)
	fragColour = mix( combTex , IN.colour, 0.25);
}