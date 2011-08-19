/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * Main.cpp
 * 
 * Main program driver for the SDL port.
 *
 * Compile-Time Options
 * --------------------
 * - SUPERMODEL_SOUND: Enables experimental sound code. The 68K core (Turbo68K)
 *   only works on x86 (32-bit) systems, so this cannot be enabled on 64-bit
 *   builds.
 * - SUPERMODEL_WIN32: Define this if compiling on Windows.
 * - SUPERMODEL_OSX: Define this if compiling on Mac OS X.
 *
 * TO-DO List
 * ----------
 * - A lot of this code is actually OS-independent! Should it be moved into the
 *   root of the source tree? Might not be worth it; eventually, OS-dependent
 *	 UIs will be introduced.
 */
 
#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "Pkgs/glew.h"
#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include "Supermodel.h"
#include "SDLInputSystem.h"
#ifdef SUPERMODEL_WIN32
#include "DirectInputSystem.h"
#endif


/******************************************************************************
 Error and Debug Logging
******************************************************************************/

// Log file names
#define DEBUG_LOG_FILE	"debug.log"
#define ERROR_LOG_FILE	"error.log"

// Logger object is used to redirect log messages appropriately
static CLogger *s_Logger = NULL;

CLogger *GetLogger()
{
	return s_Logger;
}

void SetLogger(CLogger *Logger)
{
	s_Logger = Logger;
}

void DebugLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->DebugLog(fmt, vl);
	va_end(vl);
}

void InfoLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->InfoLog(fmt, vl);
	va_end(vl);
}

BOOL ErrorLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return FAIL;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->ErrorLog(fmt, vl);
	va_end(vl);
	return FAIL;
}


/******************************************************************************
 Display Management
******************************************************************************/

/*
 * Position and size of rectangular region within OpenGL display to render to.
 * Unlike the g_Config object, these end up containing the actual resolution 
 * (and computed offsets within the viewport) that will be rendered based on
 * what was obtained from SDL.
 */
unsigned	xOffset, yOffset;	// offset of renderer output within OpenGL viewport
unsigned 	xRes, yRes;			// renderer output resolution (can be smaller than GL viewport)

/*
 * CreateGLScreen():
 *
 * Creates an OpenGL display surface of the requested size. xOffset and yOffset
 * are used to return a display surface offset (for OpenGL viewport commands)
 * because the actual drawing area may need to be adjusted to preserve the 
 * Model 3 aspect ratio. The new resolution will be passed back as well.
 */
static BOOL CreateGLScreen(const char *caption, unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr,
						   BOOL keepAspectRatio, BOOL fullScreen)
{
	const SDL_VideoInfo	*VideoInfo;
	GLenum				err;
	float				model3Ratio, ratio;
	float				xRes, yRes;
	
	// Initialize video subsystem
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
    	return ErrorLog("Unable to initialize SDL video subsystem: %s\n", SDL_GetError());
    
    // Important GL attributes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);	// need at least RGB555 for Model 3 textures
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  	// Set video mode
  	if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  	{
    	ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    	return FAIL;
  	}
  	
  	VideoInfo = SDL_GetVideoInfo();	// what resolution did we actually get?
  	
  	// If required, fix the aspect ratio of the resolution that the user passed to match Model 3 ratio
  	xRes = (float) *xResPtr;
  	yRes = (float) *yResPtr;
  	if (keepAspectRatio)
	{
		model3Ratio = 496.0f/384.0f;
  		ratio = xRes/yRes;
  		if (yRes < (xRes/model3Ratio))
  			xRes = yRes*model3Ratio;
  		if (xRes < (yRes*model3Ratio))
			yRes = xRes/model3Ratio;
	}
		
	// Center the visible area 
  	*xOffsetPtr = (*xResPtr - (unsigned) xRes)/2;
  	*yOffsetPtr = (*yResPtr - (unsigned) yRes)/2;
  	
  	// If the desired resolution is smaller than what we got, re-center again
  	if (*xResPtr < VideoInfo->current_w)
  		*xOffsetPtr += (VideoInfo->current_w - *xResPtr)/2;
  	if (*yResPtr < VideoInfo->current_h)
  		*yOffsetPtr += (VideoInfo->current_h - *yResPtr)/2;
  	
  	// Create window caption
  	SDL_WM_SetCaption(caption,NULL);
  	
  	// Initialize GLEW, allowing us to use features beyond OpenGL 1.2
	err = glewInit();
	if (GLEW_OK != err)
	{
		ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
		return FAIL;
	}
  	
  	// OpenGL initialization
 	glViewport(0,0,*xResPtr,*yResPtr);
 	glClearColor(0.0,0.0,0.0,0.0);
 	glClearDepth(1.0);
 	glDepthFunc(GL_LESS);
 	glEnable(GL_DEPTH_TEST);
 	glShadeModel(GL_SMOOTH);
 	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
 	glDisable(GL_CULL_FACE);
 	glMatrixMode(GL_PROJECTION);
 	glLoadIdentity();
 	gluPerspective(90.0,(GLfloat)xRes/(GLfloat)yRes,0.1,1e5);
 	glMatrixMode(GL_MODELVIEW);
 	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);	// clear at least once to ensure black border
 	
 	// Write back resolution parameters
 	*xResPtr = (unsigned) xRes;
 	*yResPtr = (unsigned) yRes;
 	 	
	return 0;
}

	
/******************************************************************************
 Configuration
 
 Configuration file management and input settings.
******************************************************************************/

#define CONFIG_FILE_PATH	"Config/Supermodel.ini"
#define CONFIG_FILE_COMMENT	";\n" \
							"; Supermodel Configuration File\n" \
							";\n"

// Create and configure inputs
static bool ConfigureInputs(CInputs *Inputs, bool configure)
{
	// Open and parse configuration file
	CINIFile INI;
	INI.Open(CONFIG_FILE_PATH);	// doesn't matter if it exists or not, will get overwritten
	INI.SetDefaultSectionName("Global");
	INI.Parse();
	
	Inputs->ReadFromINIFile(&INI, "Global");
		
	// If the user wants to configure the inputs, do that now
	if (configure)
	{
		// Open an SDL window 
		unsigned xOffset, yOffset, xRes=496, yRes=384;
		if (OKAY != CreateGLScreen("Supermodel - Configuring Inputs...",&xOffset,&yOffset,&xRes,&yRes,FALSE,FALSE))
			return (bool) ErrorLog("Unable to start SDL to configure inputs.\n");
		
		// Configure the inputs
		if (Inputs->ConfigureInputs(NULL, xOffset, yOffset, xRes, yRes))
		{
			// Write input configuration and input system settings to config file
			Inputs->WriteToINIFile(&INI, "Global");
		
			if (OKAY != INI.Write(CONFIG_FILE_COMMENT))
				ErrorLog("Unable to save configuration to %s.", CONFIG_FILE_PATH);
			else
				printf("Configuration successfully saved to %s.\n", CONFIG_FILE_PATH);
		}
		else
			puts("Configuration aborted...");
		puts("");
	}
	
	INI.Close();
	return OKAY;
}

// Apply configuration settings from configuration file
static void ApplySettings(CInputs *Inputs, CINIFile *INI, const char *section)
{
	unsigned	x;
	string		String;
	
	// Model 3
	if (OKAY == INI->Get(section, "MultiThreaded", x))
		g_Config.multiThreaded = x ? true : false;
	if (OKAY == INI->Get(section, "PowerPCFrequency", x))
		g_Config.SetPowerPCFrequency(x);
	
	// 3D renderer
	INI->Get(section, "VertexShader", g_Config.vertexShaderFile);
	INI->Get(section, "FragmentShader", g_Config.fragmentShaderFile);
	
	// SCSP and DSB
	if (OKAY == INI->Get(section, "SoundVolume", x))
		g_Config.SetSoundVolume(x);
	if (OKAY == INI->Get(section, "MusicVolume", x))
		g_Config.SetMusicVolume(x);
	if (OKAY == INI->Get(section, "EmulateSCSP", x))
		g_Config.emulateSCSP = x ? true : false;
	if (OKAY == INI->Get(section, "EmulateDSB", x))
		g_Config.emulateDSB = x ? true : false;
	
	// OSD
	INI->Get(section, "XResolution", g_Config.xRes);
	INI->Get(section, "YResolution", g_Config.yRes);
	if (OKAY == INI->Get(section, "FullScreen", x))
		g_Config.fullScreen = x ? true : false;
	if (OKAY == INI->Get(section, "Throttle", x))
		g_Config.throttle = x ? true : false;
	if (OKAY == INI->Get(section, "ShowFrameRate", x))
		g_Config.showFPS = x ? true : false;
			
	// Inputs
	if (Inputs != NULL)
		Inputs->ReadFromINIFile(INI, section);
}

// Read settings (from a specific section) from the config file
static void ReadConfigFile(CInputs *Inputs, const char *section)
{
	CINIFile	INI;	
	
	INI.Open(CONFIG_FILE_PATH);
	INI.SetDefaultSectionName("Global");	// required to read settings not associated with a specific section
	INI.Parse();
	ApplySettings(Inputs, &INI, section);
	INI.Close();
}

// Debug
static void DumpConfig(void)
{
	printf("MultiThreaded    = %d\n", g_Config.multiThreaded);
	printf("PowerPCFrequency = %d\n", g_Config.GetPowerPCFrequency());
	printf("EmulateSCSP      = %d\n", g_Config.emulateSCSP);
	printf("EmulateDSB       = %d\n", g_Config.emulateDSB);
	printf("VertexShader     = %s\n", g_Config.vertexShaderFile.c_str());
	printf("FragmentShader   = %s\n", g_Config.fragmentShaderFile.c_str());
	printf("XResolution      = %d\n", g_Config.xRes);
	printf("YResolution      = %d\n", g_Config.yRes);
	printf("FullScreen       = %d\n", g_Config.fullScreen);
	printf("Throttle         = %d\n", g_Config.throttle);
	printf("ShowFrameRate    = %d\n", g_Config.showFPS);
	printf("InputSystem      = %s\n", g_Config.GetInputSystem());
	printf("\n");
}


/******************************************************************************
 Save States and NVRAM
 
 Save states and NVRAM use the same basic format. When anything changes that
 breaks compatibility with previous versions of Supermodel, the save state
 and NVRAM version numbers must be incremented as needed.
 
 Header block name: "Supermodel Save State" or "Supermodel NVRAM State"
 Data: Save state file version (4-byte integer), ROM set ID (up to 9 bytes, 
 including terminating \0).
 
 Different subsystems output their own blocks.
******************************************************************************/

#define STATE_FILE_VERSION	1	// save state file version
#define NVRAM_FILE_VERSION	0	// NVRAM file version

static unsigned	saveSlot = 0;	// save state slot #

static void SaveState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion = STATE_FILE_VERSION;
	
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	if (OKAY != SaveState.Create(filePath, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save state to %s.", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	SaveState.Write(&fileVersion, sizeof(fileVersion));
	SaveState.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save state
	Model3->SaveState(&SaveState);
	SaveState.Close();
	printf("Saved state to %s.\n", filePath);
	DebugLog("Saved state to %s.\n", filePath);
}

static void LoadState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	
	// Open and check to make sure format is correct
	if (OKAY != SaveState.Load(filePath))
	{
		ErrorLog("Unable to load state from %s.", filePath);
		return;
	}
	
	if (OKAY != SaveState.FindBlock("Supermodel Save State"))
	{
		ErrorLog("%s does not appear to be a valid save state file.", filePath);
		return;
	}
	
	SaveState.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != STATE_FILE_VERSION)
	{
		ErrorLog("%s is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadState(&SaveState);
	SaveState.Close();
	printf("Loaded state from %s.\n", filePath);
	DebugLog("Loaded state from %s.\n", filePath);
}

static void SaveNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion = NVRAM_FILE_VERSION;
	
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	if (OKAY != NVRAM.Create(filePath, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save NVRAM to %s. Make sure directory exists!", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	NVRAM.Write(&fileVersion, sizeof(fileVersion));
	NVRAM.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save NVRAM
	Model3->SaveNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Saved NVRAM to %s.\n", filePath);
}

static void LoadNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	
	// Open and check to make sure format is correct
	if (OKAY != NVRAM.Load(filePath))
	{
		//ErrorLog("Unable to restore NVRAM from %s.", filePath);
		return;
	}
	
	if (OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
	{
		ErrorLog("%s does not appear to be a valid NVRAM file.", filePath);
		return;
	}
	
	NVRAM.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != NVRAM_FILE_VERSION)
	{
		ErrorLog("%s is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Loaded NVRAM from %s.\n", filePath);
}


/******************************************************************************
 Main Program Driver
 
 All configuration management is done prior to calling Supermodel().
******************************************************************************/

#ifdef SUPERMODEL_DEBUGGER
int Supermodel(const char *zipFile, CModel3 *Model3, CInputs *Inputs, Debugger::CDebugger *Debugger, CINIFile *CmdLine)
{
CLogger *oldLogger;
#else
int Supermodel(const char *zipFile, CInputs *Inputs, CINIFile *CmdLine)
{				  
	CModel3			*Model3 = new CModel3();
#endif // SUPERMODEL_DEBUGGER
	char			titleStr[128], titleFPSStr[128];
	CRender2D		*Render2D = new CRender2D();
	CRender3D		*Render3D = new CRender3D();
	unsigned		prevFPSTicks, currentFPSTicks, currentTicks, targetTicks, startTicks;
	unsigned		fpsFramesElapsed, framesElapsed;
	BOOL			showCursor = FALSE;	// show cursor in full screen mode?
	BOOL			quit = 0;
	BOOL            paused = 0;
	
	// Info log user options
	InfoLog("PowerPC frequency: %d Hz", g_Config.GetPowerPCFrequency()); 	
	InfoLog("Resolution requested: %dx%d (%s)", g_Config.xRes, g_Config.yRes, g_Config.fullScreen?"full screen":"windowed");
	InfoLog("Frame rate limiting: %s", g_Config.throttle?"Enabled":"Disabled");

	// Initialize and load ROMs
	if (OKAY != Model3->Init())
		return 1;
	if (OKAY != Model3->LoadROMSet(Model3GameList, zipFile))
		return 1;
		
	// Apply game-specific settings and then, lastly, command line settings
	ReadConfigFile(Inputs, Model3->GetGameInfo()->id);
	//DumpConfig();
	ApplySettings(Inputs, CmdLine, "Global");
	//DumpConfig();
		
	// Load NVRAM
	LoadNVRAM(Model3);
		
	// Start up SDL and open a GL window
  	xRes = g_Config.xRes;
  	yRes = g_Config.yRes;
  	sprintf(titleStr, "Supermodel - %s", Model3->GetGameInfo()->title);
	if (OKAY != CreateGLScreen(titleStr,&xOffset,&yOffset,&xRes,&yRes,TRUE,g_Config.fullScreen))
		return 1;
	
	// Initialize audio system
	if (OKAY != OpenAudio())
		return 1;

	// Hide mouse if fullscreen
	Inputs->GetInputSystem()->SetMouseVisibility(!g_Config.fullScreen);

	// Attach the inputs to the emulator
	Model3->AttachInputs(Inputs);
	
	// Initialize the renderer
	if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes))
		goto QuitError;
	if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes))
		goto QuitError;
	Model3->AttachRenderers(Render2D,Render3D);

	// Reset emulator
	Model3->Reset();
	
#ifdef SUPERMODEL_DEBUGGER
	// If debugger was supplied, set it as logger and attach it to system
	oldLogger = GetLogger();
	if (Debugger != NULL)
	{
		SetLogger(Debugger);
		Debugger->Attach();
	}
#endif // SUPERMODEL_DEBUGGER

	// Emulate!
	fpsFramesElapsed = 0;
	framesElapsed = 0;
	prevFPSTicks = SDL_GetTicks();
	startTicks = prevFPSTicks;
	while (!quit)
	{
		// Check if paused
		if (!paused)
		{
			// If not, run one frame
			Model3->RunFrame();
		
			// Swap the buffers
			SDL_GL_SwapBuffers();
		}
		
		// Poll the inputs
		if (!Inputs->Poll(Model3->GetGameInfo(), xOffset, yOffset, xRes, yRes))
			quit = 1;
		
#ifdef SUPERMODEL_DEBUGGER
		if (Debugger != NULL)
		{
			Debugger->Poll();

			// Check if debugger requests exit or pause
			if (Debugger->CheckExit())
				quit = 1;
			else if (Debugger->CheckPause())	
				paused = 1;
			else
			{
#endif // SUPERMODEL_DEBUGGER

		// Check UI controls
		if (Inputs->uiExit->Pressed())
 		{
			// Quit emulator
 			quit = 1; 			
		}
		else if (Inputs->uiReset->Pressed())
 		{
			// Reset emulator
			Model3->Reset();
			
#ifdef SUPERMODEL_DEBUGGER
			// If debugger was supplied, reset it too
			if (Debugger != NULL)
				Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER

			printf("Model 3 reset.\n");
		}
		else if (Inputs->uiPause->Pressed())
		{
			// Toggle emulator paused flag
			paused = !paused;
		}
		else if (Inputs->uiSaveState->Pressed())
		{
			// Save game state
 			SaveState(Model3);
		}
		else if (Inputs->uiChangeSlot->Pressed())
		{
			// Change save slot
 			++saveSlot;
 			saveSlot %= 10;	// clamp to [0,9]
 			printf("Save slot: %d\n", saveSlot);
		}
		else if (Inputs->uiLoadState->Pressed())
		{
			// Load game state
			LoadState(Model3);
						
#ifdef SUPERMODEL_DEBUGGER
			// If debugger was supplied, reset it after loading state
			if (Debugger != NULL)
				Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER
		}
		else if (Inputs->uiDumpInpState->Pressed())
		{
			// Dump input states
			Inputs->DumpState(Model3->GetGameInfo());
		}
		else if (Inputs->uiToggleCursor->Pressed() && g_Config.fullScreen)
 		{
			// Toggle cursor in full screen mode
			showCursor = !showCursor;
			Inputs->GetInputSystem()->SetMouseVisibility(!!showCursor);
 		}
		else if (Inputs->uiClearNVRAM->Pressed())
 		{
			// Clear NVRAM
 			Model3->ClearNVRAM();
 			printf("NVRAM cleared.\n");
 		}
		else if (Inputs->uiToggleFrLimit->Pressed())
 		{
			// Toggle frame limiting
 			g_Config.throttle = !g_Config.throttle;
 			printf("Frame limiting: %s\n", g_Config.throttle?"On":"Off");
 		}
#ifdef SUPERMODEL_DEBUGGER
				else if (Inputs->uiEnterDebugger->Pressed())
				{
					// Break execution and enter debugger
					Debugger->ForceBreak(true);
				}
			}
		}
#endif // SUPERMODEL_DEBUGGER
 		
 		// FPS and frame rate
 		currentFPSTicks = SDL_GetTicks();
 		currentTicks = currentFPSTicks;
 		
 		// FPS
 		if (g_Config.showFPS)
 		{
 			++fpsFramesElapsed;
			if((currentFPSTicks-prevFPSTicks) >= 1000)	// update FPS every 1 second (each tick is 1 ms)
			{
				sprintf(titleFPSStr, "%s - %1.1f FPS", titleStr, (float)fpsFramesElapsed*(float)(currentFPSTicks-prevFPSTicks)/1000.0f);
				SDL_WM_SetCaption(titleFPSStr,NULL);
				prevFPSTicks = currentFPSTicks;			// reset tick count
				fpsFramesElapsed = 0;					// reset frame count
			}
		}
		
		// Frame limiting/paused
		if (paused || g_Config.throttle)
		{
			++framesElapsed;
			targetTicks = startTicks + (unsigned) ((float)framesElapsed * 1000.0f/60.0f);
			if (currentTicks <= targetTicks)	// add a delay until we reach the next (target) frame time
				SDL_Delay(targetTicks-currentTicks);
			else								// begin a new frame
			{
				framesElapsed = 0;
				startTicks = currentTicks;
			}
		}
	}

#ifdef SUPERMODEL_DEBUGGER
	// If debugger was supplied, detach it from system and restore old logger
	if (Debugger != NULL)
	{
		Debugger->Detach();
		SetLogger(oldLogger);
	}
#endif // SUPERMODEL_DEBUGGER
	
	// Save NVRAM
	SaveNVRAM(Model3);
	
	// Close audio
	CloseAudio();

	// Shut down
#ifndef SUPERMODEL_DEBUGGER
	delete Model3;
#endif // SUPERMODEL_DEBUGGER
	delete Render2D;
	delete Render3D;
	
	// Dump PowerPC registers
#ifdef DEBUG
	for (int i = 0; i < 32; i += 4)
        printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
        i + 0, ppc_get_gpr(i + 0),
        i + 1, ppc_get_gpr(i + 1),
        i + 2, ppc_get_gpr(i + 2),
        i + 3, ppc_get_gpr(i + 3));
	printf("PC =%08X\n", ppc_get_pc());
  	printf("LR =%08X\n", ppc_get_lr());
	/*
	printf("DBAT0U=%08X\tIBAT0U=%08X\n", ppc_read_spr(SPR603E_DBAT0U), ppc_read_spr(SPR603E_IBAT0U));
	printf("DBAT0L=%08X\tIBAT0L=%08X\n", ppc_read_spr(SPR603E_DBAT0L), ppc_read_spr(SPR603E_IBAT0L));
	printf("DBAT1U=%08X\tIBAT1U=%08X\n", ppc_read_spr(SPR603E_DBAT1U), ppc_read_spr(SPR603E_IBAT1U));
	printf("DBAT1L=%08X\tIBAT1L=%08X\n", ppc_read_spr(SPR603E_DBAT1L), ppc_read_spr(SPR603E_IBAT1L));
	printf("DBAT2U=%08X\tIBAT2U=%08X\n", ppc_read_spr(SPR603E_DBAT2U), ppc_read_spr(SPR603E_IBAT2U));
	printf("DBAT2L=%08X\tIBAT2L=%08X\n", ppc_read_spr(SPR603E_DBAT2L), ppc_read_spr(SPR603E_IBAT2L));
	printf("DBAT3U=%08X\tIBAT3U=%08X\n", ppc_read_spr(SPR603E_DBAT3U), ppc_read_spr(SPR603E_IBAT3U));
	printf("DBAT3L=%08X\tIBAT3L=%08X\n", ppc_read_spr(SPR603E_DBAT3L), ppc_read_spr(SPR603E_IBAT3L));
	for (int i = 0; i < 10; i++)
		printf("SR%d =%08X\n", i, ppc_read_sr(i));
	for (int i = 10; i < 16; i++)
		printf("SR%d=%08X\n", i, ppc_read_sr(i));
	printf("SDR1=%08X\n", ppc_read_spr(SPR603E_SDR1));
	*/
#endif
	
	return 0;

	// Quit with an error
QuitError:
#ifndef SUPERMODEL_DEBUGGER
	delete Model3;
#endif // SUPERMODEL_DEBUGGER
	delete Render2D;
	delete Render3D;
	return 1;
}


/******************************************************************************
 Diagnostic Commands
******************************************************************************/

// Disassemble instructions from CROM
static int DisassembleCROM(const char *zipFile, UINT32 addr, unsigned n)
{
	const struct GameInfo	*Game;
	UINT8					*crom;
	struct ROMMap Map[] =
	{
		{ "CROM", 	NULL },
		{ "CROMxx",	NULL },
		{ NULL, 	NULL }
	};
	char	mnem[16], oprs[48];
	UINT32	op;
	
	// Do we have a valid CROM address?
	if (addr < 0xFF800000)
		return ErrorLog("Valid CROM address range is FF800000-FFFFFFFF.");
		
	// Allocate memory and set ROM region
	crom = new(std::nothrow) UINT8[0x8800000];
	if (NULL == crom)
		return ErrorLog("Insufficient memory to load CROM (need %d MB).", (0x8800000/8));
	Map[0].ptr = crom;
	Map[1].ptr = &crom[0x800000];
	
	// Load ROM set
	Game = LoadROMSetFromZIPFile(Map, Model3GameList, zipFile, FALSE);
	if (NULL == Game)
		return ErrorLog("Failed to load ROM set.");	
		
	// Mirror CROM if necessary
	if (Game->cromSize < 0x800000)	// high part of fixed CROM region contains CROM0
		CopyRegion(crom, 0, 0x800000-0x200000, &crom[0x800000], 0x800000);
		
	// Disassemble!
	addr -= 0xFF800000;
	while ((n > 0) && ((addr+4) <= 0x800000))
	{
		op = (crom[addr+0]<<24) | (crom[addr+1]<<16) | (crom[addr+2]<<8) | crom[addr+3];
        
        printf("%08X: ", addr+0xFF800000);
        if (DisassemblePowerPC(op, addr+0xFF800000, mnem, oprs, 1))
        {
            if (mnem[0] != '\0')    // invalid form
                printf("%08X %s*\t%s\n", op, mnem, oprs);
            else
                printf("%08X ?\n", op);
        }
        else
            printf("%08X %s\t%s\n", op, mnem, oprs);
		
		addr += 4;
		--n;
	}
	
	delete [] crom;
	return OKAY;
}

/*
 * PrintGLInfo():
 *
 * Queries and prints OpenGL information. A full list of extensions can
 * optionally be printed.
 */
static void PrintGLInfo(BOOL printExtensions)
{
	const GLubyte	*str;
	char			*strLocal;
	GLint			value;
	unsigned		xOffset, yOffset, xRes=496, yRes=384;
	
	if (OKAY != CreateGLScreen("Supermodel - Querying OpenGL Information...",&xOffset,&yOffset,&xRes,&yRes,FALSE,FALSE))
	{
		ErrorLog("Unable to query OpenGL.\n");
		return;
	}
	
	puts("OpenGL information:\n");

	str = glGetString(GL_VENDOR);
	printf("                   Vendor: %s\n", str);
	
	str = glGetString(GL_RENDERER);
	printf("                 Renderer: %s\n", str);
	
	str = glGetString(GL_VERSION);
	printf("                  Version: %s\n", str);
	
	str = glGetString(GL_SHADING_LANGUAGE_VERSION);
	printf(" Shading Language Version: %s\n", str);
	
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
	printf("Maximum Vertex Array Size: %d vertices\n", value);
	
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
	printf("     Maximum Texture Size: %d texels\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
	printf("Maximum Vertex Attributes: %d\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
	printf("  Maximum Vertex Uniforms: %d\n", value);
	
	if (printExtensions)
	{
		str = glGetString(GL_EXTENSIONS);
		strLocal = (char *) malloc((strlen((char *) str)+1)*sizeof(char));
		if (NULL == strLocal)
			printf("     Supported Extensions: %s\n", str);
		else
		{
			strcpy(strLocal, (char *) str);
			printf("     Supported Extensions: %s\n", (strLocal = strtok(strLocal, " \t\n")));
			while ((strLocal = strtok(NULL, " \t\n")) != NULL)
				printf("                           %s\n", strLocal);
		}
	}
	
	printf("\n");
}


/******************************************************************************
 Entry Point and Command Line Procesing
******************************************************************************/

// Print Supermodel title and copyright information
static void Title(void)
{
	puts("Supermodel: A Sega Model 3 Arcade Emulator (Version "SUPERMODEL_VERSION")");
	puts("Copyright (C) 2011 by Bart Trzynadlowski");
	puts("");
}

// Print usage information
static void Help(void)
{
	puts("Usage: Supermodel <romset> [options]");
	puts("ROM set must be a valid ZIP file containing a single game.");
	puts("");
	puts("General Options:");
	puts("    -?, -h                 Print this help text");
	puts("    -print-games           List supported games and quit");
	puts("");
	puts("Emulation Options:");
	puts("    -ppc-frequency=<f>     Set PowerPC frequency in MHz [Default: 40]");
	puts("    -no-scsp               Disable Sega Custom Sound Processor (sound effects)");
	puts("    -no-dsb                Disable Digital Sound Board (MPEG music)");
	puts("    -multi-threaded        Enable multi-threading");
#ifdef SUPERMODEL_DEBUGGER
	puts("    -disable-debugger	     Completely disable debugger functionality");
	puts("    -enter-debugger        Enter debugger at start of emulation");
#endif // SUPERMODEL_DEBUGGER
	puts("");
	puts("Video Options:");
	puts("    -res=<x>,<y>           Resolution");
	puts("    -fullscreen            Full screen mode");
	puts("    -no-throttle           Disable 60 Hz frame rate limit");
	puts("    -show-fps              Display frame rate in window title bar");
#ifdef DEBUG	// ordinary users do not need to know about these, but they are always available
	puts("    -vert-shader=<file>    Load 3D vertex shader from external file");
	puts("    -frag-shader=<file>    Load 3D fragment shader from external file");
#endif
	puts("");
	puts("Audio Options:");
	puts("    -sound-volume=<v>      Set volume of sound effects in % [Default: 100]");
	puts("    -music-volume=<v>      Set volume of MPEG music in % [Default: 100]");
	puts("");
	puts("Input Options:");
	puts("    -input-system=<s>      Set input system [Default: SDL]");
	puts("    -print-inputs          Prints current input configuration");
	puts("    -config-inputs         Configure inputs for keyboards, mice and joysticks");
	puts("");
	puts("Diagnostic Options:");
#ifdef DEBUG
	puts("    -dis=<addr>[,n]        Disassemble PowerPC code from CROM");
#endif
	puts("    -print-gl-info         Print extensive OpenGL information and quit\n");
}

// Print game list
static void PrintGameList(void)
{
	int	i, j;
	
	puts("Supported games:");
	puts("");
	puts("    ROM Set         Title");
	puts("    -------         -----");
	for (i = 0; Model3GameList[i].title != NULL; i++)
	{
		printf("    %s", Model3GameList[i].id);
		for (j = strlen(Model3GameList[i].id); j < 9; j++)	// pad for alignment (no game ID is more than 9 letters)
			printf(" ");
		printf("       %s\n", Model3GameList[i].title);
	}
}

/*
 * main(argc, argv):
 *
 * Program entry point.
 */
int main(int argc, char **argv)
{
	CINIFile	CmdLine;	// not associated with any files, holds command line options
	int			i, ret;
	int			fileIdx=0;
	bool		cmdPrintInputs=false, cmdConfigInputs=false, cmdDis=false;
#ifdef SUPERMODEL_DEBUGGER
	int			cmdEnterDebugger=false;
#endif // SUPERMODEL_DEBUGGER
	unsigned	n;
	UINT32		addr;

	Title();
	if (argc <= 1)
	{
		Help();
		return 0;
	}

	// Create default logger
	CFileLogger Logger(DEBUG_LOG_FILE, ERROR_LOG_FILE);
	Logger.ClearLogs();
	SetLogger(&Logger);
	
	// Read global settings from INI file
	ReadConfigFile(NULL, "Global");
	//DumpConfig();

	/*
	 * Parse command line. 
	 *
	 * Settings are stored in CmdLine so that they can be applied later, after
	 * game-specific settings are read from the configuration file (which
	 * requires the ROM set to be identified and therefore is done later).
	 *
	 * Some commands are processed here directly.
	 */
	CmdLine.SetDefaultSectionName("Global");	// command line settings are global-level
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"-?"))
		{
			Help();
			return 0;
		}
		else if (!strcmp(argv[i],"-print-games"))
		{
			PrintGameList();
			return 0;
		}
		else if (!strncmp(argv[i],"-ppc-frequency",14))
		{
			int	f;
			ret = sscanf(&argv[i][14],"=%d",&f);
			if (ret != 1)
				ErrorLog("-ppc-frequency requires a frequency.");
			else
				CmdLine.Set("Global", "PowerPCFrequency", f);
		}
		else if (!strcmp(argv[i],"-multi-threaded"))
		{
			n = 1;
			CmdLine.Set("Global", "MultiThreaded", n);
		}
#ifdef SUPERMODEL_DEBUGGER
		else if (!strncmp(argv[i],"-disable-debugger"))
			g_Config.disableDebugger = true;
		else if (!strcmp(argv[i],"-enter-debugger"))
			cmdEnterDebugger = true;
#endif // SUPERMODEL_DEBUGGER
		else if (!strncmp(argv[i],"-sound-vol",10))
		{
			ret = sscanf(&argv[i][10],"=%d",&n);
			if (ret != 1)
				ErrorLog("-sound-vol requires a volume setting.");
			else
				CmdLine.Set("Global", "SoundVolume", n);
		}
		else if (!strncmp(argv[i],"-music-vol",10))
		{
			ret = sscanf(&argv[i][10],"=%d",&n);
			if (ret != 1)
				ErrorLog("-music-vol requires a volume setting.");
			else
				CmdLine.Set("Global", "MusicVolume", n);
		}
		else if (!strcmp(argv[i], "-no-scsp"))
		{
			n = 0;
			CmdLine.Set("Global", "EmulateSCSP", n);
		}
		else if (!strcmp(argv[i], "-no-dsb"))
		{
			n = 0;
			CmdLine.Set("Global", "EmulateDSB", n);
		}
		else if (!strncmp(argv[i],"-res",4))
		{
			unsigned	x, y;
			
			ret = sscanf(&argv[i][4],"=%d,%d",&x,&y);
			if (ret != 2)
				ErrorLog("-res requires both a width and a height.");
			else
			{
				CmdLine.Set("Global", "XResolution", x);
				CmdLine.Set("Global", "YResolution", y);
			}
		}
		else if (!strcmp(argv[i],"-fullscreen"))
		{
			n = 1;
			CmdLine.Set("Global", "FullScreen", n);
		}
		else if (!strcmp(argv[i],"-no-throttle"))
		{
			n = 0;
			CmdLine.Set("Global", "Throttle", n);
		}
		else if (!strcmp(argv[i],"-show-fps"))
		{
			n = 1;
			CmdLine.Set("Global", "ShowFrameRate", n);
		}
		else if (!strncmp(argv[i],"-vert-shader=",13))
		{
			if (argv[i][13] == '\0')
				ErrorLog("-vert-shader requires a file path.");
			else
				CmdLine.Set("Global", "VertexShader", &argv[i][13]);
		}
		else if (!strncmp(argv[i],"-frag-shader=",13))
		{
			if (argv[i][13] == '\0')
				ErrorLog("-frag-shader requires a file path.");
			else
				CmdLine.Set("Global", "FragmentShader", &argv[i][13]);
		}
		else if (!strncmp(argv[i],"-input-system=", 14))
		{
			if (argv[i][14] == '\0')
				ErrorLog("-input-system requires an input system name.");
			else
				CmdLine.Set("Global", "InputSystem", &argv[i][14]);
		}
		else if (!strcmp(argv[i],"-print-inputs"))
			cmdPrintInputs = true;
		else if (!strcmp(argv[i],"-config-inputs"))
			cmdConfigInputs = true;
		else if (!strncmp(argv[i],"-dis",4))
		{
			ret = sscanf(&argv[i][4],"=%X,%X",&addr,&n);
			if (ret == 1)
			{
				n = 16;
				cmdDis = true;
			}
			else if (ret == 2)
				cmdDis = true;
			else
				ErrorLog("-dis requires address and, optionally, number of instructions.");
		}
		else if (!strcmp(argv[i],"-print-gl-info"))
		{
			PrintGLInfo(FALSE);
			return 0;
		}
		else if (argv[i][0] == '-')
			ErrorLog("Ignoring invalid option: %s.", argv[i]);
		else
		{
			if (fileIdx)		// already specified a file
 				ErrorLog("Multiple files specified. Using %s, ignoring %s.", argv[i], argv[fileIdx]);
 			else
 				fileIdx = i;
 		}
	}
	
	// Initialize SDL (individual subsystems get initialized later)
	if (SDL_Init(0) != 0)
	{
		ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	
	// Create input system (default is SDL) and debugger
	CInputSystem *InputSystem = NULL;
	CInputs *Inputs = NULL;
	int exitCode = 0;
#ifdef SUPERMODEL_DEBUGGER
	CModel3 *Model3 = NULL;
	Debugger::CSupermodelDebugger *Debugger = NULL;
#endif // SUPERMODEL_DEBUGGER

	if (stricmp(g_Config.GetInputSystem(), "sdl") == 0)
		InputSystem = new CSDLInputSystem();
#ifdef SUPERMODEL_WIN32
	else if (stricmp(g_Config.GetInputSystem(), "dinput") == 0)
		InputSystem = new CDirectInputSystem(false, false, false);
	else if (stricmp(g_Config.GetInputSystem(), "xinput") == 0)
		InputSystem = new CDirectInputSystem(false, true, false);
	else if (stricmp(g_Config.GetInputSystem(), "rawinput") == 0)
		InputSystem = new CDirectInputSystem(true, false, false);
#endif // SUPERMODEL_WIN32
	else
	{
		ErrorLog("Unknown input system: '%s'.\n", g_Config.GetInputSystem());
		exitCode = 1;
		goto Exit;
	}

	// Create inputs from input system (configuring them if required)
	Inputs = new CInputs(InputSystem);
	if (!Inputs->Initialize())
	{
		ErrorLog("Unable to initalize inputs.\n");
		exitCode = 1;
		goto Exit;
	}
	
	if (ConfigureInputs(Inputs, cmdConfigInputs))
	{
		exitCode = 1;
		goto Exit;
	}

	if (cmdPrintInputs)
	{
		Inputs->PrintInputs(NULL);
		InputSystem->PrintSettings();
	}
	
	// From this point onwards, a ROM set is needed
	if (fileIdx == 0)
	{
		ErrorLog("No ROM set specified.");
		exitCode = 1;
		goto Exit;
	}
	
	if (cmdDis)
	{
		if (OKAY != DisassembleCROM(argv[fileIdx], addr, n))
			exitCode = 1;
		goto Exit;
	}

#ifdef SUPERMODEL_DEBUGGER
	// Create Model3
	Model3 = new CModel3();
	// Create Supermodel debugger unless debugging is disabled
	if (!g_Config.disableDebugger)
	{
		Debugger = new Debugger::CSupermodelDebugger(Model3, Inputs, &Logger);
		// If -enter-debugger option was set force debugger to break straightaway
		if (cmdEnterDebugger)
			Debugger->ForceBreak(true);
	}
	// Fire up Supermodel with debugger
	exitCode = Supermodel(argv[fileIdx],Model3,Inputs,Debugger,&CmdLine);
	if (Debugger != NULL)
		delete Debugger;
	delete Model3;
#else
	// Fire up Supermodel
	exitCode = Supermodel(argv[fileIdx],Inputs,&CmdLine);
#endif // SUPERMODEL_DEBUGGER

Exit:
	if (Inputs != NULL)
		delete Inputs;
	if (InputSystem != NULL)
		delete InputSystem;
	SDL_Quit();
	return exitCode;
}