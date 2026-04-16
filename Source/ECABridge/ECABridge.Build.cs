// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ECABridge : ModuleRules
{
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

			// Mutable / Customizable Object
			"CustomizableObject",
			"CustomizableObjectEditor",

			// Level Sequencer
			"LevelSequence",
			"LevelSequenceEditor",
			"MovieScene",
			"MovieSceneTracks",
			"CinematicCamera"
		});

		// ModelViewViewModelBlueprint include path - UBT doesn't resolve it via PrivateDependencyModuleNames
		// when the plugin is EnabledByDefault=false, so add it explicitly.
		string MVVMBlueprintPath = System.IO.Path.Combine(EngineDirectory, "Plugins", "Runtime", "ModelViewViewModel", "Source", "ModelViewViewModelBlueprint", "Public");
		PrivateIncludePaths.Add(MVVMBlueprintPath);
	}
}
