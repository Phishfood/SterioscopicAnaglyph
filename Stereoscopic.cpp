//--------------------------------------------------------------------------------------
//	Stereoscopic.cpp
//
//	Rendering a scene using stereoscopy
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <atlbase.h>
#include "resource.h"

#include "Defines.h" // General definitions shared by all source files
#include "Model.h"   // Model class - new, encapsulates working with vertex/index data and world matrix
#include "Camera.h"  // Camera class - new, encapsulates the camera's view and projection matrix
#include "CTimer.h"  // Timer class - not DirectX
#include "Input.h"   // Input functions - not DirectX


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

// Models and cameras encapsulated in simple classes
CModel* Cube;
CModel* Stars;
CModel* Crate;
CModel* Ground;
CCamera* MainCamera;


//**|3D|** Left and Right Renders ****//

// The textures for left and right images and their views as render target and shader resource
ID3D10Texture2D*          LeftTexture = NULL;
ID3D10Texture2D*          RightTexture = NULL;
ID3D10RenderTargetView*   LeftRenderTarget = NULL;
ID3D10RenderTargetView*   RightRenderTarget = NULL;
ID3D10ShaderResourceView* LeftShaderResource = NULL;
ID3D10ShaderResourceView* RightShaderResource = NULL;

// Interocular distance
float Interocular = 0.65f;

//************************************//


// Textures
ID3D10ShaderResourceView* CubeDiffuseMap = NULL;
ID3D10ShaderResourceView* StarsDiffuseMap = NULL;
ID3D10ShaderResourceView* CrateDiffuseMap = NULL;
ID3D10ShaderResourceView* GroundDiffuseMap = NULL;
ID3D10ShaderResourceView* LightDiffuseMap = NULL;

// Light data stored manually, a light class would be helpful - but it's an assignment task for the second years
D3DXVECTOR4 BackgroundColour = D3DXVECTOR4( 0.2f, 0.2f, 0.3f, 1.0f );
D3DXVECTOR3 AmbientColour    = D3DXVECTOR3( 0.4f, 0.4f, 0.5f );
D3DXVECTOR3 Light1Colour     = D3DXVECTOR3( 0.8f, 0.8f, 1.0f ) * 8;
D3DXVECTOR3 Light2Colour     = D3DXVECTOR3( 1.0f, 0.8f, 0.2f ) * 30;
float SpecularPower = 256.0f;

// Display models where the lights are. One of the lights will follow an orbit
CModel* Light1;
CModel* Light2;
const float LightOrbitRadius = 20.0f;
const float LightOrbitSpeed  = 0.7f;

// Note: There are move & rotation speed constants in Defines.h



//--------------------------------------------------------------------------------------
// Shader Variables
//--------------------------------------------------------------------------------------
// Variables to connect C++ code to HLSL shaders

// Effects / techniques
ID3D10Effect*          Effect = NULL;
ID3D10EffectTechnique* VertexLitTexTechnique = NULL;
ID3D10EffectTechnique* AdditiveTexTintTechnique = NULL;
ID3D10EffectTechnique* AnaglyphTechnique = NULL;

// Matrices
ID3D10EffectMatrixVariable* WorldMatrixVar = NULL;
ID3D10EffectMatrixVariable* ViewMatrixVar = NULL;
ID3D10EffectMatrixVariable* ProjMatrixVar = NULL;
ID3D10EffectMatrixVariable* ViewProjMatrixVar = NULL;

// Textures
ID3D10EffectShaderResourceVariable* DiffuseMapVar = NULL;
ID3D10EffectShaderResourceVariable* LeftViewVar = NULL;
ID3D10EffectShaderResourceVariable* RightViewVar = NULL;

// Light Effect variables
ID3D10EffectVectorVariable* CameraPosVar = NULL;
ID3D10EffectVectorVariable* Light1PosVar = NULL;
ID3D10EffectVectorVariable* Light1ColourVar = NULL;
ID3D10EffectVectorVariable* Light2PosVar = NULL;
ID3D10EffectVectorVariable* Light2ColourVar = NULL;
ID3D10EffectVectorVariable* AmbientColourVar = NULL;
ID3D10EffectScalarVariable* SpecularPowerVar = NULL;

// Other 
ID3D10EffectVectorVariable* TintColourVar = NULL;


//--------------------------------------------------------------------------------------
// DirectX Variables
//--------------------------------------------------------------------------------------

// The main D3D interface
ID3D10Device* g_pd3dDevice = NULL;

// Variables used to setup D3D
IDXGISwapChain*         SwapChain = NULL;
ID3D10Texture2D*        DepthStencil = NULL;
ID3D10DepthStencilView* DepthStencilView = NULL;
ID3D10RenderTargetView* BackBufferRenderTarget = NULL;

// Variables used to setup the Window
HINSTANCE HInst = NULL;
HWND      HWnd = NULL;
int       g_ViewportWidth;
int       g_ViewportHeight;



//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

bool InitDevice();
void ReleaseResources();
bool LoadEffectFile();
bool InitScene();
void UpdateScene( float frameTime );
void RenderModels( CCamera* camera, EStereoscopic stereo = Monoscopic, float interocular = 0.065f );
void RenderScene();
bool InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );



//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
bool InitDevice()
{
	HRESULT hr = S_OK;


	////////////////////////////////
	// Initialise Direct3D

	// Calculate the visible area the window we are using - the "client rectangle" refered to in the first function is the 
	// size of the interior of the window, i.e. excluding the frame and title
	RECT rc;
	GetClientRect( HWnd, &rc );
	g_ViewportWidth = rc.right - rc.left;
	g_ViewportHeight = rc.bottom - rc.top;


	// Create a Direct3D device and create a swap-chain (create a back buffer to render to)
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = g_ViewportWidth;             // Target window size
	sd.BufferDesc.Height = g_ViewportHeight;           // --"--
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Pixel format of target window
	sd.BufferDesc.RefreshRate.Numerator = 60;          // Refresh rate of monitor
	sd.BufferDesc.RefreshRate.Denominator = 1;         // --"--
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.OutputWindow = HWnd;                            // Target window
	sd.Windowed = TRUE;                                // Whether to render in a window (TRUE) or go fullscreen (FALSE)
	hr = D3D10CreateDeviceAndSwapChain( NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, D3D10_CREATE_DEVICE_DEBUG, D3D10_SDK_VERSION, &sd, &SwapChain, &g_pd3dDevice );
	if( FAILED( hr ) ) return false;


	// Indicate that the back-buffer can be "viewed" as a render target - standard behaviour
	ID3D10Texture2D* pBackBuffer;
	hr = SwapChain->GetBuffer( 0, __uuidof( ID3D10Texture2D ), ( LPVOID* )&pBackBuffer );
	if( FAILED( hr ) ) return false;
	hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &BackBufferRenderTarget );
	pBackBuffer->Release();
	if( FAILED( hr ) ) return false;


	// Create a texture for a depth buffer
	D3D10_TEXTURE2D_DESC descDepth;
	descDepth.Width = g_ViewportWidth;
	descDepth.Height = g_ViewportHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D32_FLOAT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D10_USAGE_DEFAULT;
	descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D( &descDepth, NULL, &DepthStencil );
	if( FAILED( hr ) ) return false;

	// Create the depth stencil view, i.e. indicate that the texture just created is to be used as a depth buffer
	D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView( DepthStencil, &descDSV, &DepthStencilView );
	if( FAILED( hr ) ) return false;

	return true;
}


// Release the memory held by all objects created
void ReleaseResources()
{
	if( g_pd3dDevice ) g_pd3dDevice->ClearState();

	delete Light2;
	delete Light1;
	delete Ground;
	delete Crate;
	delete Stars;
	delete Cube;
	delete MainCamera;

	if (RightShaderResource)    RightShaderResource->Release();
	if (LeftShaderResource)     LeftShaderResource->Release();
	if (RightRenderTarget)      RightRenderTarget->Release();
	if (LeftRenderTarget)       LeftRenderTarget->Release();
	if (RightTexture)           RightTexture->Release();
	if (LeftTexture)            LeftTexture->Release();
    if (GroundDiffuseMap)       GroundDiffuseMap->Release();
    if (LightDiffuseMap)        LightDiffuseMap->Release();
    if (CrateDiffuseMap)        CrateDiffuseMap->Release();
    if (StarsDiffuseMap)        StarsDiffuseMap->Release();
    if (CubeDiffuseMap)         CubeDiffuseMap->Release();
	if (Effect)                 Effect->Release();
	if (DepthStencilView)       DepthStencilView->Release();
	if (BackBufferRenderTarget) BackBufferRenderTarget->Release();
	if (DepthStencil)           DepthStencil->Release();
	if (SwapChain)              SwapChain->Release();
	if (g_pd3dDevice)           g_pd3dDevice->Release();
}



//--------------------------------------------------------------------------------------
// Load and compile Effect file (.fx file containing shaders)
//--------------------------------------------------------------------------------------

// All techniques in one file in this lab
bool LoadEffectFile()
{
	ID3D10Blob* pErrors; // This strangely typed variable collects any errors when compiling the effect file
	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS; // These "flags" are used to set the compiler options

	// Load and compile the effect file
	HRESULT hr = D3DX10CreateEffectFromFile( L"Stereoscopic.fx", NULL, NULL, "fx_4_0", dwShaderFlags, 0, g_pd3dDevice, NULL, NULL, &Effect, &pErrors, NULL );
	if( FAILED( hr ) )
	{
		if (pErrors != 0)  MessageBox( NULL, CA2CT(reinterpret_cast<char*>(pErrors->GetBufferPointer())), L"Error", MB_OK ); // Compiler error: display error message
		else               MessageBox( NULL, L"Error loading FX file. Ensure your FX file is in the same folder as this executable.", L"Error", MB_OK );  // No error message - probably file not found
		return false;
	}

	// Select techniques from the compiled effect file
	VertexLitTexTechnique    = Effect->GetTechniqueByName( "VertexLitTex" );
	AdditiveTexTintTechnique = Effect->GetTechniqueByName( "AdditiveTexTint" );
	AnaglyphTechnique        = Effect->GetTechniqueByName( "CreateAnaglyph" );

	// Create variables to access global variables in the shaders from C++
	WorldMatrixVar    = Effect->GetVariableByName( "WorldMatrix"    )->AsMatrix();
	ViewMatrixVar     = Effect->GetVariableByName( "ViewMatrix"     )->AsMatrix();
	ProjMatrixVar     = Effect->GetVariableByName( "ProjMatrix"     )->AsMatrix();
	ViewProjMatrixVar = Effect->GetVariableByName( "ViewProjMatrix" )->AsMatrix();

	// Textures in shader (shader resources)
	DiffuseMapVar = Effect->GetVariableByName( "DiffuseMap" )->AsShaderResource();
	LeftViewVar   = Effect->GetVariableByName( "LeftView"   )->AsShaderResource();
	RightViewVar  = Effect->GetVariableByName( "RightView"  )->AsShaderResource();

	// Also access shader variables needed for lighting
	CameraPosVar     = Effect->GetVariableByName( "CameraPos"     )->AsVector();
	Light1PosVar     = Effect->GetVariableByName( "Light1Pos"     )->AsVector();
	Light1ColourVar  = Effect->GetVariableByName( "Light1Colour"  )->AsVector();
	Light2PosVar     = Effect->GetVariableByName( "Light2Pos"     )->AsVector();
	Light2ColourVar  = Effect->GetVariableByName( "Light2Colour"  )->AsVector();
	AmbientColourVar = Effect->GetVariableByName( "AmbientColour" )->AsVector();
	SpecularPowerVar = Effect->GetVariableByName( "SpecularPower" )->AsScalar();

	// Other shader variables
	TintColourVar = Effect->GetVariableByName( "TintColour"  )->AsVector();

	return true;
}



//--------------------------------------------------------------------------------------
// Scene Setup / Update
//--------------------------------------------------------------------------------------

// Create / load the camera, models and textures for the scene
bool InitScene()
{
	///////////////////
	// Create cameras

	// Only using one camera, will split into a left and right camera at render-time
	MainCamera = new CCamera();
	MainCamera->SetPosition( D3DXVECTOR3(-15, 35,-70) );
	MainCamera->SetRotation( D3DXVECTOR3(ToRadians(10.0f), ToRadians(18.0f), 0.0f) ); // ToRadians is a new helper function to convert degrees to radians


	///////////////////////
	// Load/Create models

	Cube   = new CModel;
	Stars = new CModel;
	Crate  = new CModel;
	Ground = new CModel;
	Light1 = new CModel;
	Light2 = new CModel;

	// Load .X files for each model
	if (!Cube->  Load( "Cube.x",  VertexLitTexTechnique )) return false;
	if (!Stars->Load( "Stars.x", VertexLitTexTechnique )) return false;
	if (!Crate-> Load( "CargoContainer.x", VertexLitTexTechnique )) return false;
	if (!Ground->Load( "Hills.x", VertexLitTexTechnique )) return false;
	if (!Light1->Load( "Light.x", AdditiveTexTintTechnique )) return false;
	if (!Light2->Load( "Light.x", AdditiveTexTintTechnique )) return false;

	// Initial positions
	Cube->  SetPosition( D3DXVECTOR3(0, 15, 0) );
	Crate-> SetPosition( D3DXVECTOR3(-10, 0, 90) );
	Crate-> SetScale( 6.0f );
	Crate-> SetRotation( D3DXVECTOR3(0.0f, ToRadians(40.0f), 0.0f) );
	Stars-> SetScale( 10000.0f );
	Light1->SetPosition( D3DXVECTOR3(30, 10, 0) );
	Light1->SetScale( 4.0f );
	Light2->SetPosition( D3DXVECTOR3(-20, 30, 50) );
	Light2->SetScale( 8.0f );


	//////////////////
	// Load textures

	if (FAILED( D3DX10CreateShaderResourceViewFromFile( g_pd3dDevice, L"StoneDiffuseSpecular.dds", NULL, NULL, &CubeDiffuseMap,  NULL ) )) return false;
	if (FAILED( D3DX10CreateShaderResourceViewFromFile( g_pd3dDevice, L"CargoA.dds",               NULL, NULL, &CrateDiffuseMap, NULL ) )) return false;
	if (FAILED( D3DX10CreateShaderResourceViewFromFile( g_pd3dDevice, L"StarsHi.jpg",              NULL, NULL, &StarsDiffuseMap, NULL ) )) return false;
	if (FAILED( D3DX10CreateShaderResourceViewFromFile( g_pd3dDevice, L"tiles1.jpg",               NULL, NULL, &GroundDiffuseMap, NULL ) )) return false;
	if (FAILED( D3DX10CreateShaderResourceViewFromFile( g_pd3dDevice, L"flare.jpg",                NULL, NULL, &LightDiffuseMap, NULL ) )) return false;


	//**|3D|** Left and Right Render Target Textures ****//

	// Create the textures for left and right views
	D3D10_TEXTURE2D_DESC textureDesc;
	textureDesc.Width  = g_ViewportWidth;  // Match views to viewport size
	textureDesc.Height = g_ViewportHeight;
	textureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we will have to render every level)
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D10_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE; // Indicate we will use texture as render target, and pass it to shaders
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	if (FAILED( g_pd3dDevice->CreateTexture2D( &textureDesc, NULL, &LeftTexture ) )) return false;
	if (FAILED( g_pd3dDevice->CreateTexture2D( &textureDesc, NULL, &RightTexture ) )) return false;

	// Now get "views" of the textures as render targets - giving us an interface for rendering to the texture
	if (FAILED( g_pd3dDevice->CreateRenderTargetView( LeftTexture, NULL, &LeftRenderTarget ) )) return false;
	if (FAILED( g_pd3dDevice->CreateRenderTargetView( RightTexture, NULL, &RightRenderTarget ) )) return false;

	// And shader-resource "view" - giving us an interface for passing texture to shaders
	D3D10_SHADER_RESOURCE_VIEW_DESC srDesc;
	srDesc.Format = textureDesc.Format;
	srDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED( g_pd3dDevice->CreateShaderResourceView( LeftTexture, &srDesc, &LeftShaderResource ) )) return false;
	if (FAILED( g_pd3dDevice->CreateShaderResourceView( RightTexture, &srDesc, &RightShaderResource ) )) return false;

	//***************************************************//

	return true;
}


// Update the scene - move/rotate each model and the camera, then update their matrices
void UpdateScene( float frameTime )
{
	// Control camera position and update its matrices (monoscopic version)
	MainCamera->Control( frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D );
	MainCamera->UpdateMatrices();
	
	// Control cube position and update its world matrix each frame
	Cube->Control( frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma );
	Cube->UpdateMatrix();

	// Update the orbiting light - a bit of a cheat with the static variable
	static float Rotate = 0.0f;
	Light1->SetPosition( Cube->GetPosition() + D3DXVECTOR3(cos(Rotate)*LightOrbitRadius, 0, sin(Rotate)*LightOrbitRadius) );
	Rotate -= LightOrbitSpeed * frameTime;
	Light1->UpdateMatrix();

	// Objects that don't move still need a world matrix - could do this in SceneSetup function (which occurs once at the start of the app)
	Stars->UpdateMatrix();
	Crate->UpdateMatrix();
	Ground->UpdateMatrix();
	Light2->UpdateMatrix();

	// Change the interocular distance on Page-up and Page-down
	if (KeyHeld(Key_Next))
	{
		Interocular += 0.6f * frameTime;
	}
	if (KeyHeld(Key_Prior))
	{
		Interocular -= 0.6f * frameTime;
	}
}


//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Render all the models from the point of view of the given camera
void RenderModels( CCamera* camera, EStereoscopic stereo /*= Monoscopic*/, float interocular /*=0.65*/ )
{
	// Pass the camera's matrices to the vertex shader and position to the vertex shader
	ViewMatrixVar->SetMatrix( (float*)&camera->GetViewMatrix( stereo, interocular ) );
	ProjMatrixVar->SetMatrix( (float*)&camera->GetProjectionMatrix( stereo, interocular ) );
	CameraPosVar->SetRawValue( camera->GetPosition( stereo, interocular ), 0, 12 );


	// Render cube
	WorldMatrixVar->SetMatrix( (float*)Cube->GetWorldMatrix() );  // Send the cube's world matrix to the shader
    DiffuseMapVar->SetResource( CubeDiffuseMap );                 // Send the cube's diffuse/specular map to the shader
	Cube->Render( VertexLitTexTechnique );                        // Pass rendering technique to the model class

	// Same for the other models in the scene
	WorldMatrixVar->SetMatrix( (float*)Crate->GetWorldMatrix() );
    DiffuseMapVar->SetResource( CrateDiffuseMap );
	Crate->Render( VertexLitTexTechnique );

	WorldMatrixVar->SetMatrix( (float*)Ground->GetWorldMatrix() );
    DiffuseMapVar->SetResource( GroundDiffuseMap );
	Ground->Render( VertexLitTexTechnique );

	WorldMatrixVar->SetMatrix( (float*)Stars->GetWorldMatrix() );
    DiffuseMapVar->SetResource( StarsDiffuseMap );
	Stars->Render( VertexLitTexTechnique );

	WorldMatrixVar->SetMatrix( (float*)Light1->GetWorldMatrix() );
    DiffuseMapVar->SetResource( LightDiffuseMap );
	TintColourVar->SetRawValue( Light1Colour, 0, 12 ); // Using special shader that tints the light model to match the light colour
	Light1->Render( AdditiveTexTintTechnique );

	WorldMatrixVar->SetMatrix( (float*)Light2->GetWorldMatrix() );
    DiffuseMapVar->SetResource( LightDiffuseMap );
	TintColourVar->SetRawValue( Light2Colour, 0, 12 );
	Light2->Render( AdditiveTexTintTechnique );
}


// Render everything in the scene
void RenderScene()
{
	//---------------------------
	// Common rendering settings

	// There are some common features all models that we will be rendering, set these once only
	//**|3D|** Camera settings are different per-eye so not set as here (as they might be in the monoscopic case) ****//

	// Pass light information to the vertex shader - lights are the same for each model *** and every render target ***
	Light1PosVar->    SetRawValue( Light1->GetPosition(), 0, 12 );  // Send 3 floats (12 bytes) from C++ LightPos variable (x,y,z) to shader counterpart (middle parameter is unused) 
	Light1ColourVar-> SetRawValue( Light1Colour, 0, 12 );
	Light2PosVar->    SetRawValue( Light2->GetPosition(), 0, 12 );
	Light2ColourVar-> SetRawValue( Light2Colour, 0, 12 );
	AmbientColourVar->SetRawValue( AmbientColour, 0, 12 );
	SpecularPowerVar->SetFloat( SpecularPower );

	// Setup the viewport - defines which part of the back-buffer we will render to (usually all of it)
	D3D10_VIEWPORT vp;
	vp.Width  = g_ViewportWidth;
	vp.Height = g_ViewportHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );


	//**|3D|****************************************//
	// Render left and right images of scene

	// Select the texture to use for rendering to, will share the depth/stencil buffer with the backbuffer though
	g_pd3dDevice->OMSetRenderTargets( 1, &LeftRenderTarget, DepthStencilView );

	// Clear the texture and the depth buffer
	g_pd3dDevice->ClearRenderTargetView( LeftRenderTarget, &BackgroundColour[0] );
	g_pd3dDevice->ClearDepthStencilView( DepthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0 );

	// Render everything from the left camera's point of view
	RenderModels( MainCamera, StereoscopicLeft, Interocular );


	vp.TopLeftX = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );

	// Same again for right view
	g_pd3dDevice->OMSetRenderTargets( 1, &RightRenderTarget, DepthStencilView );
	g_pd3dDevice->ClearRenderTargetView( RightRenderTarget, &BackgroundColour[0] );
	g_pd3dDevice->ClearDepthStencilView( DepthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0 );
	RenderModels( MainCamera, StereoscopicRight, Interocular );

	
	//***********************************//
	// Create Anaglyph on Back Buffer

	// Render full-screen quad over back-buffer using analglyph pixel shader to combine left and right views

	// Select the back buffer to use for rendering (ignore depth-buffer for full-screen quad) and select left and right views for use in shader
	g_pd3dDevice->OMSetRenderTargets( 1, &BackBufferRenderTarget, DepthStencilView );
	LeftViewVar->SetResource( LeftShaderResource );
	RightViewVar->SetResource( RightShaderResource );

	// Using special vertex shader than creates its own data (see .fx file). No need to set vertex/index buffer, just draw 4 vertices of quad
	g_pd3dDevice->IASetInputLayout( NULL );
	g_pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	ID3D10EffectTechnique * pTech = NULL;
	AnaglyphTechnique->GetPassByIndex(0)->Apply(0);
	g_pd3dDevice->Draw( 4, 0 );

	//***********************************//


	//---------------------------
	// Display the Scene

	// After we've finished drawing to the off-screen back buffer, we "present" it to the front buffer (the screen)
	SwapChain->Present( 0, 0 );
}



////////////////////////////////////////////////////////////////////////////////////////
// Window Setup
////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	// Initialise everything in turn
	if( !InitWindow( hInstance, nCmdShow) )
	{
		return 0;
	}
	if( !InitDevice() || !LoadEffectFile() || !InitScene() )
	{
		ReleaseResources(); // Cleanup DirectX on failure
		return 0;
	}

	// Initialise simple input functions (in Input.h/.cpp, not part of DirectX)
	InitInput();

	// Initialise a timer class (in CTimer.h/.cpp, not part of DirectX). It's like a stopwatch - start it counting now
	CTimer Timer;
	Timer.Start();

	// Main message loop. The window will stay in this loop until it is closed
	MSG msg = {0};
	while( WM_QUIT != msg.message )
	{
		// First check to see if there are any messages that need to be processed for the window (window resizing, minimizing, whatever), if there are then deal with them
		// If not then the window is idle and the D3D rendering occurs
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			// Deal with messages
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else // Otherwise render & update
		{
			RenderScene();

			// Get the time passed since the last frame (since the last time this line was reached) - used to synchronise update to realtime rather than machine speed
			float frameTime = Timer.GetLapTime();
			UpdateScene( frameTime );

			// Allow user to quit with escape key
			if (KeyHit( Key_Escape )) 
			{
				DestroyWindow( HWnd );
			}

		}
	}

	// Release all the resources we've created before leaving
	ReleaseResources();

	return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
bool InitWindow( HINSTANCE hInstance, int nCmdShow )
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof( WNDCLASSEX );
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
	wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
	if( !RegisterClassEx( &wcex ) )	return false;

	// Create window
	HInst = hInstance;
	RECT rc = { 0, 0, 1280, 960 };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	HWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 10: Stereoscopic Rendering", WS_OVERLAPPEDWINDOW,
	                     CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL );
	if( !HWnd )	return false;

	ShowWindow( HWnd, nCmdShow );

	return true;
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch( message )
	{
		case WM_PAINT:
			hdc = BeginPaint( hWnd, &ps );
			EndPaint( hWnd, &ps );
			break;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;

		// These windows messages (WM_KEYXXXX) can be used to get keyboard input to the window
		// This application has added some simple functions (not DirectX) to process these messages (all in Input.cpp/h)
		case WM_KEYDOWN:
			KeyDownEvent( static_cast<EKeyState>(wParam) );
			break;

		case WM_KEYUP:
			KeyUpEvent( static_cast<EKeyState>(wParam) );
			break;
		
		default:
			return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}
