//--------------------------------------------------------------------------------------
// File: Stereoscopic.fx
//
// Rendering anaglyph stereoscopic
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

// This structure describes the vertex data to be sent into the vertex shader
struct VS_INPUT
{
    float3 Pos    : POSITION;
    float3 Normal : NORMAL;
	float2 UV     : TEXCOORD0;
};


// This vertex shader input uses a special input type, the vertex ID. This value does not come from the vertex buffer, it is a value
// that automatically increases by one with each vertex processed. As this is the only input for post processing - **no vertex buffer is required**
struct VS_POSTPROCESS_INPUT
{
    uint vertexId : SV_VertexID;
};


// The vertex shader processes the input geometry above and outputs data to be used by the rest of the pipeline. This is the output
// used in the lighting technique - containing a 2D position, lighting colours and texture coordinates
struct VS_LIGHTING_OUTPUT
{
    float4 ProjPos       : SV_POSITION;  // 2D "projected" position for vertex (required output for vertex shader)
	float3 WorldPos      : POSITION;
	float3 WorldNormal   : NORMAL;
    float2 UV            : TEXCOORD0;
};


// More basic techniques don't deal with lighting. So the vertex shader output is different in those cases
struct VS_BASIC_OUTPUT
{
    float4 ProjPos       : SV_POSITION;  // 2D "projected" position for vertex (required output for vertex shader)
    float2 UV            : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

// The matrices (4x4 matrix of floats) for transforming from 3D model to 2D projection (used in vertex shader)
float4x4 WorldMatrix;
float4x4 ViewMatrix;
float4x4 ProjMatrix;

// Information used for lighting (in the vertex or pixel shader)
float3 Light1Pos;
float3 Light2Pos;
float3 Light1Colour;
float3 Light2Colour;
float3 AmbientColour;
float  SpecularPower;
float3 CameraPos;

// Variable used to tint each light model to show the colour that it emits
float3 TintColour;

// Diffuse texture map
Texture2D DiffuseMap;

// Left and right images for anaglyph
Texture2D LeftView;
Texture2D RightView;

// Samplers to use with the above textures
SamplerState TrilinearWrap
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState PointSample
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};


//--------------------------------------------------------------------------------------
// Vertex Shaders
//--------------------------------------------------------------------------------------

// The vertex shader will process each of the vertices in the model, typically transforming/projecting them into 2D at a minimum.
// This vertex shader passes on the vertex position and normal to the pixel shader for per-pixel lighting
//
VS_LIGHTING_OUTPUT VertexLightingTex( VS_INPUT vIn )
{
	VS_LIGHTING_OUTPUT vOut;

	// Use world matrix passed from C++ to transform the input model vertex position into world space
	float4 modelPos = float4(vIn.Pos, 1.0f); // Promote to 1x4 so we can multiply by 4x4 matrix, put 1.0 in 4th element for a point (0.0 for a vector)
	float4 worldPos = mul( modelPos, WorldMatrix );
	vOut.WorldPos = worldPos.xyz;

	// Use camera matrices to further transform the vertex from world space into view space (camera's point of view) and finally into 2D "projection" space for rendering
	float4 viewPos  = mul( worldPos, ViewMatrix );
	vOut.ProjPos    = mul( viewPos,  ProjMatrix );

	// Transform the vertex normal from model space into world space (almost same as first lines of code above)
	float4 modelNormal = float4(vIn.Normal, 0.0f); // Set 4th element to 0.0 this time as normals are vectors
	vOut.WorldNormal = mul( modelNormal, WorldMatrix ).xyz;

	// Pass texture coordinates (UVs) on to the pixel shader, the vertex shader doesn't need them
	vOut.UV = vIn.UV;


	return vOut;
}


// Basic vertex shader to transform 3D model vertices to 2D and pass UVs to the pixel shader
//
VS_BASIC_OUTPUT BasicTransform( VS_INPUT vIn )
{
	VS_BASIC_OUTPUT vOut;
	
	// Use world matrix passed from C++ to transform the input model vertex position into world space
	float4 modelPos = float4(vIn.Pos, 1.0f); // Promote to 1x4 so we can multiply by 4x4 matrix, put 1.0 in 4th element for a point (0.0 for a vector)
	float4 worldPos = mul( modelPos, WorldMatrix );
	float4 viewPos  = mul( worldPos, ViewMatrix );
	vOut.ProjPos    = mul( viewPos,  ProjMatrix );
	
	// Pass texture coordinates (UVs) on to the pixel shader
	vOut.UV = vIn.UV;

	return vOut;
}


//**|3D|********************************************//
//**** DirectX 10 Post Processing Vertex Shader ****//

// This rather unusual shader generates its own vertices - the input data is merely the vertex ID - an automatically generated increasing index.
// There is no vertex buffer, the vertices are generated in this shader. Probably not so efficient, but fine for a full-screen quad.
VS_BASIC_OUTPUT FullScreenQuad(VS_POSTPROCESS_INPUT vIn)
{
    VS_BASIC_OUTPUT vOut;
	
    if (vIn.vertexId == 0)
	{
        vOut.ProjPos = float4(-1.0, 1.0, 0.0, 1.0);
		vOut.UV = float2(0.0, 0.0);
	}
    else if (vIn.vertexId == 1)
	{
        vOut.ProjPos = float4(-1.0,-1.0, 0.0, 1.0);
		vOut.UV = float2(0.0, 1.0);
	}
    else if (vIn.vertexId == 2)
	{
        vOut.ProjPos = float4( 1.0, 1.0, 0.0, 1.0);
		vOut.UV = float2(1.0, 0.0);
	}
	else
	{
        vOut.ProjPos = float4( 1.0,-1.0, 0.0, 1.0);
		vOut.UV = float2(1.0, 1.0);
	}
	
    return vOut;
}

//**************************************************//


//--------------------------------------------------------------------------------------
// Pixel Shaders
//--------------------------------------------------------------------------------------

// The pixel shader determines colour for each pixel in the rendered polygons, given the data passed on from the vertex shader
// This shader expects vertex position, normal and UVs from the vertex shader. It calculates per-pixel lighting and combines with diffuse and specular map
//
float4 VertexLitDiffuseMap( VS_LIGHTING_OUTPUT vOut ) : SV_Target  // The ": SV_Target" bit just indicates that the returned float4 colour goes to the render target (i.e. it's a colour to render)
{
	// Can't guarantee the normals are length 1 now (because the world matrix may contain scaling), so renormalise
	// If lighting in the pixel shader, this is also because the interpolation from vertex shader to pixel shader will also rescale normals
	float3 worldNormal = normalize(vOut.WorldNormal); 


	///////////////////////
	// Calculate lighting

	// Calculate direction of camera
	float3 CameraDir = normalize(CameraPos - vOut.WorldPos.xyz); // Position of camera - position of current vertex (or pixel) (in world space)
	
	//// LIGHT 1
	float3 Light1Dir = normalize(Light1Pos - vOut.WorldPos.xyz);   // Direction for each light is different
	float3 Light1Dist = length(Light1Pos - vOut.WorldPos.xyz); 
	float3 DiffuseLight1 = Light1Colour * max( dot(worldNormal.xyz, Light1Dir), 0 ) / Light1Dist;
	float3 halfway = normalize(Light1Dir + CameraDir);
	float3 SpecularLight1 = DiffuseLight1 * pow( max( dot(worldNormal.xyz, halfway), 0 ), SpecularPower );

	//// LIGHT 2
	float3 Light2Dir = normalize(Light2Pos - vOut.WorldPos.xyz);
	float3 Light2Dist = length(Light2Pos - vOut.WorldPos.xyz);
	float3 DiffuseLight2 = Light2Colour * max( dot(worldNormal.xyz, Light2Dir), 0 ) / Light2Dist;
	halfway = normalize(Light2Dir + CameraDir);
	float3 SpecularLight2 = DiffuseLight2 * pow( max( dot(worldNormal.xyz, halfway), 0 ), SpecularPower );

	// Sum the effect of the two lights - add the ambient at this stage rather than for each light (or we will get twice the ambient level)
	float3 DiffuseLight = AmbientColour + DiffuseLight1 + DiffuseLight2;
	float3 SpecularLight = SpecularLight1 + SpecularLight2;


	////////////////////
	// Sample texture

	// Extract diffuse material colour for this pixel from a texture
	float4 DiffuseMaterial = DiffuseMap.Sample( TrilinearWrap, vOut.UV );
	
	// Assume specular material colour is white (i.e. highlights are a full, untinted reflection of light)
	float3 SpecularMaterial = DiffuseMaterial.a;

	
	////////////////////
	// Combine colours 
	
	// Combine maps and lighting for final pixel colour
	float4 combinedColour;
	combinedColour.rgb = DiffuseMaterial.rgb * DiffuseLight + SpecularMaterial * SpecularLight;
	combinedColour.a = 1.0f; // No alpha processing in this shader, so just set it to 1

	return combinedColour;
}


// A pixel shader that just tints a (diffuse) texture with a fixed colour
//
float4 TintDiffuseMap( VS_BASIC_OUTPUT vOut ) : SV_Target
{
	// Extract diffuse material colour for this pixel from a texture
	float4 diffuseMapColour = DiffuseMap.Sample( TrilinearWrap, vOut.UV );

	// Tint by global colour (set from C++)
	diffuseMapColour.rgb *= TintColour / 10;

	return diffuseMapColour;
}


//**|3D|*************************//
//**** Anaglyph Pixel Shader ****//

float4 ColourAnaglyph( VS_BASIC_OUTPUT vOut ) : SV_Target
{
	// Extract left and right pixel values
	float3 leftColour  = LeftView.Sample( PointSample, vOut.UV ).rgb;
	float3 rightColour = RightView.Sample( PointSample, vOut.UV ).rgb;

	// Combine into anaglyph
	const float3 grey = float3( 0.299f, 0.587f, 0.114f );
	const float3 opti = float3( 0.0f,   0.7f,   0.3f );

	//*** MISSING - well todo really ****//
	// Change this line to support other anaglyph modes (e.g. greyscale, half-colour or optimised) - two convenient float3 vectors are set-up above
	
	//regular
	float4 anaglyph = float4( leftColour.r, rightColour.g, rightColour.b, 1.0f ); // Simple anaglyph
	
	//grey
	//float4 anaglyph;
	//anaglyph.r = dot(grey, leftColour);
	//anaglyph.g = dot( grey, rightColour);
	//anaglyph.b = dot( grey, rightColour );
	//anaglyph.a = 1.0f;
	
	//opti
	//float4 anaglyph;
	//anaglyph.r = pow(dot( opti, leftColour), 57);

	//anaglyph.g = dot( float3(0,1,0), rightColour);
	//anaglyph.b = dot( float3(0, 0, 1), rightColour );
	//anaglyph.a = 1.0f;

	return anaglyph;
}

//*******************************//


//--------------------------------------------------------------------------------------
// States
//--------------------------------------------------------------------------------------

// States are needed to switch between additive blending for the lights and no blending for other models

RasterizerState CullNone  // Cull none of the polygons, i.e. show both sides
{
	CullMode = None;
};
RasterizerState CullBack  // Cull back side of polygon - normal behaviour, only show front of polygons
{
	CullMode = Back;
};


DepthStencilState DepthWritesOff // Don't write to the depth buffer - polygons rendered will not obscure other polygons
{
	DepthFunc      = LESS;
	DepthWriteMask = ZERO;
};
DepthStencilState DepthWritesOn  // Write to the depth buffer - normal behaviour 
{
	DepthFunc      = LESS;
	DepthWriteMask = ALL;
};
DepthStencilState DisableDepth   // Disable depth buffer entirely
{
	DepthFunc      = ALWAYS;
	DepthWriteMask = ZERO;
};


BlendState NoBlending // Switch off blending - pixels will be opaque
{
    BlendEnable[0] = FALSE;
};
BlendState AdditiveBlending // Additive blending is used for lighting effects
{
    BlendEnable[0] = TRUE;
    SrcBlend = ONE;
    DestBlend = ONE;
    BlendOp = ADD;
};


//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------

// Techniques are used to render models in our scene. They select a combination of vertex, geometry and pixel shader from those provided above. Can also set states.

// Vertex lighting with diffuse map
technique10 VertexLitTex
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VertexLightingTex() ) );
        SetGeometryShader( NULL );                                   
        SetPixelShader( CompileShader( ps_4_0, VertexLitDiffuseMap() ) );

		// Switch off blending states
		SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetRasterizerState( CullBack ); 
		SetDepthStencilState( DepthWritesOn, 0 );
	}
}


// Additive blended texture. No lighting, but uses a global colour tint. Used for light models
technique10 AdditiveTexTint
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, BasicTransform() ) );
        SetGeometryShader( NULL );                                   
        SetPixelShader( CompileShader( ps_4_0, TintDiffuseMap() ) );

		SetBlendState( AdditiveBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetRasterizerState( CullNone ); 
		SetDepthStencilState( DepthWritesOff, 0 );
     }
}


//**|3D|******************************//
// Anaglyph Post-Processing Technique

// Draw full screen quad combining left and right views into an anaglyph output. 
// Doesn't require any vertex or index data (see FullScreenQuad vertex shader)
technique10 CreateAnaglyph
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, FullScreenQuad() ) );
        SetGeometryShader( NULL );                                   
        SetPixelShader( CompileShader( ps_4_0, ColourAnaglyph() ) );

		SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetRasterizerState( CullNone ); 
		SetDepthStencilState( DisableDepth, 0 );
     }
}

//************************************//

