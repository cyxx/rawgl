
uniform sampler1D Palette;
uniform sampler2D IndexedColorTexture;

void main()
{
	vec4 index = texture2D(IndexedColorTexture, gl_TexCoord[0].xy);
	if (index.r == 1.) {
		gl_FragColor = vec4(0., 0., 0., 0.);
	} else {
		vec4 color = texture1D(Palette, index.r);
		gl_FragColor = color;
	}
}
