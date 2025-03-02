#version 300 es
// BEWARE: This is a example shader and it has not been optimized for speed.

precision mediump float;
uniform sampler2D TextureDiffuse;
uniform sampler2D TextureSpecular;
uniform sampler2D TextureNormal;

// Material
uniform vec4 MatAmbient;
uniform float MatShininess;


in vec4 v_Color;
in vec3 v_LightVec;
in vec3 v_HalfVec;
in vec2 v_TexCoord;

out vec4 o_fragColor;

void main()
{
  // Read the normal from the normal map and move it from the [0,1] range to [-1,1] and normalize it
  vec3 normal = normalize((texture(TextureNormal, v_TexCoord).xyz * 2.0) - 1.0);

  // set the specular term to black
  vec4 spec = vec4(0.0);

  // Calc the intensity as the dot product the max prevents negative intensity values
  float intensity = max(dot(normal, v_LightVec), 0.0);

  vec4 diffuse = texture(TextureDiffuse, v_TexCoord);

  // if the vertex is lit calc the specular term
  if (intensity > 0.0)
  {
    // Calc the specular term into spec
    float intSpec = max(dot(normalize(v_HalfVec), normal), 0.0);
    spec = texture(TextureSpecular, v_TexCoord) * pow(intSpec, MatShininess);
  }

  o_fragColor = diffuse * ((v_Color * intensity) + spec + MatAmbient);
}
