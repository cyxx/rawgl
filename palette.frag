
uniform sampler1D Palette;
uniform sampler2D IndexedColorTexture;

void main()
{
	vec4 index = texture2D(IndexedColorTexture, gl_TexCoord[0].xy);
	vec4 color = texture1D(Palette, index.r);
	gl_FragColor = color;
//	gl_FragColor = vec4(index.r, index.r, index.r, 1.);
}
