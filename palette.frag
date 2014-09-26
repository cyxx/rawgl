
uniform sampler1D Palette;
uniform sampler2D IndexedColorTexture;

void main()
{
	vec4 index = texture2D(IndexedColorTexture, gl_TexCoord[0].xy);
	if (index.r == 1.) { // bitmap background
		gl_FragColor = vec4(0., 0., 0., 0.);
	} else if (index.r > .5) { // alpha blending
		vec4 color = texture1D(Palette, 8. / 31.);
		gl_FragColor = vec4(color.r, color.g, color.b, .5);
	} else { // 16 colors game palette
		vec4 color = texture1D(Palette, index.r);
		gl_FragColor = color;
	}
}
