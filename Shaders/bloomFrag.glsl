#version 330 core

uniform int isVertical ;
uniform sampler2D sceneTex ;
uniform bool isInverted ;

in Vertex {
	vec2 texCoord;
} IN ;

out vec4 fragColor ;

const float scaleFactors [7] =
float [](0.006 , 0.061 , 0.242 , 0.383 , 0.242 , 0.061 , 0.006);

void main ( void ) {
	fragColor = vec4 (0 ,0 ,0 ,1);
	vec2 delta = vec2 (0 ,0);
	if( isVertical == 1) {
		delta = dFdy ( IN.texCoord );
	}
	else {
	delta = dFdx ( IN.texCoord );
	}
	for (int i = 0; i < 7; i ++ ) {
		vec2 offset = delta * ( i - 3);
		vec4 tmp = texture2D ( sceneTex , IN.texCoord.xy + offset );
		fragColor += tmp * scaleFactors [ i ];
	}
	vec4 tex =  texture2D ( sceneTex , IN.texCoord.xy);
	
	float brightness = dot(tex.rgb, vec3(0.2126, 0.7152, 0.0722));
	
	if(	(brightness >= 0.1f && !isInverted) ||
		(brightness <= 0.1f && isInverted))
        fragColor = vec4(fragColor.rgb, 1.0);
    else
        fragColor = vec4(tex.rgb, 1.0);
}