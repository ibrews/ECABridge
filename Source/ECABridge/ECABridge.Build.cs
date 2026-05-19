// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ECABridge : ModuleRules
{
	// Returns true if the engine ships the named plugin (search Engine/Plugins recursively
	// for <PluginName>.uplugin). Used to gate optional feature subsystems so that ECABridge
	// can be built against engines that don't have e.g. Mutable or MovieRenderPipeline.
	//
	// Note: this answers "is the plugin present in the engine on disk", which is the right
	// question for distinguishing a stock UE install from a custom engine fork that compiled
	// out a plugin. It does NOT answer "did the user enable this plugin in their .uproject"
	// — Target.Plugins isn't exposed on ReadOnlyTargetRules in 5.8, and reading the .uproject
	// from Build.cs is too fragile. Users who want to opt out of a subsystem despite the
	// engine having it can either disable the plugin in their .uproject (the .uplugin
	// references are "Optional": true so UE won't error) OR force-override the WITH_ECA_*
	// preprocessor symbols here.
	private bool EngineHasPlugin(string PluginName)
	{
		string PluginsDir = Path.Combine(EngineDirectory, "Plugins");
		if (!Directory.Exists(PluginsDir)) return false;
		return Directory.GetFiles(PluginsDir, PluginName + ".uplugin", SearchOption.AllDirectories).Length > 0;
	}

	public ECABridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp23;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "EngineSettings", "Json", "JsonUtilities",
			"HTTPServer",  // For MCP HTTP/SSE server
			"Sockets",     // For ISocketSubsystem
			"Networking"   // For networking utilities
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Editor core
			"UnrealEd",
			
			// MetaSound
			"MetasoundEngine", "MetasoundEditor", "MetasoundFrontend", "MetasoundGraphCore", "MetasoundStandardNodes", "AudioExtensions",
			
			// Mesh import/manipulation
			"MeshDescription", "StaticMeshDescription",
			// GeometryScript - Modern procedural mesh API
			"GeometryCore", "GeometryFramework", "GeometryScriptingCore", "DynamicMesh", "MeshConversion",
			// Modeling tools subdivision
			"ModelingComponentsEditorOnly", "ModelingOperators",
			// ProceduralMeshComponent - Runtime procedural meshes
			"ProceduralMeshComponent",
			
			// Niagara FX
			"Niagara", "NiagaraCore", "NiagaraEditor", "NiagaraShader",
			"EditorSubsystem", "EditorFramework", "LevelEditor",
			
			// UI/Slate
			"Slate", "SlateCore", "UMG", "UMGEditor", "PropertyEditor", "InputCore",
			// MVVM ViewModel binding
			"ModelViewViewModel", "ModelViewViewModelBlueprint", "ModelViewViewModelEditor",
			
			// Blueprint manipulation
			"BlueprintGraph", "Kismet", "KismetCompiler", "GraphEditor",
			
			// Asset management
			"AssetTools", "AssetRegistry", "ContentBrowser", "EditorScriptingUtilities",  // For EditorAssetLibrary
			"MaterialEditor",            // For material editing utilities
			"DataTableEditor",           // For DataTable editor UI refresh (BroadcastPostChange)
			
			// Additional utilities
			"Projects", "DeveloperSettings", "ToolMenus",
			
			// Image processing
			"ImageWrapper",
			
			// Interchange framework for FBX import
			"InterchangeCore",
			"InterchangeEngine",
			
			// Rendering (for thumbnail generation)
			"RenderCore",
			"RHI",

			// Level Sequencer
			"LevelSequence",
			"LevelSequenceEditor",
			"MovieScene",
			"MovieSceneTracks",
			"CinematicCamera",

			// Navigation / AI
			"NavigationSystem",
			"AIModule",

			// Landscape
			"Landscape",
			"LandscapeEditor",

			// Source Control (always available in editor)
			"SourceControl",

			// PCG (Procedural Content Generation) - requires PCG plugin
			"PCG",

			// Control Rig - requires ControlRig plugin
			"ControlRig",
			"ControlRigDeveloper",

			// Gameplay Ability System - requires GameplayAbilities plugin
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// MetaHuman Character - requires MetaHumanCharacter plugin
			"MetaHumanCharacter",
			"MetaHumanCharacterPalette",

			// Enhanced Input - requires EnhancedInput plugin
			"EnhancedInput"
		});

		// ModelViewViewModelBlueprint include path - UBT doesn't resolve it via PrivateDependencyModuleNames
		// when the plugin is EnabledByDefault=false, so add it explicitly.
		string MVVMBlueprintPath = System.IO.Path.Combine(EngineDirectory, "Plugins", "Runtime", "ModelViewViewModel", "Source", "ModelViewViewModelBlueprint", "Public");
		PrivateIncludePaths.Add(MVVMBlueprintPath);

		// ── Optional engine-plugin feature subsystems ──────────────────────
		// Each block conditionally links its DLLs only if the engine ships the plugin,
		// and exposes a WITH_ECA_<FEATURE> preprocessor symbol the source files use to
		// #if-gate the corresponding commands. A project that doesn't want these
		// subsystems just doesn't enable the upstream engine plugin — ECABridge will
		// load fine without them, with the relevant commands simply absent.

		// Mutable / Customizable Object (character customization, 13 commands)
		if (EngineHasPlugin("Mutable"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "CustomizableObject", "CustomizableObjectEditor" });
			PublicDefinitions.Add("WITH_ECA_MUTABLE=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_MUTABLE=0");
		}

		// Movie Render Pipeline (high-quality offline rendering, 2 commands)
		if (EngineHasPlugin("MovieRenderPipeline"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MovieRenderPipelineCore", "MovieRenderPipelineEditor", "MovieRenderPipelineRenderPasses" });
			PublicDefinitions.Add("WITH_ECA_MOVIE_RENDER_PIPELINE=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_MOVIE_RENDER_PIPELINE=0");
		}
	}
}
